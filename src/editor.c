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

#include "editor.h"
#include "bbs.h"
#include "io.h"
#include "log.h"
#include "common.h"
#include "str_process.h"
#include <stdlib.h>
#include <sys/param.h>

#define _POSIX_C_SOURCE 200809L
#include <string.h>

EDITOR_DATA *editor_data_load(const char *p_data)
{
	EDITOR_DATA *p_editor_data;
	long line_offsets[MAX_EDITOR_DATA_LINES];
	long current_data_line_length = 0;
	long i, j;

	if (p_data == NULL)
	{
		log_error("editor_data_load() error: NULL pointer\n");
		return NULL;
	}

	p_editor_data = malloc(sizeof(EDITOR_DATA));
	if (p_editor_data == NULL)
	{
		log_error("malloc(EDITOR_DATA) error: OOM\n");
		return NULL;
	}

	p_editor_data->data_line_total = 0;
	p_editor_data->display_line_total = split_data_lines(p_data, SCREEN_COLS, line_offsets, MAX_EDITOR_DATA_LINES);

	for (i = 0; i < p_editor_data->display_line_total; i++)
	{
		p_editor_data->display_line_lengths[i] = line_offsets[i + 1] - line_offsets[i];

		if (i == 0 ||
			current_data_line_length + p_editor_data->display_line_lengths[i] + 1 > MAX_EDITOR_DATA_LINE_LENGTH ||
			(p_editor_data->display_line_lengths[i - 1] > 0 && p_data[line_offsets[i - 1] + p_editor_data->display_line_lengths[i - 1] - 1] == '\n'))
		{
			if (p_editor_data->data_line_total >= MAX_EDITOR_DATA_LINES)
			{
				log_error("Append line error, data_line_total(%ld) reach limit(%d)\n", p_editor_data->data_line_total, MAX_EDITOR_DATA_LINES);
				return NULL;
			}

			// Allocate new data line
			p_editor_data->p_data_lines[p_editor_data->data_line_total] = malloc(MAX_EDITOR_DATA_LINE_LENGTH);
			if (p_editor_data->p_data_lines[p_editor_data->data_line_total] == NULL)
			{
				log_error("malloc(MAX_EDITOR_DATA_LINE_LENGTH * %d) error: OOM\n", i);
				// Cleanup
				for (j = p_editor_data->data_line_total - 1; j >= 0; j--)
				{
					free(p_editor_data->p_data_lines[j]);
				}
				free(p_editor_data);
				return NULL;
			}

			p_editor_data->p_display_lines[i] = p_editor_data->p_data_lines[p_editor_data->data_line_total];
			(p_editor_data->data_line_total)++;
			current_data_line_length = 0;
		}
		else
		{
			p_editor_data->p_display_lines[i] = p_editor_data->p_display_lines[i - 1] + p_editor_data->display_line_lengths[i - 1];
		}

		memcpy(p_editor_data->p_display_lines[i], p_data + line_offsets[i], (size_t)p_editor_data->display_line_lengths[i]);
		current_data_line_length += p_editor_data->display_line_lengths[i];
		p_editor_data->p_data_lines[p_editor_data->data_line_total - 1][current_data_line_length] = '\0';
	}

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
	long i;

	if (p_editor_data == NULL)
	{
		return;
	}

	for (i = p_editor_data->data_line_total - 1; i >= 0; i--)
	{
		free(p_editor_data->p_data_lines[i]);
		p_editor_data->p_data_lines[i] = NULL;
	}

	free(p_editor_data);
}

int editor_data_insert(EDITOR_DATA *p_editor_data, long *p_display_line, long *p_offset,
					   const char *str, int str_len, long *p_last_updated_line)
{
	long display_line = *p_display_line;
	long offset = *p_offset;
	int done = 0;
	int len;
	int display_len;
	int eol;
	char *p_data_line;
	long len_data_line;
	long offset_data_line;
	long last_display_line; // of data line
	char buf_insert[MAX_EDITOR_DATA_LINE_LENGTH];
	long len_insert;
	int display_len_insert;
	char buf_catenate[MAX_EDITOR_DATA_LINE_LENGTH];
	long len_catenate;
	long i;

	if (p_editor_data == NULL || p_last_updated_line == NULL)
	{
		log_error("editor_data_op() error: NULL pointer\n");
		return -1;
	}

	memcpy(buf_insert, str, (size_t)str_len);
	buf_insert[str_len] = '\0';
	len_insert = str_len;

	// Get accurate offset of first character of CJK at offset position
	for (i = 0; i < offset; i++)
	{
		if (p_editor_data->p_display_lines[display_line][i] < 0) // GBK
		{
			i++;
		}
	}
	if (i > offset) // offset was skipped
	{
		offset--;
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
	if (len_data_line + str_len + 1 > MAX_EDITOR_DATA_LINE_LENGTH)
	{
		if (p_editor_data->display_line_total >= MAX_EDITOR_DATA_LINES || p_editor_data->data_line_total >= MAX_EDITOR_DATA_LINES)
		{
			log_error("Split line error, display_line_total(%ld) or data_line_total(%ld) reach limit(%d)\n",
					  p_editor_data->display_line_total, p_editor_data->data_line_total, MAX_EDITOR_DATA_LINES);
			return -2;
		}

		// Allocate new data line
		p_editor_data->p_data_lines[p_editor_data->data_line_total] = malloc(MAX_EDITOR_DATA_LINE_LENGTH);
		if (p_editor_data->p_data_lines[p_editor_data->data_line_total] == NULL)
		{
			log_error("malloc(MAX_EDITOR_DATA_LINE_LENGTH) error: OOM\n");
			return -2;
		}

		if (last_display_line > display_line)
		{
			// Copy rest part of current data line (since next display line) to new data line
			memcpy(p_editor_data->p_data_lines[p_editor_data->data_line_total],
				   p_editor_data->p_display_lines[display_line + 1],
				   (size_t)(len_data_line - (p_editor_data->p_display_lines[display_line + 1] - p_data_line)));
			p_editor_data->p_data_lines[p_editor_data->data_line_total]
									   [len_data_line - (p_editor_data->p_display_lines[display_line + 1] - p_data_line)] = '\0';

			// Relocate rest display lines (since next one) of current data line
			p_data_line = p_editor_data->p_display_lines[display_line + 1];
			for (i = display_line + 1; i <= last_display_line; i++)
			{
				p_editor_data->p_display_lines[i] =
					p_editor_data->p_data_lines[p_editor_data->data_line_total] +
					(p_editor_data->p_display_lines[i] - p_data_line);
			}
		}
		else // last_display_line == display_line
		{
			// Insert blank display line pointing to new data line
			for (i = p_editor_data->display_line_total; i > display_line + 1; i--)
			{
				p_editor_data->p_display_lines[i] = p_editor_data->p_display_lines[i - 1];
				p_editor_data->display_line_lengths[i] = p_editor_data->display_line_lengths[i - 1];
			}
			p_editor_data->p_display_lines[display_line + 1] = p_editor_data->p_data_lines[p_editor_data->data_line_total];
			p_editor_data->display_line_lengths[display_line + 1] = 0;

			(p_editor_data->display_line_total)++;
			last_display_line++;
		}

		*p_last_updated_line = p_editor_data->display_line_total;
		(p_editor_data->data_line_total)++;

		if (offset_data_line + str_len + 2 < MAX_EDITOR_DATA_LINE_LENGTH)
		{
			// Copy rest part of current display line to insert buffer
			memcpy(buf_insert,
				   p_editor_data->p_display_lines[display_line] + offset,
				   (size_t)(p_editor_data->display_line_lengths[display_line] - offset));
			len_insert = (p_editor_data->display_line_lengths[display_line] - offset);
			buf_insert[len_insert] = '\0';

			// Append str to current display line
			memcpy(p_editor_data->p_display_lines[display_line] + offset, str, (size_t)str_len);

			// Add line ending to current display line (data line)
			p_editor_data->p_display_lines[display_line][offset + str_len] = '\n';
			p_editor_data->p_display_lines[display_line][offset + str_len + 1] = '\0';
			p_editor_data->display_line_lengths[display_line] = offset + str_len + 1;

			if (!done)
			{
				*p_display_line = display_line;
				*p_offset = offset + str_len;
				done = 1;
			}
		}
		else
		{
			// Append rest part of current display line to insert buffer
			memcpy(buf_insert + len_insert,
				   p_editor_data->p_display_lines[display_line] + offset,
				   (size_t)(p_editor_data->display_line_lengths[display_line] - offset));
			len_insert += (p_editor_data->display_line_lengths[display_line] - offset);
			buf_insert[len_insert] = '\0';

			// Add line ending to current display line (data line)
			p_editor_data->p_display_lines[display_line][offset] = '\n';
			p_editor_data->p_display_lines[display_line][offset + 1] = '\0';
			p_editor_data->display_line_lengths[display_line] = offset + 1;
		}

		display_line++;
		offset = 0;
	}

	for (i = display_line; len_insert > 0 && i <= last_display_line; i++)
	{
		len = split_line(buf_insert, SCREEN_COLS, &eol, &display_len_insert);
		if (len != len_insert)
		{
			log_error("buf_insert is truncated at display_line(%ld): len(%d) != len_insert(%d), buf_insert: %s\n",
					  i, len, len_insert, buf_insert);
			return -3;
		}

		memcpy(buf_catenate, p_editor_data->p_display_lines[i], (size_t)p_editor_data->display_line_lengths[i]);
		buf_catenate[p_editor_data->display_line_lengths[i]] = '\0';

		len = split_line(buf_catenate, SCREEN_COLS - display_len_insert, &eol, &display_len);
		if (len < offset) // offset out of current display line
		{
			offset -= len;
			continue;
		}

		// move \n to next display line if current line is full
		if (len > 0 && buf_catenate[len - 1] == '\n' && display_len + display_len_insert >= SCREEN_COLS)
		{
			len--;
		}

		memcpy(buf_catenate, p_editor_data->p_display_lines[i], (size_t)offset);
		memcpy(buf_catenate + offset, buf_insert, (size_t)len_insert);
		memcpy(buf_catenate + offset + len_insert, p_editor_data->p_display_lines[i] + offset, (size_t)(len - offset));
		len_catenate = len_insert + len;
		buf_catenate[len_catenate] = '\0';

		len_insert = p_editor_data->display_line_lengths[i] - len;
		if (len_insert > 0)
		{
			memcpy(buf_insert, p_editor_data->p_display_lines[i] + len, (size_t)len_insert);
			buf_insert[len_insert] = '\0';
		}

		memcpy(p_editor_data->p_display_lines[i], buf_catenate, (size_t)len_catenate);
		p_editor_data->display_line_lengths[i] = len_catenate;

		if (!done)
		{
			*p_display_line = i;
			*p_offset = offset + str_len;
			done = 1;
		}

		offset = 0;
	}

	*p_last_updated_line = MAX(i, *p_last_updated_line);

	if (len_insert > 0)
	{
		if (p_editor_data->display_line_total >= MAX_EDITOR_DATA_LINES)
		{
			log_error("Append line error, display_line_total(%ld) reach limit(%d)\n",
					  p_editor_data->display_line_total, MAX_EDITOR_DATA_LINES);
			return -2;
		}

		// Prepare one blank display line after last_display_line
		for (i = p_editor_data->display_line_total; i > last_display_line + 1; i--)
		{
			p_editor_data->p_display_lines[i] = p_editor_data->p_display_lines[i - 1];
			p_editor_data->display_line_lengths[i] = p_editor_data->display_line_lengths[i - 1];
		}
		p_editor_data->p_display_lines[last_display_line + 1] =
			p_editor_data->p_display_lines[last_display_line] + p_editor_data->display_line_lengths[last_display_line];
		p_editor_data->display_line_lengths[last_display_line + 1] = 0;

		(p_editor_data->display_line_total)++;
		last_display_line++;

		// Fill data into blank display line
		memcpy(p_editor_data->p_display_lines[last_display_line], buf_insert, (size_t)len_insert);
		p_editor_data->p_display_lines[last_display_line][len_insert] = '\0';
		p_editor_data->display_line_lengths[last_display_line] = len_insert;

		if (!done)
		{
			*p_display_line = last_display_line;
			*p_offset = str_len;
			done = 1;
		}

		*p_last_updated_line = MAX(last_display_line, *p_last_updated_line);
	}

	if (done && *p_offset >= SCREEN_COLS)
	{
		(*p_display_line)++;
		*p_offset = 0;

		if (*p_display_line >= p_editor_data->display_line_total)
		{
			log_error("*p_display_line(%d) >= display_line_total(%d)\n", *p_display_line, p_editor_data->display_line_total);
		}
	}

	return done;
}

int editor_data_delete(EDITOR_DATA *p_editor_data, long display_line, long offset,
					   long *p_last_updated_line)
{
	return 0;
}

static int editor_display_key_handler(int *p_key, EDITOR_CTX *p_ctx)
{
	switch (*p_key)
	{
	case 0: // Set msg
		snprintf(p_ctx->msg, sizeof(p_ctx->msg),
				 "| ÍË³ö[\033[32mCtrl-C\033[33m] | °ïÖú[\033[32mh\033[33m] |");
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
	char insert_str[4];
	int str_len = 0;
	int input_ok;
	int screen_current_row;
	const int screen_begin_row = 1;
	int screen_end_row = SCREEN_ROWS - 1;
	const int screen_row_total = screen_end_row - screen_begin_row + 1;
	long int line_current = 0;
	long int len;
	int loop;
	int eol, display_len;
	long row_pos = 1, col_pos = 1;
	long display_line_in, offset_in;
	long display_line_out, offset_out;
	int scroll_rows;
	long last_updated_line = 0;
	int insert = 1;
	int i;

	screen_current_row = screen_begin_row;
	clrline(screen_begin_row, SCREEN_ROWS);

	// update msg_ext with extended key handler
	if (editor_display_key_handler(&ch, &ctx) != 0)
	{
		return ch;
	}

	loop = 1;
	while (!SYS_server_exit && loop)
	{
		if (line_current >= p_editor_data->display_line_total || screen_current_row > screen_end_row)
		{
			ctx.line_cursor = line_current - screen_current_row + row_pos + 1;

			snprintf(buffer, sizeof(buffer),
					 "\033[1;44;33m[\033[32m%ld\033[33m;\033[32m%ld\033[33m] "
					 "µÚ\033[32m%ld\033[33m/\033[32m%ld\033[33mÐÐ "
					 "%s",
					 row_pos, col_pos,
					 ctx.line_cursor, p_editor_data->display_line_total,
					 ctx.msg);

			len = split_line(buffer, SCREEN_COLS, &eol, &display_len);
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

			input_ok = 0;
			while (!SYS_server_exit && !input_ok)
			{
				ch = igetch_t(MAX_DELAY_TIME);
				input_ok = 1;

				// extended key handler
				if (editor_display_key_handler(&ch, &ctx) != 0)
				{
					goto cleanup;
				}

				if (ch > 127 && ch <= 255) // GBK
				{
					insert_str[str_len] = (char)(ch - 256);
					str_len++;
				}
				else
				{
					str_len = 0;
				}

				if ((ch >= 32 && ch < 127) || (ch > 127 && ch <= 255 && str_len == 2)) // printable character or GBK
				{
					if (str_len == 0)
					{
						insert_str[0] = (char)ch;
						str_len = 1;
					}

					last_updated_line = line_current;
					display_line_in = line_current - screen_current_row + row_pos;
					offset_in = col_pos - 1;
					display_line_out = display_line_in;
					offset_out = offset_in;

					if (editor_data_insert(p_editor_data, &display_line_out, &offset_out,
										   insert_str, str_len, &last_updated_line) < 0)
					{
						log_error("editor_data_insert(%s) error\n", insert_str);
						str_len = 0;
					}
					else
					{
						str_len = 0;

						screen_end_row = MIN(SCREEN_ROWS - 1, screen_current_row + (int)(last_updated_line - line_current));
						line_current -= (screen_current_row - row_pos);
						screen_current_row = (int)row_pos;

						scroll_rows = MAX(0, (int)(display_line_out - display_line_in) - (screen_end_row - screen_current_row));

						if (scroll_rows > 0)
						{
							moveto(SCREEN_ROWS, 0);
							clrtoeol();
							for (i = 0; i < scroll_rows; i++)
							{
								prints("\033[S"); // Scroll up 1 line
							}

							screen_current_row -= scroll_rows;
							if (screen_current_row < screen_begin_row)
							{
								line_current += (screen_begin_row - screen_current_row);
								screen_current_row = screen_begin_row;
							}
							row_pos = screen_end_row;
						}
						else // if (scroll_lines == 0)
						{
							row_pos += (display_line_out - display_line_in);
						}
						col_pos = offset_out + 1;

						continue;
					}
				}
				else if (ch == KEY_DEL) // Del
				{
					last_updated_line = line_current;

					if (editor_data_delete(p_editor_data, line_current - screen_current_row + row_pos, col_pos - 1,
										   &last_updated_line) < 0)
					{
						log_error("editor_data_delete() error\n");
					}
					else
					{
						screen_end_row = MIN(SCREEN_ROWS - 1, screen_current_row + (int)(last_updated_line - line_current));
					}

					continue;
				}

				switch (ch)
				{
				case KEY_NULL:
				case KEY_TIMEOUT:
					goto cleanup;
				case Ctrl('C'):
					loop = 0;
					break;
				case KEY_CTRL_LEFT:
					col_pos = 1;
					break;
				case KEY_CTRL_RIGHT:
					col_pos = MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]);
					break;
				case KEY_CTRL_UP:
					row_pos = screen_begin_row;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]));
					break;
				case KEY_CTRL_DOWN:
					row_pos = SCREEN_ROWS - 1;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]));
					break;
				case KEY_INS:
					insert = !insert;
					break;
				case KEY_HOME:
					row_pos = 1;
					col_pos = 1;
					if (line_current - screen_current_row < 0) // Reach begin
					{
						break;
					}
					line_current = 0;
					screen_current_row = screen_begin_row;
					screen_end_row = SCREEN_ROWS - 1;
					clrline(screen_begin_row, SCREEN_ROWS);
					break;
				case KEY_END:
					if (p_editor_data->display_line_total < screen_row_total)
					{
						row_pos = p_editor_data->display_line_total;
						col_pos = MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]);
						break;
					}
					line_current = p_editor_data->display_line_total - screen_row_total;
					screen_current_row = screen_begin_row;
					screen_end_row = SCREEN_ROWS - 1;
					row_pos = screen_row_total;
					col_pos = MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]);
					clrline(screen_begin_row, SCREEN_ROWS);
					break;
				case KEY_LEFT:
					if (col_pos > 1)
					{
						col_pos--;
						break;
					}
					col_pos = SCREEN_COLS; // continue to KEY_UP
				case KEY_UP:
					if (row_pos > screen_begin_row)
					{
						row_pos--;
						col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]));
						break;
					}
					if (line_current - screen_current_row < 0) // Reach begin
					{
						col_pos = 1;
						break;
					}
					line_current -= screen_current_row;
					screen_current_row = screen_begin_row;
					// screen_end_line = begin_line;
					// prints("\033[T"); // Scroll down 1 line
					screen_end_row = SCREEN_ROWS - 1; // Legacy Fterm only works with this line
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]));
					break;
				case CR:
					break;
				case KEY_SPACE:
					break;
				case KEY_RIGHT:
					if (col_pos < p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos])
					{
						col_pos++;
						break;
					}
					col_pos = 1; // continue to KEY_DOWN
				case KEY_DOWN:
					if (row_pos < MIN(screen_row_total, p_editor_data->display_line_total))
					{
						row_pos++;
						col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]));
						break;
					}
					if (line_current + (screen_row_total - (screen_current_row - screen_begin_row)) >= p_editor_data->display_line_total) // Reach end
					{
						col_pos = MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]);
						break;
					}
					line_current += (screen_row_total - (screen_current_row - screen_begin_row));
					screen_current_row = screen_row_total;
					screen_end_row = SCREEN_ROWS - 1;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]));
					moveto(SCREEN_ROWS, 0);
					clrtoeol();
					prints("\033[S"); // Scroll up 1 line
					break;
				case KEY_PGUP:
					if (line_current - screen_current_row < 0) // Reach begin
					{
						break;
					}
					line_current -= ((screen_row_total - 1) + (screen_current_row - screen_begin_row));
					if (line_current < 0)
					{
						line_current = 0;
					}
					screen_current_row = screen_begin_row;
					screen_end_row = SCREEN_ROWS - 1;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]));
					clrline(screen_begin_row, SCREEN_ROWS);
					break;
				case KEY_PGDN:
					if (line_current + screen_row_total - (screen_current_row - screen_begin_row) >= p_editor_data->display_line_total) // Reach end
					{
						break;
					}
					line_current += (screen_row_total - 1) - (screen_current_row - screen_begin_row);
					if (line_current + screen_row_total > p_editor_data->display_line_total) // No enough lines to display
					{
						line_current = p_editor_data->display_line_total - screen_row_total;
					}
					screen_current_row = screen_begin_row;
					screen_end_row = SCREEN_ROWS - 1;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_row + row_pos]));
					clrline(screen_begin_row, SCREEN_ROWS);
					break;
				case KEY_ESC:
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
					line_current -= (screen_current_row - screen_begin_row);
					screen_current_row = screen_begin_row;
					screen_end_row = SCREEN_ROWS - 1;
					clrline(screen_begin_row, SCREEN_ROWS);
					break;
				case 0: // Refresh bottom line
					break;
				default:
					input_ok = 0;
					break;
				}

				BBS_last_access_tm = time(0);
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

		memcpy(buffer, (const char *)p_editor_data->p_display_lines[line_current], (size_t)len);
		buffer[len] = '\0';

		moveto(screen_current_row, 0);
		clrtoeol();
		prints("%s", buffer);
		line_current++;
		screen_current_row++;
	}

cleanup:
	return ch;
}
