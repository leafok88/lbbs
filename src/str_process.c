/***************************************************************************
						  str_process.c  -  description
							 -------------------
	Copyright            : (C) 2004-2025 by Leaflet
	Email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "common.h"
#include "log.h"
#include "str_process.h"
#include <stdio.h>
#include <string.h>

int split_line(const char *buffer, int max_display_len, int *p_eol, int *p_display_len, int skip_ctrl_seq)
{
	int i;
	*p_eol = 0;
	*p_display_len = 0;
	char c;

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

		if (c < 0 || c > 127) // GBK chinese character
		{
			if (*p_display_len + 2 > max_display_len)
			{
				break;
			}
			i++;
			(*p_display_len) += 2;
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
			// log_error("Line count %d reaches limit %d\n", line_cnt + 1, line_offsets_count);
			return line_cnt;
		}

		p_line_offsets[line_cnt + 1] = p_line_offsets[line_cnt] + len;
		line_cnt++;
		p_buf += len;
	} while (p_buf[0] != '\0');

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
