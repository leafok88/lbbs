/***************************************************************************
						  screen.c  -  description
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

#include "screen.h"
#include "bbs.h"
#include "common.h"
#include "str_process.h"
#include "log.h"
#include "io.h"
#include "file_loader.h"
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/shm.h>

#define _POSIX_C_SOURCE 200809L
#include <string.h>

#define ACTIVE_BOARD_HEIGHT 8

#define STR_TOP_LEFT_MAX_LEN 40
#define STR_TOP_MIDDLE_MAX_LEN 20
#define STR_TOP_RIGHT_MAX_LEN 20

void moveto(int row, int col)
{
	if (row >= 0)
	{
		prints("\033[%d;%dH", row, col);
	}
	else
	{
		prints("\r");
	}
}

void clrtoeol()
{
	prints(CTRL_SEQ_CLR_LINE);
}

void clrline(int line_begin, int line_end)
{
	int i;

	for (i = line_begin; i <= line_end; i++)
	{
		moveto(i, 0);
		prints(CTRL_SEQ_CLR_LINE);
	}
}

void clrtobot(int line_begin)
{
	moveto(line_begin, 0);
	prints("\033[J");
	moveto(line_begin, 0);
}

void clearscr()
{
	prints("\033[2J");
	moveto(0, 0);
}

int press_any_key()
{
	moveto(SCREEN_ROWS, 0);
	clrtoeol();

	prints("                           \033[1;33m按任意键继续...\033[0;37m");
	iflush();

	return igetch_t(MIN(MAX_DELAY_TIME, 60));
}

void set_input_echo(int echo)
{
	if (echo)
	{
		outc('\x83'); // ASCII code 131
		iflush();
	}
	else
	{
		//    outc ('\x85'); // ASCII code 133
		prints("\xff\xfb\x01\xff\xfb\x03");
		iflush();
		igetch(0);
		igetch_reset();
	}
}

static int _str_input(char *buffer, int buf_size, int echo_mode)
{
	int c;
	int offset = 0;
	int hz = 0;

	buffer[buf_size - 1] = '\0';
	for (offset = 0; offset < buf_size - 1 && buffer[offset] != '\0'; offset++)
		;

	igetch_reset();

	while (!SYS_server_exit)
	{
		c = igetch_t(MIN(MAX_DELAY_TIME, 60));

		if (c == CR)
		{
			igetch_reset();
			break;
		}
		else if (c == KEY_TIMEOUT || c == KEY_NULL) // timeout or broken pipe
		{
			return -1;
		}
		else if (c == LF || c == '\0')
		{
			continue;
		}
		else if (c == BACKSPACE)
		{
			if (offset > 0)
			{
				offset--;
				if (buffer[offset] < 0 || buffer[offset] > 127)
				{
					prints("\033[D \033[D");
					offset--;
					if (offset < 0) // should not happen
					{
						log_error("Offset of buffer is negative\n");
						offset = 0;
					}
				}
				buffer[offset] = '\0';
				prints("\033[D \033[D");
				iflush();
			}
			continue;
		}
		else if (c > 255 || iscntrl(c))
		{
			continue;
		}
		else if (c > 127 && c <= 255)
		{
			if (!hz && offset + 2 > buf_size - 1) // No enough space for Chinese character
			{
				igetch(0); // Ignore 1 character
				outc('\a');
				iflush();
				continue;
			}
			hz = (!hz);
		}

		if (offset + 1 > buf_size - 1)
		{
			outc('\a');
			iflush();
			continue;
		}

		buffer[offset++] = (char)c;
		buffer[offset] = '\0';

		switch (echo_mode)
		{
		case DOECHO:
			outc((char)c);
			break;
		case NOECHO:
			outc('*');
			break;
		}
		if (!hz)
		{
			iflush();
		}
	}

	return offset;
}

int str_input(char *buffer, int buf_size, int echo_mode)
{
	int len;

	buffer[0] = '\0';

	len = _str_input(buffer, buf_size, echo_mode);

	prints("\r\n");
	iflush();

	return len;
};

int get_data(int row, int col, char *prompt, char *buffer, int buf_size, int echo_mode)
{
	int len;

	moveto(row, col);
	prints(prompt);
	prints(buffer);
	iflush();

	len = _str_input(buffer, buf_size, echo_mode);

	return len;
}

int display_file_ex(const char *filename, int begin_line, int wait)
{
	static int show_help = 1;
	char buffer[LINE_BUFFER_LEN];
	int ch = KEY_NULL;
	int input_ok, line, max_lines;
	long int line_current = 0;
	const void *p_shm;
	size_t data_len;
	long line_total;
	const void *p_data;
	const long *p_line_offsets;
	long int len;
	long int percentile;
	int loop;

	if ((p_shm = get_file_shm_readonly(filename, &data_len, &line_total, &p_data, &p_line_offsets)) == NULL)
	{
		log_error("get_file_shm(%s) error\n", filename);
		return KEY_NULL;
	}

	clrline(begin_line, SCREEN_ROWS);
	line = begin_line;
	max_lines = SCREEN_ROWS - 1;

	loop = 1;
	while (!SYS_server_exit && loop)
	{
		if (line_current >= line_total && line_total <= SCREEN_ROWS - 2)
		{
			if (wait)
			{
				ch = press_any_key();
			}
			else
			{
				iflush();
			}

			loop = 0;
			break;
		}

		if (line_current >= line_total || line >= max_lines)
		{
			if (line_current - (line - 1) + (SCREEN_ROWS - 2) < line_total)
			{
				percentile = (line_current - (line - 1) + (SCREEN_ROWS - 2)) * 100 / line_total;
			}
			else
			{
				percentile = 100;
			}

			moveto(SCREEN_ROWS, 0);
			prints("\033[1;44;32m%s (%d%%)%s\033[33m          │ 结束 ← <q> │ ↑/↓/PgUp/PgDn 移动 │ ? 辅助说明 │     \033[m",
				   (percentile < 100 ? "下面还有喔" : "没有更多了"), percentile,
				   (percentile < 10 ? "  " : (percentile < 100 ? " " : "")));
			iflush();

			input_ok = 0;
			while (!SYS_server_exit && !input_ok)
			{
				ch = igetch_t(MAX_DELAY_TIME);
				input_ok = 1;
				switch (ch)
				{
				case KEY_NULL:
				case KEY_TIMEOUT:
					goto cleanup;
				case KEY_HOME:
					line_current = 0;
					line = begin_line;
					max_lines = SCREEN_ROWS - 1;
					clrline(begin_line, SCREEN_ROWS);
					break;
				case KEY_END:
					line_current = line_total - (SCREEN_ROWS - 2);
					line = begin_line;
					max_lines = SCREEN_ROWS - 1;
					clrline(begin_line, SCREEN_ROWS);
					break;
				case KEY_UP:
					if (line_current - line < 0) // Reach top
					{
						break;
					}
					line_current -= line;
					line = begin_line;
					// max_lines = begin_line + 1;
					// prints("\033[T"); // Scroll down 1 line
					max_lines = SCREEN_ROWS - 1; // Legacy Fterm only works with this line
					break;
				case CR:
					igetch_reset();
				case KEY_DOWN:
					if (line_current + ((SCREEN_ROWS - 2) - (line - 1)) >= line_total) // Reach bottom
					{
						break;
					}
					line_current += ((SCREEN_ROWS - 2) - (line - 1));
					line = SCREEN_ROWS - 2;
					max_lines = SCREEN_ROWS - 1;
					moveto(SCREEN_ROWS, 0);
					clrtoeol();
					prints("\033[S"); // Scroll up 1 line
					break;
				case KEY_PGUP:
				case Ctrl('B'):
					if (line_current - line < 0) // Reach top
					{
						break;
					}
					line_current -= ((SCREEN_ROWS - 3) + (line - 1));
					if (line_current < 0)
					{
						line_current = 0;
					}
					line = begin_line;
					max_lines = SCREEN_ROWS - 1;
					clrline(begin_line, SCREEN_ROWS);
					break;
				case KEY_RIGHT:
				case KEY_PGDN:
				case Ctrl('F'):
				case KEY_SPACE:
					if (line_current + (SCREEN_ROWS - 2) - (line - 1) >= line_total) // Reach bottom
					{
						break;
					}
					line_current += (SCREEN_ROWS - 3) - (line - 1);
					if (line_current + SCREEN_ROWS - 2 > line_total) // No enough lines to display
					{
						line_current = line_total - (SCREEN_ROWS - 2);
					}
					line = begin_line;
					max_lines = SCREEN_ROWS - 1;
					clrline(begin_line, SCREEN_ROWS);
					break;
				case KEY_ESC:
				case KEY_LEFT:
				case 'q':
				case 'Q':
					loop = 0;
					break;
				case '?':
				case 'h':
				case 'H':
					if (!show_help)
					{
						break;
					}

					// Display help information
					show_help = 0;
					display_file_ex(DATA_READ_HELP, begin_line, 1);
					show_help = 1;

					// Refresh after display help information
					line_current -= (line - 1);
					line = begin_line;
					max_lines = SCREEN_ROWS - 1;
					clrline(begin_line, SCREEN_ROWS);
					break;
				default:
					input_ok = 0;
					break;
				}

				BBS_last_access_tm = time(0);
			}

			continue;
		}

		len = p_line_offsets[line_current + 1] - p_line_offsets[line_current];
		if (len >= LINE_BUFFER_LEN)
		{
			log_error("buffer overflow: len=%ld(%ld - %ld) line=%ld \n",
					  len, p_line_offsets[line_current + 1], p_line_offsets[line_current], line_current);
			len = LINE_BUFFER_LEN - 1;
		}

		memcpy(buffer, (const char *)p_data + p_line_offsets[line_current], (size_t)len);
		buffer[len] = '\0';

		moveto(line, 0);
		clrtoeol();
		prints("%s", buffer);
		line_current++;
		line++;
	}

cleanup:
	if (detach_file_shm(p_shm) < 0)
	{
		log_error("detach_file_shm(%s) error\n", filename);
	}

	return ch;
}

int show_top(const char *str_left, const char *str_middle, const char *str_right)
{
	char str_left_f[STR_TOP_LEFT_MAX_LEN + 1];
	char str_middle_f[STR_TOP_MIDDLE_MAX_LEN + 1];
	char str_right_f[STR_TOP_RIGHT_MAX_LEN + 1];
	int str_left_len;
	int str_middle_len;
	int str_right_len;
	int eol;
	int len;

	strncpy(str_left_f, str_left, sizeof(str_left_f) - 1);
	str_left_f[sizeof(str_left_f) - 1] = '\0';
	len = split_line(str_left_f, STR_TOP_LEFT_MAX_LEN, &eol, &str_left_len);
	str_left_f[len] = '\0';

	strncpy(str_middle_f, str_middle, sizeof(str_middle_f) - 1);
	str_middle_f[sizeof(str_middle_f) - 1] = '\0';
	len = split_line(str_middle, STR_TOP_MIDDLE_MAX_LEN, &eol, &str_middle_len);
	str_middle_f[len] = '\0';

	strncpy(str_right_f, str_right, sizeof(str_right_f) - 1);
	str_right_f[sizeof(str_right_f) - 1] = '\0';
	len = split_line(str_right, STR_TOP_RIGHT_MAX_LEN, &eol, &str_right_len);
	str_right_f[len] = '\0';

	moveto(1, 0);
	clrtoeol();
	prints("\033[1;44;33m%s\033[37m%*s\033[33m%*s\033[m",
		   str_left_f, 44 - str_left_len, str_middle_f, 36, str_right_f);

	return 0;
}

int show_bottom(const char *msg)
{
	char str_time[LINE_BUFFER_LEN];
	time_t time_online;
	struct tm *tm_online;
	char msg_f[21];
	int eol;
	int msg_len;
	int len;
	int len_username;

	get_time_str(str_time, sizeof(str_time));

	strncpy(msg_f, msg, sizeof(msg_f) - 1);
	msg_f[sizeof(msg_f) - 1] = '\0';

	len = split_line(msg_f, 20, &eol, &msg_len);
	msg_f[len] = '\0';

	len_username = (int)strnlen(BBS_username, sizeof(BBS_username));

	time_online = time(0) - BBS_login_tm;
	tm_online = gmtime(&time_online);

	moveto(SCREEN_ROWS, 0);
	clrtoeol();
	prints("\033[1;44;33m时间[\033[36m%s\033[33m]%*s%*s帐号[\033[36m%s\033[33m]"
		   "[\033[36m%1d\033[33m天\033[36m%2d\033[33m时\033[36m%2d\033[33m分]\033[m",
		   str_time, 24, msg_f, 12 - len_username, "", BBS_username,
		   tm_online->tm_mday - 1, tm_online->tm_hour, tm_online->tm_min);

	return 0;
}

int show_active_board()
{
	static int line_current = 0;
	static const void *p_shm = NULL;
	static size_t data_len;
	static long line_total;
	static const void *p_data;
	static const long *p_line_offsets;

	static time_t t_last_show = 0;
	static int line_last = 0;

	char buffer[LINE_BUFFER_LEN];
	long int len;

	if (p_shm == NULL)
	{
		if ((p_shm = get_file_shm_readonly(DATA_ACTIVE_BOARD, &data_len, &line_total, &p_data, &p_line_offsets)) == NULL)
		{
			log_error("get_file_shm(%s) error\n", DATA_ACTIVE_BOARD);
			return KEY_NULL;
		}
	}

	if (time(0) - t_last_show >= 10)
	{
		line_last = line_current;
		t_last_show = time(0);
	}
	else
	{
		line_current = line_last;
	}

	clrline(2, 2 + ACTIVE_BOARD_HEIGHT);

	for (int i = 0; i < ACTIVE_BOARD_HEIGHT; i++)
	{
		len = p_line_offsets[line_current + 1] - p_line_offsets[line_current];
		if (len >= LINE_BUFFER_LEN)
		{
			log_error("buffer overflow: len=%ld(%ld - %ld) line=%ld \n",
					  len, p_line_offsets[line_current + 1], p_line_offsets[line_current], line_current);
			len = LINE_BUFFER_LEN - 1;
		}

		memcpy(buffer, (const char *)p_data + p_line_offsets[line_current], (size_t)len);
		buffer[len] = '\0';

		moveto(3 + i, 0);
		prints("%s", buffer);

		line_current++;
		if (line_current >= line_total)
		{
			line_current = 0;
			break;
		}
	}

	return 0;
}
