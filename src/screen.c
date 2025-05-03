/***************************************************************************
						  screen.c  -  description
							 -------------------
	begin                : Mon Oct 18 2004
	copyright            : (C) 2004 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "bbs.h"
#include "common.h"
#include "str_process.h"
#include "log.h"
#include "io.h"
#include "screen.h"
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#define ACTIVE_BOARD_HEIGHT 8

int screen_rows = 24;
int screen_cols = 80;

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
	prints("\033[K");
}

void clrline(int line_begin, int line_end)
{
	int i;

	for (i = line_begin; i <= line_end; i++)
	{
		moveto(i, 0);
		prints("\033[K");
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
	igetch(1);

	moveto(screen_rows, 0);
	clrtoeol();

	prints("                           \033[1;33m按任意键继续...\033[0;37m");
	iflush();

	return igetch_t(60);
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
		igetch(1);
	}
}

static int _str_input(char *buffer, int buffer_length, int echo_mode)
{
	char buf[256], ch;
	int c, offset = 0, len, loop = 1, i, hz = 0;

	for (i = 0; i < buffer_length && buffer[i] != '\0'; i++)
	{
		offset++;
	}

	igetch(1);

	while (c = igetch_t(60))
	{
		if (c == KEY_NULL || c == KEY_TIMEOUT || c == CR)
		{
			break;
		}
		if (c == LF)
		{
			continue;
		}
		if (c == BACKSPACE)
		{
			if (offset > 0)
			{
				buffer[--offset] = '\0';
				prints("\b \b");
				//            clrtoeol ();
				iflush();
			}
			continue;
		}
		if (c > 255 || iscntrl(c))
		{
			continue;
		}
		if (c > 127 && c <= 255)
		{
			hz = (!hz);
		}
		if (offset >= buffer_length)
		{
			outc('\a');
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

	prints("\r\n");
	iflush();

	return offset;
}

int str_input(char *buffer, int buffer_length, int echo_mode)
{
	int offset;

	memset(buffer, '\0', buffer_length);

	offset = _str_input(buffer, buffer_length, echo_mode);

	return offset;
};

int get_data(int row, int col, char *prompt, char *buffer, int buffer_length, int echo_mode)
{
	int len;

	moveto(row, col);
	prints(prompt);
	prints(buffer);
	iflush();

	len = _str_input(buffer, buffer_length, echo_mode);

	return len;
}

int display_file(const char *filename)
{
	char buffer[LINE_BUFFER_LEN];
	FILE *fin;
	int i;

	if ((fin = fopen(filename, "r")) == NULL)
	{
		return -1;
	}

	while (fgets(buffer, sizeof(buffer) - 1, fin))
	{
		i = strnlen(buffer, sizeof(buffer) - 1);
		if (buffer[i - 1] == '\n' && buffer[i - 2] != '\r')
		{
			buffer[i - 1] = '\r';
			buffer[i] = '\n';
			buffer[i + 1] = '\0';
		}
		prints(buffer);
		iflush();
	}
	fclose(fin);

	return 0;
}

int display_file_ex(const char *filename, int begin_line, int wait)
{
	char buffer[LINE_BUFFER_LEN];
	char temp[LINE_BUFFER_LEN];
	int ch = 0;
	int input_ok, line, max_lines;
	long int c_line_current = 0, c_line_total = 0;
	FILE *fin;
	struct stat f_stat;
	long *p_line_offsets;
	int len;
	int percentile;
	int loop = 1;

	if ((fin = fopen(filename, "r")) == NULL)
	{
		log_error("Unable to open file %s\n", filename);
		return -1;
	}

	p_line_offsets = (long *)malloc(sizeof(long) * MAX_FILE_LINES);

	c_line_total = split_file_lines(fin, screen_cols, p_line_offsets, MAX_FILE_LINES);

	clrline(begin_line, screen_rows);
	line = begin_line;
	max_lines = screen_rows - 1;

	while (loop)
	{
		if (c_line_current >= c_line_total)
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

		if (line >= max_lines)
		{
			if (c_line_current - (line - 1) + (screen_rows - 2) < c_line_total)
			{
				percentile = (c_line_current - (line - 1) + (screen_rows - 2)) * 100 / c_line_total;
			}
			else
			{
				log_error("P100 reached\n");
				percentile = 100;
			}

			moveto(screen_rows, 0);
			prints("\033[1;44;32m下面还有喔 (%d%%)\033[33m   │ 结束 ← <q> │ ↑/↓/PgUp/PgDn 移动 │ ? 辅助说明 │     \033[m",
				   percentile);
			iflush();

			input_ok = 0;
			while (!input_ok)
			{
				ch = igetch_t(MAX_DELAY_TIME);
				input_ok = 1;
				switch (ch)
				{
				case KEY_UP:
					if (c_line_current - line < 0) // Reach top
					{
						break;
					}
					c_line_current -= line;
					line = begin_line;
					max_lines = begin_line + 1;
					prints("\033[1T"); // Scroll down 1 line
					break;
				case KEY_DOWN:
				case CR:
					if (c_line_current + ((screen_rows - 2) - (line - 1)) >= c_line_total) // Reach bottom
					{
						break;
					}
					c_line_current += ((screen_rows - 2) - (line - 1));
					line = screen_rows - 2;
					max_lines = screen_rows - 1;
					moveto(screen_rows, 0);
					clrtoeol();
					prints("\033[1S"); // Scroll up 1 line
					break;
				case KEY_PGUP:
				case Ctrl('B'):
					if (c_line_current - line < 0) // Reach top
					{
						break;
					}
					c_line_current -= ((screen_rows - 3) + (line - 1));
					if (c_line_current < 0)
					{
						c_line_current = 0;
					}
					line = begin_line;
					max_lines = screen_rows - 1;
					clrline(begin_line, screen_rows);
					break;
				case KEY_RIGHT:
				case KEY_PGDN:
				case Ctrl('F'):
				case KEY_SPACE:
					if (c_line_current + (screen_rows - 2) - (line - 1) >= c_line_total) // Reach bottom
					{
						break;
					}
					c_line_current += (screen_rows - 3) - (line - 1);
					if (c_line_current + screen_rows - 2 > c_line_total) // No enough lines to display
					{
						c_line_current = c_line_total - (screen_rows - 2);
					}
					line = begin_line;
					max_lines = screen_rows - 1;
					clrline(begin_line, screen_rows);
					break;
				case KEY_NULL:
				case KEY_TIMEOUT:
				case KEY_LEFT:
				case 'q':
				case 'Q':
					loop = 0;
					break;
				case '?':
				case 'h':
				case 'H':
					// Display help information
					strcpy(temp, app_home_dir);
					strcat(temp, "data/read_help.txt");
					display_file_ex(temp, begin_line, 1);

					// Refresh after display help information
					c_line_current -= (line - 1);
					line = begin_line;
					max_lines = screen_rows - 1;
					clrline(begin_line, screen_rows);
					break;
				default:
					input_ok = 0;
					break;
				}
			}

			continue;
		}

		fseek(fin, p_line_offsets[c_line_current], SEEK_SET);
		len = p_line_offsets[c_line_current + 1] - p_line_offsets[c_line_current];
		if (len >= LINE_BUFFER_LEN)
		{
			log_error("Error length exceeds buffer size: %d\n", len);
			len = LINE_BUFFER_LEN - 1;
		}
		if (fgets(buffer, len + 1, fin) == NULL)
		{
			log_error("Reach EOF\n");
			break;
		}
		moveto(line, 0);
		clrtoeol();
		prints("%s", buffer);
		c_line_current++;
		line++;
	}

	free(p_line_offsets);
	fclose(fin);

	return ch;
}

int show_top(char *status)
{
	char buffer[LINE_BUFFER_LEN];

	str_space(buffer, 20 - strlen(BBS_current_section_name));

	moveto(1, 0);
	clrtoeol();
	prints("\033[1;44;33m%-20s \033[37m%20s"
		   "         %s\033[33m讨论区 [%s]\033[m",
		   status, BBS_name, buffer, BBS_current_section_name);
	iflush();

	return 0;
}

int show_bottom(char *msg)
{
	char str_time[LINE_BUFFER_LEN];
	char str_time_onine[20];
	char buffer[LINE_BUFFER_LEN];
	time_t time_online;
	struct tm *tm_online;

	get_time_str(str_time, sizeof(str_time));
	str_space(buffer, 33 - strlen(BBS_username));

	time_online = time(0) - BBS_login_tm;
	tm_online = gmtime(&time_online);

	moveto(screen_rows, 0);
	clrtoeol();
	prints("\033[1;44;33m[\033[36m%s\033[33m]"
		   "%s帐号[\033[36m%s\033[33m]"
		   "[\033[36m%1d\033[33m:\033[36m%2d\033[33m:\033[36m%2d\033[33m]\033[m",
		   str_time, buffer, BBS_username, tm_online->tm_mday - 1,
		   tm_online->tm_hour, tm_online->tm_min);
	iflush();

	return 0;
}

int show_active_board()
{
	char filename[FILE_PATH_LEN];
	char buffer[LINE_BUFFER_LEN];
	FILE *fin;
	static int line;
	int len;
	int end_of_line;

	sprintf(filename, "%sdata/active_board.txt", app_home_dir);

	clrline(3, 2 + ACTIVE_BOARD_HEIGHT);

	if ((fin = fopen(filename, "r")) == NULL)
	{
		log_error("Unable to open file %s\n", filename);
		return -1;
	}

	for (int i = 0; i < line; i++)
	{
		if (fgets(buffer, sizeof(buffer), fin) == NULL)
		{
			line = 0;
			rewind(fin);
			break;
		}
	}

	for (int i = 0; i < ACTIVE_BOARD_HEIGHT; i++)
	{
		if (fgets(buffer, sizeof(buffer), fin) == NULL)
		{
			line = 0;
			break;
		}
		line++;
		len = split_line(buffer, screen_cols, &end_of_line);
		buffer[len] = '\0'; // Truncate over-length line
		moveto(3 + i, 0);
		prints("%s", buffer);
	}
	iflush();

	fclose(fin);

	return 0;
}
