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
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "str_process.h"
#include "common.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

unsigned int split_line(const char *buffer, int max_len, int *p_eol)
{
	size_t len = strnlen(buffer, LINE_BUFFER_LEN);
	int display_len = 0;
	unsigned int i = 0;
	*p_eol = 0;

	if (len == 0)
	{
		return 0;
	}

	for (; i < len; i++)
	{
		char c = buffer[i];

		if (c == '\r' || c == '\7') // skip
		{
			continue;
		}

		if (c == '\n')
		{
			i++;
			*p_eol = 1;
			break;
		}

		if (c == '\033' && buffer[i + 1] == '[') // Skip control sequence
		{
			i += 2;
			while (i < len && buffer[i] != 'm')
			{
				i++;
			}
			continue;
		}

		if (c > 127 && c <= 255) // GBK chinese character
		{
			if (display_len + 2 > max_len)
			{
				*p_eol = 1;
				break;
			}
			i++;
			display_len += 2;
		}
		else
		{
			if (display_len >= max_len)
			{
				*p_eol = 1;
				break;
			}
			display_len++;
		}
	}

	return i;
}

unsigned int split_file_lines(FILE *fin, int max_len, long *p_line_offsets, int max_line_cnt)
{
	char buffer[LINE_BUFFER_LEN];
	char *p_buf = buffer;
	unsigned int line_cnt = 0;
	unsigned int len = 0;
	int end_of_line = 0;
	p_line_offsets[line_cnt] = 0L;

	while (fgets(p_buf, (int)(sizeof(buffer) - len), fin))
	{
		p_buf = buffer;
		while (1)
		{
			len = split_line(p_buf, max_len, &end_of_line);

			if (len == 0 || !end_of_line)
			{
				break;
			}

			// Exceed max_line_cnt
			if (line_cnt + 1 >= max_line_cnt)
			{
				log_error("File line count %d reaches limit\n", line_cnt + 1);
				return line_cnt;
			}

			p_line_offsets[line_cnt + 1] = p_line_offsets[line_cnt] + len;
			line_cnt++;
			p_buf += len;
		}

		// Move p_buf[0 .. len - 1] to head of buffer
		for (int i = 0; i < len; i++)
		{
			buffer[i] = p_buf[i];
		}
		p_buf = buffer + len;
	}

	if (len > 0 && line_cnt + 1 < max_line_cnt)
	{
		p_line_offsets[line_cnt + 1] = p_line_offsets[line_cnt] + len;
		line_cnt++;
	}

	return line_cnt;
}
