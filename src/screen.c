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

#include "bbs.h"
#include "common.h"
#include "editor.h"
#include "file_loader.h"
#include "io.h"
#include "log.h"
#include "login.h"
#include "screen.h"
#include "str_process.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/types.h>

#define ACTIVE_BOARD_HEIGHT 8

#define STR_TOP_LEFT_MAX_LEN 80
#define STR_TOP_MIDDLE_MAX_LEN 40
#define STR_TOP_RIGHT_MAX_LEN 40

static const char *get_time_str(char *s, size_t len)
{
	static const char *weekday[] = {
		"天", "一", "二", "三", "四", "五", "六"};
	time_t curtime;
	struct tm local_tm;

	time(&curtime);
	localtime_r(&curtime, &local_tm);
	size_t j = strftime(s, len, "%b %d %H:%M 星期", &local_tm);

	if (j == 0 || j + strlen(weekday[local_tm.tm_wday]) + 1 > len)
	{
		return NULL;
	}

	strncat(s, weekday[local_tm.tm_wday], len - 1 - j);

	return s;
}

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

static int _str_input(char *buffer, int buf_size, int max_display_len, int echo_mode)
{
	int ch;
	int offset = 0;
	int eol;
	int display_len;
	char input_str[4];
	int str_len = 0;
	char c;

	buffer[buf_size - 1] = '\0';
	offset = split_line(buffer, max_display_len, &eol, &display_len, 0);

	igetch_reset();

	while (!SYS_server_exit)
	{
		ch = igetch_t(MIN(MAX_DELAY_TIME, 60));

		if (ch == CR)
		{
			igetch_reset();
			break;
		}
		else if (ch == KEY_TIMEOUT || ch == KEY_NULL) // timeout or broken pipe
		{
			return -1;
		}
		else if (ch == LF || ch == '\0')
		{
			continue;
		}
		else if (ch == BACKSPACE)
		{
			if (offset > 0)
			{
				offset--;
				if (buffer[offset] < 0 || buffer[offset] > 127) // UTF8
				{
					while (offset > 0 && (buffer[offset] & 0b11000000) != 0b11000000)
					{
						offset--;
					}
					display_len--;
					prints("\033[D \033[D");
				}
				buffer[offset] = '\0';
				display_len--;
				prints("\033[D \033[D");
				iflush();
			}
			continue;
		}
		else if (ch > 255 || iscntrl(ch))
		{
			continue;
		}
		else if ((ch & 0xff80) == 0x80) // head of multi-byte character
		{
			str_len = 0;
			c = (char)(ch & 0b11110000);
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
#ifdef _DEBUG
					log_error("Ignore %d bytes of incomplete UTF8 character\n", str_len);
#endif
					str_len = 0;
					break;
				}
			}

			if (str_len == 0) // Incomplete input
			{
				continue;
			}

			if (offset + str_len > buf_size - 1 || display_len + 2 > max_display_len) // No enough space for Chinese character
			{
				outc('\a');
				iflush();
				continue;
			}

			memcpy(buffer + offset, input_str, (size_t)str_len);
			offset += str_len;
			buffer[offset] = '\0';
			display_len += 2;

			switch (echo_mode)
			{
			case DOECHO:
				prints(input_str);
				break;
			case NOECHO:
				prints("**");
				break;
			}
		}
		else if (ch >= 32 && ch < 127) // Printable character
		{
			if (offset + 1 > buf_size - 1 || display_len + 1 > max_display_len)
			{
				outc('\a');
				iflush();
				continue;
			}

			buffer[offset++] = (char)ch;
			buffer[offset] = '\0';
			display_len++;

			switch (echo_mode)
			{
			case DOECHO:
				outc((char)ch);
				break;
			case NOECHO:
				outc('*');
				break;
			}
		}
		else // Invalid character
		{
			continue;
		}

		iflush();
	}

	return offset;
}

int str_input(char *buffer, int buf_size, int echo_mode)
{
	int len;

	buffer[0] = '\0';

	len = _str_input(buffer, buf_size, buf_size, echo_mode);

	prints("\r\n");
	iflush();

	return len;
};

int get_data(int row, int col, char *prompt, char *buffer, int buf_size, int max_display_len, int echo_mode)
{
	int len;

	moveto(row, col);
	prints("%s", prompt);
	prints("%s", buffer);
	iflush();

	len = _str_input(buffer, buf_size, max_display_len, echo_mode);

	return len;
}

int display_data(const void *p_data, long display_line_total, const long *p_line_offsets, int eof_exit,
				 display_data_key_handler key_handler, const char *help_filename)
{
	static int show_help = 1;
	char buffer[LINE_BUFFER_LEN];
	DISPLAY_CTX ctx;
	int ch = 0;
	int input_ok;
	const int screen_begin_row = 1;
	const int screen_row_total = SCREEN_ROWS - screen_begin_row;
	int output_current_row = screen_begin_row;
	int output_end_row = SCREEN_ROWS - 1;
	long int line_current = 0;
	long int len;
	long int percentile;
	int loop;
	int eol, display_len;

	clrline(output_current_row, SCREEN_ROWS);

	// update msg_ext with extended key handler
	if (key_handler(&ch, &ctx) != 0)
	{
		return ch;
	}

	loop = 1;
	while (!SYS_server_exit && loop)
	{
		if (eof_exit > 0 && line_current >= display_line_total && display_line_total <= screen_row_total)
		{
			if (eof_exit == 1)
			{
				ch = press_any_key();
			}
			else // if (eof_exit == 2)
			{
				iflush();
			}

			loop = 0;
			break;
		}

		if (line_current >= display_line_total || output_current_row > output_end_row)
		{
			ctx.reach_begin = (line_current < output_current_row ? 1 : 0);

			if (line_current - (output_current_row - screen_begin_row) + screen_row_total < display_line_total)
			{
				percentile = (line_current - (output_current_row - screen_begin_row) + screen_row_total) * 100 / display_line_total;
				ctx.reach_end = 0;
			}
			else
			{
				percentile = 100;
				ctx.reach_end = 1;
			}

			ctx.line_top = line_current - (output_current_row - screen_begin_row) + 1;
			ctx.line_bottom = MIN(line_current - (output_current_row - screen_begin_row) + screen_row_total, display_line_total);

			snprintf(buffer, sizeof(buffer),
					 "\033[1;44;33m第\033[32m%ld\033[33m-\033[32m%ld\033[33m行 (\033[32m%ld%%\033[33m) %s",
					 ctx.line_top,
					 ctx.line_bottom,
					 percentile,
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
			iflush();

			input_ok = 0;
			while (!SYS_server_exit && !input_ok)
			{
				ch = igetch_t(MAX_DELAY_TIME);
				input_ok = 1;

				// extended key handler
				if (key_handler(&ch, &ctx) != 0)
				{
					goto cleanup;
				}

				switch (ch)
				{
				case KEY_NULL:
				case KEY_TIMEOUT:
					goto cleanup;
				case KEY_HOME:
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
					if (display_line_total < screen_row_total)
					{
						break;
					}
					line_current = display_line_total - screen_row_total;
					output_current_row = screen_begin_row;
					output_end_row = SCREEN_ROWS - 1;
					clrline(output_current_row, SCREEN_ROWS);
					break;
				case KEY_UP:
					if (line_current - output_current_row < 0) // Reach begin
					{
						break;
					}
					line_current -= output_current_row;
					output_current_row = screen_begin_row;
					// screen_end_line = screen_begin_line;
					// prints("\033[T"); // Scroll down 1 line
					output_end_row = SCREEN_ROWS - 1; // Legacy Fterm only works with this line
					break;
				case CR:
					igetch_reset();
				case KEY_SPACE:
				case KEY_DOWN:
					if (line_current + (screen_row_total - (output_current_row - screen_begin_row)) >= display_line_total) // Reach end
					{
						break;
					}
					line_current += (screen_row_total - (output_current_row - screen_begin_row));
					output_current_row = screen_row_total;
					output_end_row = SCREEN_ROWS - 1;
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
					clrline(output_current_row, SCREEN_ROWS);
					break;
				case KEY_PGDN:
					if (line_current + screen_row_total - (output_current_row - screen_begin_row) >= display_line_total) // Reach end
					{
						break;
					}
					line_current += (screen_row_total - 1) - (output_current_row - screen_begin_row);
					if (line_current + screen_row_total > display_line_total) // No enough lines to display
					{
						line_current = display_line_total - screen_row_total;
					}
					output_current_row = screen_begin_row;
					output_end_row = SCREEN_ROWS - 1;
					clrline(output_current_row, SCREEN_ROWS);
					break;
				case KEY_ESC:
				case KEY_LEFT:
					loop = 0;
					break;
				case 'h':
					if (!show_help) // Not reentrant
					{
						break;
					}
					// Display help information
					show_help = 0;
					display_file(help_filename, 1);
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
			}

			continue;
		}

		len = p_line_offsets[line_current + 1] - p_line_offsets[line_current];
		if (len >= sizeof(buffer))
		{
			log_error("Buffer overflow: len=%ld(%ld - %ld) line=%ld \n",
					  len, p_line_offsets[line_current + 1], p_line_offsets[line_current], line_current);
			len = sizeof(buffer) - 1;
		}
		else if (len < 0)
		{
			log_error("Incorrect line offsets: len=%ld(%ld - %ld) line=%ld \n",
					  len, p_line_offsets[line_current + 1], p_line_offsets[line_current], line_current);
			len = 0;
		}

		memcpy(buffer, (const char *)p_data + p_line_offsets[line_current], (size_t)len);
		buffer[len] = '\0';

		moveto(output_current_row, 0);
		clrtoeol();
		prints("%s", buffer);
		line_current++;
		output_current_row++;
	}

cleanup:
	return ch;
}

static int display_file_key_handler(int *p_key, DISPLAY_CTX *p_ctx)
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

int display_file(const char *filename, int eof_exit)
{
	int ret;
	const void *p_shm;
	size_t data_len;
	long line_total;
	const void *p_data;
	const long *p_line_offsets;

	if ((p_shm = get_file_shm_readonly(filename, &data_len, &line_total, &p_data, &p_line_offsets)) == NULL)
	{
		log_error("get_file_shm(%s) error\n", filename);
		return KEY_NULL;
	}

	if (user_online_update("VIEW_FILE") < 0)
	{
		log_error("user_online_update(VIEW_FILE) error\n");
	}

	ret = display_data(p_data, line_total, p_line_offsets, eof_exit, display_file_key_handler, DATA_READ_HELP);

	if (detach_file_shm(p_shm) < 0)
	{
		log_error("detach_file_shm(%s) error\n", filename);
	}

	return ret;
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
	len = split_line(str_left_f, STR_TOP_LEFT_MAX_LEN / 2, &eol, &str_left_len, 1);
	str_left_f[len] = '\0';

	strncpy(str_middle_f, str_middle, sizeof(str_middle_f) - 1);
	str_middle_f[sizeof(str_middle_f) - 1] = '\0';
	len = split_line(str_middle, STR_TOP_MIDDLE_MAX_LEN / 2, &eol, &str_middle_len, 1);
	str_middle_f[len] = '\0';

	strncpy(str_right_f, str_right, sizeof(str_right_f) - 1);
	str_right_f[sizeof(str_right_f) - 1] = '\0';
	len = split_line(str_right, STR_TOP_RIGHT_MAX_LEN / 2, &eol, &str_right_len, 1);
	str_right_f[len] = '\0';

	moveto(1, 0);
	clrtoeol();
	prints("\033[1;44;33m%s\033[37m%*s%s\033[33m%*s%s\033[m",
		   str_left_f, 44 - str_left_len - str_middle_len, "",
		   str_middle_f, 36 - str_right_len, "", str_right_f);

	return 0;
}

int show_bottom(const char *msg)
{
	char str_time[LINE_BUFFER_LEN];
	time_t time_online;
	struct tm *tm_online;
	char msg_f[LINE_BUFFER_LEN];
	int eol;
	int msg_len;
	int len;
	int len_username;
	char str_tm_online[LINE_BUFFER_LEN];

	get_time_str(str_time, sizeof(str_time));

	msg_f[0] = '\0';
	msg_len = 0;
	if (msg != NULL)
	{
		strncpy(msg_f, msg, sizeof(msg_f) - 1);
		msg_f[sizeof(msg_f) - 1] = '\0';
		len = split_line(msg_f, 23, &eol, &msg_len, 1);
		msg_f[len] = '\0';
	}

	len_username = (int)strnlen(BBS_username, sizeof(BBS_username));

	time_online = time(NULL) - BBS_login_tm;
	tm_online = gmtime(&time_online);
	if (tm_online->tm_mday > 1)
	{
		snprintf(str_tm_online, sizeof(str_tm_online),
				 "\033[36m%2d\033[33m天\033[36m%2d\033[33m时",
				 tm_online->tm_mday - 1, tm_online->tm_hour);
	}
	else
	{
		snprintf(str_tm_online, sizeof(str_tm_online),
				 "\033[36m%2d\033[33m时\033[36m%2d\033[33m分",
				 tm_online->tm_hour, tm_online->tm_min);
	}

	moveto(SCREEN_ROWS, 0);
	clrtoeol();
	prints("\033[1;44;33m时间[\033[36m%s\033[33m]%s%*s \033[33m帐号[\033[36m%s\033[33m][%s\033[33m]\033[m",
		   str_time, msg_f, 38 - msg_len - len_username, "", BBS_username, str_tm_online);

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

	if (time(NULL) - t_last_show >= 10)
	{
		line_last = line_current;
		t_last_show = time(NULL);
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
