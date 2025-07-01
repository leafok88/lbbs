/***************************************************************************
						   editor.h  -  description
							 -------------------
	copyright            : (C) 2004-2025 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "bbs.h"
#include "common.h"
#include "editor.h"
#include "io.h"
#include "log.h"
#include "memory_pool.h"
#include "str_process.h"
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#define EDITOR_ESC_DISPLAY_STR "\033[32m*\033[m"
#define EDITOR_MEM_POOL_LINE_PER_CHUNK 1000
#define EDITOR_MEM_POOL_CHUNK_LIMIT (MAX_EDITOR_DATA_LINES / EDITOR_MEM_POOL_LINE_PER_CHUNK + 1)

static MEMORY_POOL *p_mp_data_line;
static MEMORY_POOL *p_mp_editor_data;

int editor_memory_pool_init(void)
{
	if (p_mp_data_line != NULL || p_mp_editor_data != NULL)
	{
		log_error("Editor mem pool already initialized\n");
		return -1;
	}

	p_mp_data_line = memory_pool_init(MAX_EDITOR_DATA_LINE_LENGTH, EDITOR_MEM_POOL_LINE_PER_CHUNK, EDITOR_MEM_POOL_CHUNK_LIMIT);
	if (p_mp_data_line == NULL)
	{
		log_error("Memory pool init error\n");
		return -2;
	}

	p_mp_editor_data = memory_pool_init(sizeof(EDITOR_DATA), 1, 1);
	if (p_mp_data_line == NULL)
	{
		log_error("Memory pool init error\n");
		return -3;
	}

	return 0;
}

void editor_memory_pool_cleanup(void)
{
	if (p_mp_data_line != NULL)
	{
		memory_pool_cleanup(p_mp_data_line);
		p_mp_data_line = NULL;
	}

	if (p_mp_editor_data != NULL)
	{
		memory_pool_cleanup(p_mp_editor_data);
		p_mp_editor_data = NULL;
	}
}

EDITOR_DATA *editor_data_load(const char *p_data)
{
	EDITOR_DATA *p_editor_data;
	char *p_data_line = NULL;
	long line_offsets[MAX_EDITOR_DATA_LINES + 1];
	long current_data_line_length = 0;
	long i;

	if (p_data == NULL)
	{
		log_error("editor_data_load() error: NULL pointer\n");
		return NULL;
	}

	p_editor_data = memory_pool_alloc(p_mp_editor_data);
	if (p_editor_data == NULL)
	{
		log_error("memory_pool_alloc() error\n");
		return NULL;
	}

	p_editor_data->display_line_total = split_data_lines(p_data, SCREEN_COLS, line_offsets, MAX_EDITOR_DATA_LINES + 1,
														 0, p_editor_data->display_line_widths);

	for (i = 0; i < p_editor_data->display_line_total; i++)
	{
		p_editor_data->display_line_lengths[i] = line_offsets[i + 1] - line_offsets[i];

		if (i == 0 ||
			current_data_line_length + p_editor_data->display_line_lengths[i] + 1 > MAX_EDITOR_DATA_LINE_LENGTH ||
			(p_editor_data->display_line_lengths[i - 1] > 0 && p_data[line_offsets[i - 1] + p_editor_data->display_line_lengths[i - 1] - 1] == '\n'))
		{
			// Allocate new data line
			p_data_line = memory_pool_alloc(p_mp_data_line);
			if (p_data_line == NULL)
			{
				log_error("memory_pool_alloc() error: i = %d\n", i);
				// Cleanup
				editor_data_cleanup(p_editor_data);
				return NULL;
			}

			p_editor_data->p_display_lines[i] = p_data_line;
			current_data_line_length = 0;
		}
		else
		{
			p_editor_data->p_display_lines[i] = p_editor_data->p_display_lines[i - 1] + p_editor_data->display_line_lengths[i - 1];
		}

		memcpy(p_editor_data->p_display_lines[i], p_data + line_offsets[i], (size_t)p_editor_data->display_line_lengths[i]);
		current_data_line_length += p_editor_data->display_line_lengths[i];

		// Trim \n from last line
		if (i + 1 == p_editor_data->display_line_total &&
			p_editor_data->display_line_lengths[i] > 0 &&
			p_editor_data->p_display_lines[i][p_editor_data->display_line_lengths[i] - 1] == '\n')
		{
			p_editor_data->display_line_lengths[i]--;
			current_data_line_length--;
		}
		p_data_line[current_data_line_length] = '\0';
	}

	memset(p_editor_data->p_display_lines + p_editor_data->display_line_total, 0, MAX_EDITOR_DATA_LINES - (size_t)p_editor_data->display_line_total);

	return p_editor_data;
}

long editor_data_save(const EDITOR_DATA *p_editor_data, char *p_data, size_t buf_len)
{
	long current_pos = 0;
	long i;

	if (p_editor_data == NULL || p_data == NULL)
	{
		log_error("editor_data_save() error: NULL pointer\n");
		return -1;
	}

	for (i = 0; i < p_editor_data->display_line_total; i++)
	{
		if (current_pos + p_editor_data->display_line_lengths[i] + 1 > buf_len)
		{
			log_error("Data buffer not longer enough %d > %d\n", current_pos + p_editor_data->display_line_lengths[i] + 1, buf_len);
			p_data[current_pos] = '\0';
			return -2;
		}

		memcpy(p_data + current_pos, p_editor_data->p_display_lines[i], (size_t)p_editor_data->display_line_lengths[i]);
		current_pos += p_editor_data->display_line_lengths[i];
	}

	p_data[current_pos] = '\0';

	return current_pos;
}

void editor_data_cleanup(EDITOR_DATA *p_editor_data)
{
	char *p_data_line = NULL;
	long i;

	if (p_editor_data == NULL)
	{
		return;
	}

	for (i = 0; i < p_editor_data->display_line_total; i++)
	{
		if (p_data_line == NULL)
		{
			p_data_line = p_editor_data->p_display_lines[i];
		}

		if (p_editor_data->display_line_lengths[i] > 0 &&
			p_editor_data->p_display_lines[i][p_editor_data->display_line_lengths[i] - 1] == '\n')
		{
			memory_pool_free(p_mp_data_line, p_data_line);
			p_data_line = NULL;
		}
	}

	if (p_data_line != NULL)
	{
		memory_pool_free(p_mp_data_line, p_data_line);
	}

	memory_pool_free(p_mp_editor_data, p_editor_data);
}

int editor_data_insert(EDITOR_DATA *p_editor_data, long *p_display_line, long *p_offset,
					   const char *str, int str_len, long *p_last_updated_line)
{
	long display_line = *p_display_line;
	long offset = *p_offset;
	char *p_data_line = NULL;
	long len_data_line;
	long offset_data_line;
	long last_display_line; // of data line
	long line_offsets[MAX_EDITOR_DATA_LINE_LENGTH + 1];
	int line_widths[MAX_EDITOR_DATA_LINE_LENGTH + 1];
	long split_line_total;
	long i;
	int len;
	int eol;
	int display_len;

	if (p_editor_data == NULL || p_last_updated_line == NULL)
	{
		log_error("editor_data_op() error: NULL pointer\n");
		return -1;
	}

	// Get length of current data line
	len_data_line = 0;
	p_data_line = p_editor_data->p_display_lines[display_line];
	for (i = display_line - 1; i >= 0; i--)
	{
		if (p_editor_data->display_line_lengths[i] > 0 &&
			p_editor_data->p_display_lines[i][p_editor_data->display_line_lengths[i] - 1] == '\n') // reach end of prior data line
		{
			break;
		}

		len_data_line += p_editor_data->display_line_lengths[i];
		p_data_line = p_editor_data->p_display_lines[i];
	}
	offset_data_line = len_data_line + offset;
	last_display_line = p_editor_data->display_line_total - 1;
	for (i = display_line; i < p_editor_data->display_line_total; i++)
	{
		len_data_line += p_editor_data->display_line_lengths[i];

		if (p_editor_data->display_line_lengths[i] > 0 &&
			p_editor_data->p_display_lines[i][p_editor_data->display_line_lengths[i] - 1] == '\n') // reach end of current data line
		{
			last_display_line = i;
			break;
		}
	}

	// Split current data line if over-length
	if (len_data_line + str_len + 2 > MAX_EDITOR_DATA_LINE_LENGTH || str[0] == CR)
	{
		if (p_editor_data->display_line_total >= MAX_EDITOR_DATA_LINES)
		{
#ifdef _DEBUG
			log_error("Split line error, display_line_total(%ld) reach limit(%d)\n",
					  p_editor_data->display_line_total, MAX_EDITOR_DATA_LINES);
#endif

			return -2;
		}

		// Allocate new data line
		p_data_line = memory_pool_alloc(p_mp_data_line);
		if (p_data_line == NULL)
		{
			log_error("memory_pool_alloc() error\n");
			return -2;
		}

		if (offset_data_line + str_len + 2 >= MAX_EDITOR_DATA_LINE_LENGTH || str[0] == CR)
		{
			if (str[0] == CR)
			{
				str_len = 0;
			}

			// Copy str to new data line
			memcpy(p_data_line, str, (size_t)str_len);

			// Copy rest part of current data line to new data line
			memcpy(p_data_line + str_len,
				   p_editor_data->p_display_lines[display_line] + offset,
				   (size_t)(len_data_line - offset_data_line));

			p_data_line[str_len + len_data_line - offset_data_line] = '\0';

			// Add line ending to current display line (data line)
			p_editor_data->p_display_lines[display_line][offset] = '\n';
			p_editor_data->p_display_lines[display_line][offset + 1] = '\0';
			p_editor_data->display_line_lengths[display_line] = offset + 1;

			*p_display_line = display_line + 1;
			*p_offset = str_len;
		}
		else
		{
			// Copy rest part of current data line to new data line
			memcpy(p_data_line,
				   p_editor_data->p_display_lines[display_line] + offset,
				   (size_t)(len_data_line - offset_data_line));

			p_data_line[len_data_line - offset_data_line] = '\0';

			// Append str to current display line
			memcpy(p_editor_data->p_display_lines[display_line] + offset, str, (size_t)str_len);

			// Add line ending to current display line (data line)
			p_editor_data->p_display_lines[display_line][offset + str_len] = '\n';
			p_editor_data->p_display_lines[display_line][offset + str_len + 1] = '\0';
			p_editor_data->display_line_lengths[display_line] = offset + str_len + 1;

			*p_display_line = display_line;
			*p_offset = offset + str_len;
		}

		// Update display width of current display line
		len = split_line(p_editor_data->p_display_lines[display_line], SCREEN_COLS, &eol, &display_len, 0);
		p_editor_data->display_line_widths[display_line] = display_len;

		split_line_total = last_display_line - display_line + 3;

		// Set start display_line for spliting new data line
		display_line++;

		*p_last_updated_line = p_editor_data->display_line_total;
	}
	else // insert str into current data line at offset_data_line
	{
		memmove(p_data_line + offset_data_line + str_len, p_data_line + offset_data_line, (size_t)(len_data_line - offset_data_line));
		memcpy(p_data_line + offset_data_line, str, (size_t)str_len);
		p_data_line[len_data_line + str_len] = '\0';

		// Set p_data_line to head of current display line
		p_data_line = p_editor_data->p_display_lines[display_line];
		split_line_total = last_display_line - display_line + 3;

		*p_display_line = display_line;
		*p_offset = offset + str_len;
	}

	// Split current data line since beginning of current display line
	split_line_total = split_data_lines(p_data_line, SCREEN_COLS, line_offsets, split_line_total, 0, line_widths);

	for (i = 0; i < split_line_total; i++)
	{
		if (display_line + i > last_display_line)
		{
			// Insert blank display line after last_display_line
			if (p_editor_data->display_line_total >= MAX_EDITOR_DATA_LINES)
			{
#ifdef _DEBUG
				log_error("display_line_total over limit %d >= %d\n", p_editor_data->display_line_total, MAX_EDITOR_DATA_LINES);
#endif

				// Terminate prior display line with \n, to avoid error on cleanup
				if (display_line + i - 1 >= 0 && p_editor_data->display_line_lengths[display_line + i - 1] > 0)
				{
					len = split_line(p_editor_data->p_display_lines[display_line + i - 1], SCREEN_COLS - 1, &eol, &display_len, 0);
					p_editor_data->p_display_lines[display_line + i - 1][len] = '\n';
					p_editor_data->p_display_lines[display_line + i - 1][len + 1] = '\0';
					p_editor_data->display_line_lengths[display_line + i - 1] = len + 1;
					p_editor_data->display_line_widths[display_line + i - 1] = display_len;
				}
				if (*p_offset >= p_editor_data->display_line_lengths[*p_display_line])
				{
					*p_offset = p_editor_data->display_line_lengths[*p_display_line] - 1;
				}
				break;
			}

			// for (j = p_editor_data->display_line_total; j > last_display_line + 1; j--)
			// {
			// 	p_editor_data->p_display_lines[j] = p_editor_data->p_display_lines[j - 1];
			// 	p_editor_data->display_line_lengths[j] = p_editor_data->display_line_lengths[j - 1];
			// 	p_editor_data->display_line_widths[j] = p_editor_data->display_line_widths[j - 1];
			// }
			memmove(p_editor_data->p_display_lines + last_display_line + 2,
					p_editor_data->p_display_lines + last_display_line + 1,
					(size_t)(p_editor_data->display_line_total - last_display_line - 1) *
						sizeof(p_editor_data->p_display_lines[last_display_line + 1]));
			memmove(p_editor_data->display_line_lengths + last_display_line + 2,
					p_editor_data->display_line_lengths + last_display_line + 1,
					(size_t)(p_editor_data->display_line_total - last_display_line - 1) *
						sizeof(p_editor_data->display_line_lengths[last_display_line + 1]));
			memmove(p_editor_data->display_line_widths + last_display_line + 2,
					p_editor_data->display_line_widths + last_display_line + 1,
					(size_t)(p_editor_data->display_line_total - last_display_line - 1) *
						sizeof(p_editor_data->display_line_widths[last_display_line + 1]));

			last_display_line++;
			*p_last_updated_line = p_editor_data->display_line_total;
			(p_editor_data->display_line_total)++;
		}

		p_editor_data->display_line_lengths[display_line + i] = line_offsets[i + 1] - line_offsets[i];
		p_editor_data->display_line_widths[display_line + i] = line_widths[i];
		p_editor_data->p_display_lines[display_line + i] =
			(i == 0
				 ? p_data_line
				 : (p_editor_data->p_display_lines[display_line + i - 1] + p_editor_data->display_line_lengths[display_line + i - 1]));

		if (p_editor_data->display_line_lengths[display_line + i] > 0 &&
			p_editor_data->p_display_lines[display_line + i][p_editor_data->display_line_lengths[display_line + i] - 1] == '\n')
		{
			break;
		}
	}

	*p_last_updated_line = MAX(display_line + MIN(i, split_line_total - 1), *p_last_updated_line);

	if (*p_offset >= p_editor_data->display_line_lengths[*p_display_line])
	{
		if (*p_display_line + 1 < p_editor_data->display_line_total)
		{
			*p_offset -= p_editor_data->display_line_lengths[*p_display_line];
			(*p_display_line)++;
		}
	}

	// Prevent the last display line from being over-length
	if (p_editor_data->display_line_total == MAX_EDITOR_DATA_LINES)
	{
		len = split_line(p_editor_data->p_display_lines[p_editor_data->display_line_total - 1], SCREEN_COLS - 1, &eol, &display_len, 0);
		p_editor_data->p_display_lines[p_editor_data->display_line_total - 1][len] = '\0';
		p_editor_data->display_line_lengths[p_editor_data->display_line_total - 1] = len;
		p_editor_data->display_line_widths[p_editor_data->display_line_total - 1] = display_len;
		if (*p_display_line + 1 >= p_editor_data->display_line_total)
		{
			*p_offset = MIN(*p_offset, len);
			*p_display_line = p_editor_data->display_line_total - 1;
		}
	}

	return 0;
}

int editor_data_delete(EDITOR_DATA *p_editor_data, long *p_display_line, long *p_offset,
					   long *p_last_updated_line)
{
	long display_line = *p_display_line;
	long offset = *p_offset;
	char *p_data_line = NULL;
	long len_data_line;
	long offset_data_line;
	long last_display_line; // of data line
	long line_offsets[MAX_EDITOR_DATA_LINE_LENGTH + 1];
	int line_widths[MAX_EDITOR_DATA_LINE_LENGTH + 1];
	long split_line_total;
	long i, j;
	int str_len = 0;
	char c;

	if (p_editor_data == NULL || p_last_updated_line == NULL)
	{
		log_error("editor_data_op() error: NULL pointer\n");
		return -1;
	}

	// Get length of current data line
	len_data_line = 0;
	p_data_line = p_editor_data->p_display_lines[display_line];
	for (i = display_line - 1; i >= 0; i--)
	{
		if (p_editor_data->display_line_lengths[i] > 0 &&
			p_editor_data->p_display_lines[i][p_editor_data->display_line_lengths[i] - 1] == '\n') // reach end of prior data line
		{
			break;
		}

		len_data_line += p_editor_data->display_line_lengths[i];
		p_data_line = p_editor_data->p_display_lines[i];
	}
	offset_data_line = len_data_line + offset;
	last_display_line = p_editor_data->display_line_total - 1;
	for (i = display_line; i < p_editor_data->display_line_total; i++)
	{
		len_data_line += p_editor_data->display_line_lengths[i];

		if (p_editor_data->display_line_lengths[i] > 0 &&
			p_editor_data->p_display_lines[i][p_editor_data->display_line_lengths[i] - 1] == '\n') // reach end of current data line
		{
			last_display_line = i;
			break;
		}
	}

	if (offset_data_line >= len_data_line) // end-of-line
	{
		return 0;
	}

	// Check str to be deleted
	if (p_data_line[offset_data_line] > 0 && p_data_line[offset_data_line] < 127)
	{
		str_len = 1;
	}
	else if ((p_data_line[offset_data_line] & 0b10000000) == 0b10000000) // head of multi-byte character
	{
		str_len = 1;
		c = (p_data_line[offset_data_line] & 0b01111000) << 1;
		while (c & 0b10000000)
		{
			str_len++;
			c = (c & 0b01111111) << 1;
		}
	}
	else
	{
		log_error("Some strange character at display_line %ld, offset %ld: %d %d\n",
				  display_line, offset, p_data_line[offset_data_line], p_data_line[offset_data_line + 1]);
		str_len = 1;
	}

	// Current display line is (almost) empty
	if (offset_data_line + str_len > len_data_line ||
		(offset_data_line + str_len == len_data_line && p_data_line[offset_data_line] == '\n'))
	{
		if (display_line + 1 >= p_editor_data->display_line_total) // No additional display line (data line)
		{
			return 0;
		}

		len_data_line = 0; // Next data line
		last_display_line = p_editor_data->display_line_total - 1;
		for (i = display_line + 1; i < p_editor_data->display_line_total; i++)
		{
			len_data_line += p_editor_data->display_line_lengths[i];

			if (p_editor_data->display_line_lengths[i] > 0 &&
				p_editor_data->p_display_lines[i][p_editor_data->display_line_lengths[i] - 1] == '\n') // reach end of current data line
			{
				last_display_line = i;
				break;
			}
		}

		if (offset_data_line + len_data_line + 1 > MAX_EDITOR_DATA_LINE_LENGTH) // No enough buffer to merge current data line with next data line
		{
			return 0;
		}

		// Append next data line to current one
		memcpy(p_data_line + offset_data_line, p_editor_data->p_display_lines[display_line + 1], (size_t)len_data_line);
		p_data_line[offset_data_line + len_data_line] = '\0';

		// Recycle next data line
		memory_pool_free(p_mp_data_line, p_editor_data->p_display_lines[display_line + 1]);
	}
	else
	{
		memmove(p_data_line + offset_data_line, p_data_line + offset_data_line + str_len, (size_t)(len_data_line - offset_data_line - str_len));
		p_data_line[len_data_line - str_len] = '\0';
		len_data_line -= str_len;
	}

	// Set p_data_line to head of current display line
	p_data_line = p_editor_data->p_display_lines[display_line];
	split_line_total = last_display_line - display_line + 2;

	// Split current data line since beginning of current display line
	split_line_total = split_data_lines(p_data_line, SCREEN_COLS, line_offsets, split_line_total, 0, line_widths);

	for (i = 0; i < split_line_total; i++)
	{
		p_editor_data->display_line_lengths[display_line + i] = line_offsets[i + 1] - line_offsets[i];
		p_editor_data->display_line_widths[display_line + i] = line_widths[i];
		p_editor_data->p_display_lines[display_line + i] =
			(i == 0
				 ? p_data_line
				 : (p_editor_data->p_display_lines[display_line + i - 1] + p_editor_data->display_line_lengths[display_line + i - 1]));

		if (p_editor_data->display_line_lengths[display_line + i] > 0 &&
			p_editor_data->p_display_lines[display_line + i][p_editor_data->display_line_lengths[display_line + i] - 1] == '\n')
		{
			break;
		}
	}

	*p_last_updated_line = display_line + MIN(i, split_line_total - 1);

	if (*p_last_updated_line < last_display_line)
	{
		// Remove redundant display line after last_display_line
		// for (j = last_display_line + 1; j < p_editor_data->display_line_total; j++)
		// {
		// 	p_editor_data->p_display_lines[j - (last_display_line - *p_last_updated_line)] = p_editor_data->p_display_lines[j];
		// 	p_editor_data->display_line_lengths[j - (last_display_line - *p_last_updated_line)] = p_editor_data->display_line_lengths[j];
		// 	p_editor_data->display_line_widths[j - (last_display_line - *p_last_updated_line)] = p_editor_data->display_line_widths[j];
		// }
		memmove(p_editor_data->p_display_lines + *p_last_updated_line + 1,
				p_editor_data->p_display_lines + last_display_line + 1,
				(size_t)(p_editor_data->display_line_total - last_display_line - 1) *
					sizeof(p_editor_data->p_display_lines[last_display_line + 1]));
		memmove(p_editor_data->display_line_lengths + *p_last_updated_line + 1,
				p_editor_data->display_line_lengths + last_display_line + 1,
				(size_t)(p_editor_data->display_line_total - last_display_line - 1) *
					sizeof(p_editor_data->display_line_lengths[last_display_line + 1]));
		memmove(p_editor_data->display_line_widths + *p_last_updated_line + 1,
				p_editor_data->display_line_widths + last_display_line + 1,
				(size_t)(p_editor_data->display_line_total - last_display_line - 1) *
					sizeof(p_editor_data->display_line_widths[last_display_line + 1]));

		j = p_editor_data->display_line_total;
		(p_editor_data->display_line_total) -= (last_display_line - *p_last_updated_line);
		*p_last_updated_line = MAX(j - 1, *p_last_updated_line);
	}

	// Return real offset
	*p_offset = offset;

	return str_len;
}

static int editor_display_key_handler(int *p_key, EDITOR_CTX *p_ctx)
{
	switch (*p_key)
	{
	case 0: // Set msg
		snprintf(p_ctx->msg, sizeof(p_ctx->msg),
				 "| 退出[\033[32mCtrl-W\033[33m] |");
		break;
	case KEY_CSI:
		*p_key = KEY_ESC;
		break;
	}

	return 0;
}

int editor_display(EDITOR_DATA *p_editor_data)
{
	static int show_help = 1;
	char buffer[MAX_EDITOR_DATA_LINE_LENGTH];
	EDITOR_CTX ctx;
	int ch = 0;
	char input_str[4];
	char c;
	int str_len = 0;
	int input_ok;
	const int screen_begin_row = 1;
	const int screen_row_total = SCREEN_ROWS - screen_begin_row;
	int output_current_row = screen_begin_row;
	int output_end_row = SCREEN_ROWS - 1;
	long int line_current = 0;
	long int len;
	int loop;
	int eol, display_len;
	long row_pos = 1, col_pos = 1;
	long display_line_in, offset_in;
	long display_line_out, offset_out;
	int scroll_rows;
	long last_updated_line = 0;
	int key_insert = 1;
	int i, j;
	char *p_str;

	clrline(output_current_row, SCREEN_ROWS);

	// update msg_ext with extended key handler
	if (editor_display_key_handler(&ch, &ctx) != 0)
	{
		return ch;
	}

	for (loop = 1; !SYS_server_exit && loop;)
	{
		if (line_current >= p_editor_data->display_line_total || output_current_row > output_end_row)
		{
			ctx.line_cursor = line_current - output_current_row + row_pos + 1;

			snprintf(buffer, sizeof(buffer),
					 "\033[1;44;33m[\033[32m%ld\033[33m;\033[32m%ld\033[33m] "
					 "第\033[32m%ld\033[33m/\033[32m%ld\033[33m行 [\033[32m%s\033[33m] "
					 "%s",
					 row_pos, col_pos,
					 ctx.line_cursor, p_editor_data->display_line_total,
					 key_insert ? "插入" : "替换",
					 ctx.msg);

			len = split_line(buffer, SCREEN_COLS, &eol, &display_len, 1);
			for (; display_len < SCREEN_COLS; display_len++)
			{
				buffer[len++] = ' ';
			}
			buffer[len] = '\0';
			strncat(buffer, "\033[m", sizeof(buffer) - 1 - strnlen(buffer, sizeof(buffer)));

			moveto(SCREEN_ROWS, 0);
			prints("%s", buffer);

			moveto((int)row_pos, (int)col_pos);
			iflush();

			str_len = 0;
			ch = igetch_t(MAX_DELAY_TIME);
			while (!SYS_server_exit)
			{
				// extended key handler
				if (editor_display_key_handler(&ch, &ctx) != 0)
				{
					goto cleanup;
				}

				if ((ch & 0xff80) == 0x80) // head of multi-byte character
				{
					str_len = 0;
					c = (char)(ch & 0b11111000);
					while (c & 0b10000000)
					{
						input_str[str_len] = (char)(ch - 256);
						str_len++;
						c = (c & 0b01111111) << 1;

						if ((c & 0b10000000) == 0) // Input completed
						{
							break;
						}

						// Expect additional bytes of input
						ch = igetch(100);						 // 0.1 second
						if (ch == KEY_NULL || ch == KEY_TIMEOUT) // Ignore received bytes if no futher input
						{
							log_error("Ignore %d bytes of incomplete UTF8 character\n", str_len);
							str_len = 0;
							break;
						}
					}
				}

				if ((ch >= 32 && ch < 127) || str_len >= 2 || // Printable character or multi-byte character
					ch == CR || ch == KEY_ESC)				  // Special character
				{
					BBS_last_access_tm = time(NULL);

					if (str_len == 0) // ch >= 32 && ch < 127
					{
						input_str[0] = (char)ch;
						str_len = 1;
					}

					display_line_in = line_current - output_current_row + row_pos;
					offset_in = split_line(p_editor_data->p_display_lines[display_line_in], (int)col_pos - 1, &eol, &display_len, 0);
					display_line_out = display_line_in;
					offset_out = offset_in;

					last_updated_line = display_line_in;

					if (!key_insert) // overwrite
					{
						if (editor_data_delete(p_editor_data, &display_line_out, &offset_out,
											   &last_updated_line) < 0)
						{
							log_error("editor_data_delete() error\n");
						}
					}

					if (editor_data_insert(p_editor_data, &display_line_out, &offset_out,
										   input_str, str_len, &last_updated_line) < 0)
					{
						log_error("editor_data_insert(str_len=%d) error\n", str_len);
					}
					else
					{
						output_end_row = MIN(SCREEN_ROWS - 1, output_current_row + (int)(last_updated_line - line_current));
						line_current -= (output_current_row - row_pos);
						output_current_row = (int)row_pos;

						scroll_rows = MAX(0, (int)(display_line_out - display_line_in) - (output_end_row - output_current_row));

						if (scroll_rows > 0)
						{
							moveto(SCREEN_ROWS, 0);
							clrtoeol();
							for (i = 0; i < scroll_rows; i++)
							{
								// prints("\033[S"); // Scroll up 1 line
								prints("\n"); // Legacy Cterm only works with this line
							}

							output_current_row -= scroll_rows;
							if (output_current_row < screen_begin_row)
							{
								line_current += (screen_begin_row - output_current_row);
								output_current_row = screen_begin_row;
							}
							row_pos = output_end_row;
						}
						else // if (scroll_lines == 0)
						{
							row_pos += (display_line_out - display_line_in);
						}

						if (offset_out != offset_in)
						{
							if (display_line_out != display_line_in)
							{
								col_pos = 1;
							}
							if (ch != CR)
							{
								col_pos += (str_len == 1 ? 1 : 2);
							}
						}
					}

					if (display_line_out != display_line_in) // Output on line change
					{
						break;
					}

					ch = igetch(0);
					if (ch == KEY_NULL || ch == KEY_TIMEOUT) // Output if no futher input
					{
						break;
					}

					str_len = 0;
					continue;
				}
				else if (ch == KEY_DEL || ch == BACKSPACE) // Del
				{
					BBS_last_access_tm = time(NULL);

					if (ch == BACKSPACE)
					{
						if (line_current - output_current_row + row_pos <= 0 && col_pos <= 1) // Forbidden
						{
							break; // force output prior operation result if any
						}

						offset_in = split_line(p_editor_data->p_display_lines[line_current - output_current_row + row_pos],
											   (int)col_pos - 1, &eol, &display_len, 0);
						if (offset_in >= 1 && p_editor_data->p_display_lines[line_current - output_current_row + row_pos][offset_in - 1] < 0) // UTF8
						{
							col_pos = display_len - 1;
						}
						else
						{
							col_pos = display_len;
						}

						if (col_pos < 1 && line_current - output_current_row + row_pos >= 0)
						{
							row_pos--;
							col_pos = MAX(1, p_editor_data->display_line_widths[line_current - output_current_row + row_pos]);
						}
					}

					display_line_in = line_current - output_current_row + row_pos;
					offset_in = split_line(p_editor_data->p_display_lines[display_line_in], (int)col_pos - 1, &eol, &display_len, 0);
					display_line_out = display_line_in;
					offset_out = offset_in;

					if ((str_len = editor_data_delete(p_editor_data, &display_line_out, &offset_out,
													  &last_updated_line)) < 0)
					{
						log_error("editor_data_delete() error\n");
					}
					else
					{
						col_pos = display_len + 1; // Set col_pos to accurate pos

						output_end_row = MIN(SCREEN_ROWS - 1, output_current_row + (int)(last_updated_line - line_current));
						line_current -= (output_current_row - row_pos);
						output_current_row = (int)row_pos;

						if (output_current_row < screen_begin_row) // row_pos <= 0
						{
							output_current_row = screen_begin_row;
							row_pos = screen_begin_row;
							output_end_row = SCREEN_ROWS - 1;
						}

						// Exceed end
						if (line_current + (screen_row_total - output_current_row) >= p_editor_data->display_line_total &&
							p_editor_data->display_line_total > screen_row_total)
						{
							scroll_rows = (int)((line_current - (output_current_row - screen_begin_row)) -
												(p_editor_data->display_line_total - screen_row_total));

							line_current = p_editor_data->display_line_total - screen_row_total;
							row_pos += scroll_rows;
							output_current_row = screen_begin_row;
							output_end_row = SCREEN_ROWS - 1;
						}

						clrline(output_current_row, output_end_row);
					}

					if (display_line_out != display_line_in) // Output on line change
					{
						break;
					}

					ch = igetch(0);
					if (ch == KEY_NULL || ch == KEY_TIMEOUT) // Output if no futher input
					{
						break;
					}

					str_len = 0;
					continue;
				}

				input_ok = 1;
				switch (ch)
				{
				case KEY_NULL:
				case KEY_TIMEOUT:
					goto cleanup;
				case Ctrl('W'):
					loop = 0;
					break;
				case Ctrl('S'): // Start of line
				case KEY_CTRL_LEFT:
					col_pos = 1;
					break;
				case Ctrl('E'): // End of line
				case KEY_CTRL_RIGHT:
					if (line_current - output_current_row + row_pos == p_editor_data->display_line_total - 1) // row_pos at end line
					{
						// last display line does NOT have \n in the end
						col_pos = p_editor_data->display_line_widths[line_current - output_current_row + row_pos] + 1;
						break;
					}
					col_pos = MAX(1, p_editor_data->display_line_widths[line_current - output_current_row + row_pos]);
					break;
				case Ctrl('T'): // Top of screen
				case KEY_CTRL_UP:
					row_pos = screen_begin_row;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_widths[line_current - output_current_row + row_pos]));
					break;
				case Ctrl('B'): // Bottom of screen
				case KEY_CTRL_DOWN:
					if (p_editor_data->display_line_total < screen_row_total)
					{
						row_pos = p_editor_data->display_line_total;
					}
					else
					{
						row_pos = SCREEN_ROWS - 1;
					}
					if (line_current + (screen_row_total - (output_current_row - screen_begin_row)) >= p_editor_data->display_line_total) // Reach end
					{
						// last display line does NOT have \n in the end
						col_pos = MIN(col_pos, p_editor_data->display_line_widths[line_current - output_current_row + row_pos] + 1);
					}
					else
					{
						col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_widths[line_current - output_current_row + row_pos]));
					}
					break;
				case KEY_INS:
					key_insert = !key_insert;
					break;
				case KEY_HOME:
					row_pos = 1;
					col_pos = 1;
					if (line_current - output_current_row < 0) // Reach begin
					{
						break;
					}
					line_current = 0;
					output_current_row = screen_begin_row;
					output_end_row = SCREEN_ROWS - 1;
					clrline(output_current_row, SCREEN_ROWS);
					break;
				case KEY_END:
					if (p_editor_data->display_line_total < screen_row_total)
					{
						row_pos = p_editor_data->display_line_total;
						col_pos = p_editor_data->display_line_widths[line_current - output_current_row + row_pos] + 1;
						break;
					}
					line_current = p_editor_data->display_line_total - screen_row_total;
					output_current_row = screen_begin_row;
					output_end_row = SCREEN_ROWS - 1;
					row_pos = SCREEN_ROWS - 1;
					col_pos = p_editor_data->display_line_widths[line_current - output_current_row + row_pos] + 1;
					clrline(output_current_row, SCREEN_ROWS);
					break;
				case KEY_LEFT:
					offset_in = split_line(p_editor_data->p_display_lines[line_current - output_current_row + row_pos],
										   (int)col_pos - 1, &eol, &display_len, 0);
					if (offset_in >= 1 && p_editor_data->p_display_lines[line_current - output_current_row + row_pos][offset_in - 1] < 0) // UTF8
					{
						col_pos = display_len - 1;
					}
					else
					{
						col_pos = display_len;
					}
					if (col_pos >= 1)
					{
						break;
					}
					col_pos = SCREEN_COLS; // continue to KEY_UP
				case KEY_UP:
					if (row_pos > screen_begin_row)
					{
						row_pos--;
						col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_widths[line_current - output_current_row + row_pos]));
						break;
					}
					if (line_current - output_current_row < 0) // Reach begin
					{
						col_pos = 1;
						break;
					}
					line_current -= output_current_row;
					output_current_row = screen_begin_row;
					// screen_end_line = begin_line;
					// prints("\033[T"); // Scroll down 1 line
					output_end_row = SCREEN_ROWS - 1; // Legacy Fterm only works with this line
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_widths[line_current - output_current_row + row_pos]));
					break;
				case KEY_SPACE:
					break;
				case KEY_RIGHT:
					offset_in = split_line(p_editor_data->p_display_lines[line_current - output_current_row + row_pos],
										   (int)col_pos - 1, &eol, &display_len, 0);
					if (offset_in < p_editor_data->display_line_lengths[line_current - output_current_row + row_pos] &&
						p_editor_data->p_display_lines[line_current - output_current_row + row_pos][offset_in] < 0) // UTF8
					{
						col_pos = display_len + 3;
					}
					else
					{
						col_pos = display_len + 2;
					}
					if (col_pos <= p_editor_data->display_line_widths[line_current - output_current_row + row_pos])
					{
						break;
					}
					col_pos = 1; // continue to KEY_DOWN
				case KEY_DOWN:
					if (row_pos < MIN(screen_row_total, p_editor_data->display_line_total))
					{
						row_pos++;
						col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_widths[line_current - output_current_row + row_pos]));
						break;
					}
					if (line_current - output_current_row + row_pos == p_editor_data->display_line_total - 1) // row_pos at end line
					{
						// last display line does NOT have \n in the end
						col_pos = p_editor_data->display_line_widths[line_current - output_current_row + row_pos] + 1;
						break;
					}
					line_current += (screen_row_total - (output_current_row - screen_begin_row));
					output_current_row = screen_row_total;
					output_end_row = SCREEN_ROWS - 1;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_widths[line_current - output_current_row + row_pos]));
					moveto(SCREEN_ROWS, 0);
					clrtoeol();
					// prints("\033[S"); // Scroll up 1 line
					prints("\n"); // Legacy Cterm only works with this line
					break;
				case KEY_PGUP:
					if (line_current - output_current_row < 0) // Reach begin
					{
						break;
					}
					line_current -= ((screen_row_total - 1) + (output_current_row - screen_begin_row));
					if (line_current < 0)
					{
						line_current = 0;
					}
					output_current_row = screen_begin_row;
					output_end_row = SCREEN_ROWS - 1;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_widths[line_current - output_current_row + row_pos]));
					clrline(output_current_row, SCREEN_ROWS);
					break;
				case KEY_PGDN:
					if (line_current + screen_row_total - (output_current_row - screen_begin_row) >= p_editor_data->display_line_total) // Reach end
					{
						break;
					}
					line_current += (screen_row_total - 1) - (output_current_row - screen_begin_row);
					if (line_current + screen_row_total > p_editor_data->display_line_total) // No enough lines to display
					{
						line_current = p_editor_data->display_line_total - screen_row_total;
					}
					output_current_row = screen_begin_row;
					output_end_row = SCREEN_ROWS - 1;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_widths[line_current - output_current_row + row_pos]));
					clrline(output_current_row, SCREEN_ROWS);
					break;
				case KEY_F1:
					if (!show_help) // Not reentrant
					{
						break;
					}
					// Display help information
					show_help = 0;
					display_file(DATA_READ_HELP, 1);
					show_help = 1;
				case KEY_F5:
					// Refresh after display help information
					line_current -= (output_current_row - screen_begin_row);
					output_current_row = screen_begin_row;
					output_end_row = SCREEN_ROWS - 1;
					clrline(output_current_row, SCREEN_ROWS);
					break;
				case 0: // Refresh bottom line
					break;
				default:
					input_ok = 0;
					break;
				}

				BBS_last_access_tm = time(NULL);

				if (input_ok)
				{
					break;
				}

				ch = igetch_t(MAX_DELAY_TIME);
			}

			continue;
		}

		len = p_editor_data->display_line_lengths[line_current];
		if (len >= sizeof(buffer))
		{
			log_error("Buffer overflow: len=%ld line=%ld \n", len, line_current);
			len = sizeof(buffer) - 1;
		}
		else if (len < 0)
		{
			log_error("Incorrect line offsets: len=%ld line=%ld \n", len, line_current);
			len = 0;
		}

		// memcpy(buffer, p_editor_data->p_display_lines[line_current], (size_t)len);
		// Replace '\033' with '*'
		p_str = p_editor_data->p_display_lines[line_current];
		for (i = 0, j = 0; i < len; i++)
		{
			if (p_str[i] == '\033')
			{
				memcpy(buffer + j, EDITOR_ESC_DISPLAY_STR, sizeof(EDITOR_ESC_DISPLAY_STR) - 1);
				j += (int)(sizeof(EDITOR_ESC_DISPLAY_STR) - 1);
			}
			else
			{
				buffer[j] = p_str[i];
				j++;
			}
		}
		buffer[j] = '\0';

		moveto(output_current_row, 0);
		clrtoeol();
		prints("%s", buffer);
		line_current++;
		output_current_row++;
	}

cleanup:
	return ch;
}
