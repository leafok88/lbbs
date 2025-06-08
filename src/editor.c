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
			(current_data_line_length + p_editor_data->display_line_lengths[i] + 1) >= MAX_EDITOR_DATA_LINE_LENGTH ||
			(p_editor_data->display_line_lengths[i - 1] > 0 && p_data[line_offsets[i - 1] + p_editor_data->display_line_lengths[i - 1] - 1] == '\n'))
		{
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
		}
		else
		{
			p_editor_data->p_display_lines[i] = p_editor_data->p_display_lines[i - 1] + p_editor_data->display_line_lengths[i - 1];
			current_data_line_length = 0;
		}

		memcpy(p_editor_data->p_display_lines[i], p_data + line_offsets[i], (size_t)p_editor_data->display_line_lengths[i]);
		p_editor_data->p_display_lines[i][p_editor_data->display_line_lengths[i]] = '\0';

		current_data_line_length += p_editor_data->display_line_lengths[i];
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
	}

	free(p_editor_data);
}

static int editor_display_key_handler(int *p_key, DISPLAY_CTX *p_ctx)
{
	switch (*p_key)
	{
	case 0: // Set msg
		snprintf(p_ctx->msg, sizeof(p_ctx->msg),
				 "| 返回[\033[32m←\033[33m,\033[32mESC\033[33m] | "
				 "移动[\033[32m↑\033[33m/\033[32m↓\033[33m/\033[32mPgUp\033[33m/\033[32mPgDn\033[33m] | "
				 "帮助[\033[32mh\033[33m] |");
		break;
	}

	return 0;
}

int editor_display(EDITOR_DATA *p_editor_data)
{
	static int show_help = 1;
	char buffer[MAX_EDITOR_DATA_LINE_LENGTH];
	DISPLAY_CTX ctx;
	int ch = 0;
	int input_ok, screen_current_line;
	const int screen_begin_line = 1;
	int screen_end_line = SCREEN_ROWS - 1;
	const int screen_line_total = screen_end_line - screen_begin_line + 1;
	long int line_current = 0;
	long int len;
	long int percentile;
	int loop;
	int eol, display_len;
	long row_pos = 1, col_pos = 1;

	screen_current_line = screen_begin_line;
	clrline(screen_begin_line, SCREEN_ROWS);

	// update msg_ext with extended key handler
	if (editor_display_key_handler(&ch, &ctx) != 0)
	{
		return ch;
	}

	loop = 1;
	while (!SYS_server_exit && loop)
	{
		if (line_current >= p_editor_data->display_line_total || screen_current_line > screen_end_line)
		{
			ctx.reach_begin = (line_current < screen_current_line ? 1 : 0);

			if (line_current - (screen_current_line - screen_begin_line) + screen_line_total < p_editor_data->display_line_total)
			{
				percentile = (line_current - (screen_current_line - screen_begin_line) + screen_line_total) * 100 / p_editor_data->display_line_total;
				ctx.reach_end = 0;
			}
			else
			{
				percentile = 100;
				ctx.reach_end = 1;
			}

			ctx.line_top = line_current - (screen_current_line - screen_begin_line) + 1;
			ctx.line_bottom = MIN(line_current - (screen_current_line - screen_begin_line) + screen_line_total, p_editor_data->display_line_total);

			snprintf(buffer, sizeof(buffer),
					 "\033[1;44;33m第\033[32m%ld\033[33m-\033[32m%ld\033[33m行 (\033[32m%ld%%\033[33m) %s",
					 ctx.line_top,
					 ctx.line_bottom,
					 percentile,
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

				switch (ch)
				{
				case KEY_NULL:
				case KEY_TIMEOUT:
					goto cleanup;
				case Ctrl('C'):
					loop = 0;
					break;
				case Ctrl('H'):
					col_pos = 1;
					break;
				case Ctrl('E'):
					col_pos = MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]);
					break;
				case KEY_HOME:
					row_pos = 1;
					col_pos = 1;
					if (line_current - screen_current_line < 0) // Reach begin
					{
						break;
					}
					line_current = 0;
					screen_current_line = screen_begin_line;
					screen_end_line = SCREEN_ROWS - 1;
					clrline(screen_begin_line, SCREEN_ROWS);
					break;
				case KEY_END:
					if (p_editor_data->display_line_total < screen_line_total)
					{
						row_pos = p_editor_data->display_line_total;
						col_pos = MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]);
						break;
					}
					line_current = p_editor_data->display_line_total - screen_line_total;
					screen_current_line = screen_begin_line;
					screen_end_line = SCREEN_ROWS - 1;
					row_pos = screen_line_total;
					col_pos = MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]);
					clrline(screen_begin_line, SCREEN_ROWS);
					break;
				case KEY_LEFT:
					if (col_pos > 1)
					{
						col_pos--;
						break;
					}
					col_pos = SCREEN_COLS; // continue to KEY_UP
				case KEY_UP:
					if (row_pos > screen_begin_line)
					{
						row_pos--;
						col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]));
						break;
					}
					if (line_current - screen_current_line < 0) // Reach begin
					{
						col_pos = MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]);
						break;
					}
					line_current -= screen_current_line;
					screen_current_line = screen_begin_line;
					// screen_end_line = begin_line;
					// prints("\033[T"); // Scroll down 1 line
					screen_end_line = SCREEN_ROWS - 1; // Legacy Fterm only works with this line
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]));
					break;
				case CR:
					break;
				case KEY_SPACE:
					break;
				case KEY_RIGHT:
					if (col_pos < p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos])
					{
						col_pos++;
						break;
					}
					col_pos = 1; // continue to KEY_DOWN
				case KEY_DOWN:
					if (row_pos < MIN(screen_line_total, p_editor_data->display_line_total))
					{
						row_pos++;
						col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]));
						break;
					}
					if (line_current + (screen_line_total - (screen_current_line - screen_begin_line)) >= p_editor_data->display_line_total) // Reach end
					{
						col_pos = MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]);
						break;
					}
					line_current += (screen_line_total - (screen_current_line - screen_begin_line));
					screen_current_line = screen_line_total;
					screen_end_line = SCREEN_ROWS - 1;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]));
					moveto(SCREEN_ROWS, 0);
					clrtoeol();
					prints("\033[S"); // Scroll up 1 line
					break;
				case KEY_PGUP:
					if (line_current - screen_current_line < 0) // Reach begin
					{
						break;
					}
					line_current -= ((screen_line_total - 1) + (screen_current_line - screen_begin_line));
					if (line_current < 0)
					{
						line_current = 0;
					}
					screen_current_line = screen_begin_line;
					screen_end_line = SCREEN_ROWS - 1;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]));
					clrline(screen_begin_line, SCREEN_ROWS);
					break;
				case KEY_PGDN:
					if (line_current + screen_line_total - (screen_current_line - screen_begin_line) >= p_editor_data->display_line_total) // Reach end
					{
						break;
					}
					line_current += (screen_line_total - 1) - (screen_current_line - screen_begin_line);
					if (line_current + screen_line_total > p_editor_data->display_line_total) // No enough lines to display
					{
						line_current = p_editor_data->display_line_total - screen_line_total;
					}
					screen_current_line = screen_begin_line;
					screen_end_line = SCREEN_ROWS - 1;
					col_pos = MIN(col_pos, MAX(1, p_editor_data->display_line_lengths[line_current - screen_current_line + row_pos]));
					clrline(screen_begin_line, SCREEN_ROWS);
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
					line_current -= (screen_current_line - screen_begin_line);
					screen_current_line = screen_begin_line;
					screen_end_line = SCREEN_ROWS - 1;
					clrline(screen_begin_line, SCREEN_ROWS);
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

		moveto(screen_current_line, 0);
		clrtoeol();
		prints("%s", buffer);
		line_current++;
		screen_current_line++;
	}

cleanup:
	return ch;
}
