/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * str_process
 *   - common string processing features with UTF-8 support
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "log.h"
#include "str_process.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int UTF8_fixed_width = 1;

int str_length(const char *str, int skip_ctrl_seq)
{
	int str_len;
	char input_str[5];
	wchar_t wcs[2];
	int wc_len;
	int i;
	char c;
	int ret = 0;

	for (i = 0; str[i] != '\0'; i++)
	{
		c = str[i];

		if (c == '\r' || c == '\7') // skip
		{
			continue;
		}

		if (skip_ctrl_seq && c == '\033' && str[i + 1] == '[') // Skip control sequence
		{
			for (i = i + 2; isdigit((int)str[i]) || str[i] == ';' || str[i] == '?'; i++)
				;

			if (str[i] == 'm') // valid
			{
				// skip
			}
			else if (isalpha((int)str[i]))
			{
				// unsupported ANSI CSI command
			}
			else
			{
				i--;
			}

			continue;
		}

		// Process UTF-8 Chinese characters
		if (c & 0x80) // head of multi-byte character
		{
			str_len = 0;
			c = (char)(c & 0xf0);
			while (c & 0x80)
			{
				input_str[str_len] = str[i + str_len];
				str_len++;
				c = (c & 0x7f) << 1;
			}
			input_str[str_len] = '\0';

			if (mbstowcs(wcs, input_str, 1) == (size_t)-1)
			{
				log_debug("mbstowcs(%s) error\n", input_str);
				wc_len = (UTF8_fixed_width ? 2 : 1); // Fallback
			}
			else
			{
				wc_len = (UTF8_fixed_width ? 2 : wcwidth(wcs[0]));
			}

			i += (str_len - 1);
			ret += wc_len;
		}
		else
		{
			ret++;
		}
	}

	return ret;
}

int split_line(const char *buffer, int max_display_len, int *p_eol, int *p_display_len, int skip_ctrl_seq)
{
	int i;
	*p_eol = 0;
	*p_display_len = 0;
	char c;
	int str_len;
	char input_str[5];
	wchar_t wcs[2];
	int wc_len;

	for (i = 0; buffer[i] != '\0'; i++)
	{
		c = buffer[i];

		if (c == '\r' || c == '\7') // skip
		{
			continue;
		}

		if (skip_ctrl_seq && c == '\033' && buffer[i + 1] == '[') // Skip control sequence
		{
			i += 2;
			while (buffer[i] != '\0' && buffer[i] != 'm')
			{
				i++;
			}
			continue;
		}

		if (c & 0x80) // head of multi-byte character
		{
			str_len = 0;
			c = (char)(c & 0xf0);
			while (c & 0x80)
			{
				input_str[str_len] = buffer[i + str_len];
				str_len++;
				c = (c & 0x7f) << 1;
			}
			input_str[str_len] = '\0';

			if (mbstowcs(wcs, input_str, 1) == (size_t)-1)
			{
				log_debug("mbstowcs(%s) error\n", input_str);
				wc_len = (UTF8_fixed_width ? 2 : 1); // Fallback
			}
			else
			{
				wc_len = (UTF8_fixed_width ? 2 : wcwidth(wcs[0]));
			}

			if (*p_display_len + wc_len > max_display_len)
			{
				break;
			}

			i += (str_len - 1);
			(*p_display_len) += wc_len;
		}
		else
		{
			if (*p_display_len + 1 > max_display_len)
			{
				break;
			}
			(*p_display_len)++;

			// \n is regarded as 1 character wide in terminal editor, which is different from Web version
			if (c == '\n')
			{
				i++;
				*p_eol = 1;
				break;
			}
		}
	}

	return i;
}

long split_data_lines(const char *p_buf, int max_display_len, long *p_line_offsets, long line_offsets_count,
					  int skip_ctrl_seq, int *p_line_widths)
{
	int line_cnt = 0;
	int len;
	int end_of_line = 0;
	int display_len = 0;

	p_line_offsets[line_cnt] = 0L;

	do
	{
		len = split_line(p_buf, max_display_len, &end_of_line, &display_len, skip_ctrl_seq);

		if (p_line_widths)
		{
			p_line_widths[line_cnt] = display_len;
		}

		// Exceed max_line_cnt
		if (line_cnt + 1 >= line_offsets_count)
		{
			log_debug("Line count %d reaches limit %d\n", line_cnt + 1, line_offsets_count);
			return line_cnt;
		}

		p_line_offsets[line_cnt + 1] = p_line_offsets[line_cnt] + len;
		line_cnt++;
		p_buf += len;
	} while (p_buf[0] != '\0' || end_of_line);

	return line_cnt;
}

int str_filter(char *buffer, int skip_ctrl_seq)
{
	int i;
	int j;

	for (i = 0, j = 0; buffer[i] != '\0'; i++)
	{
		if (buffer[i] == '\r' || buffer[i] == '\7') // skip
		{
			continue;
		}

		if (skip_ctrl_seq && buffer[i] == '\033' && buffer[i + 1] == '[') // Skip control sequence
		{
			i += 2;
			while (buffer[i] != '\0' && buffer[i] != 'm')
			{
				i++;
			}
			continue;
		}

		buffer[j] = buffer[i];
		j++;
	}

	buffer[j] = '\0';

	return j;
}
