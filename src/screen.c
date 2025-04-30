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
#include "log.h"
#include "io.h"
#include "screen.h"
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define ACTIVE_BOARD_HEIGHT 8

int screen_lines = 24;

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
	iflush();
}

void clrtoeol()
{
	prints("\033[K");
	iflush();
}

void clrline(int line_begin, int line_end)
{
	int i;

	for (i = line_begin; i <= line_end; i++)
	{
		moveto(i, 0);
		prints("\033[K");
	}

	iflush();
}

void clrtobot(int line_begin)
{
	clrline(line_begin, screen_lines);
	moveto(line_begin, 0);
}

void clearscr()
{
	prints("\033[2J");
	moveto(0, 0);
	iflush();
}

int press_any_key()
{
	moveto(screen_lines, 0);
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
		igetch_t(60);
		igetch_t(60);
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
	iflush();
	prints(prompt);
	prints(buffer);
	iflush();

	len = _str_input(buffer, buffer_length, echo_mode);

	return len;
}

int display_file(const char *filename)
{
	char buffer[260];
	FILE *fin;
	int i;

	if ((fin = fopen(filename, "r")) == NULL)
	{
		return -1;
	}

	while (fgets(buffer, 255, fin))
	{
		i = strlen(buffer);
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
	char buffer[260], temp[256];
	int i, ch, input_ok, max_lines;
	long int line, c_line_begin = 0, c_line_total = 0;
	long int f_line, f_size, f_offset;
	FILE *fin;
	struct stat f_stat;

	max_lines = screen_lines - 1;
	clrline(begin_line, screen_lines);
	line = begin_line;
	moveto(line, 0);

	if ((fin = fopen(filename, "r")) != NULL)
	{
		if (fstat(fileno(fin), &f_stat) != 0)
		{
			log_error("Get file stat failed\n");
			return -1;
		}
		f_size = f_stat.st_size;

		while (fgets(buffer, 255, fin))
			c_line_total++;
		rewind(fin);

		while (fgets(buffer, 255, fin))
		{
			if (line >= max_lines)
			{
				f_offset = ftell(fin);

				moveto(screen_lines, 0);
				prints("\033[1;44;32m下面还有喔 (%d%%)\033[33m   │ 结束 ← <q> │ ↑/↓/PgUp/PgDn 移动 │ ? 辅助说明 │     \033[m",
					   (f_offset - strlen(buffer)) * 100 / f_size);
				iflush();

				input_ok = 0;
				while (!input_ok)
				{
					ch = igetch_t(MAX_DELAY_TIME);
					input_ok = 1;
					switch (ch)
					{
					case KEY_UP:
						c_line_begin--;
						if (c_line_begin >= 0)
						{
							rewind(fin);
							for (f_line = 0; f_line < c_line_begin; f_line++)
							{
								if (fgets(buffer, 255, fin) == NULL)
									goto exit;
							}
						}
						else
						{
							goto exit;
						}
						break;
					case KEY_DOWN:
					case CR:
						c_line_begin++;
						rewind(fin);
						for (f_line = 0; f_line < c_line_begin; f_line++)
						{
							if (fgets(buffer, 255, fin) == NULL)
								goto exit;
						}
						break;
					case KEY_PGUP:
					case Ctrl('B'):
						if (c_line_begin > 0)
							c_line_begin -= (max_lines - begin_line - 1);
						else
							goto exit;
						if (c_line_begin < 0)
							c_line_begin = 0;
						rewind(fin);
						for (f_line = 0; f_line < c_line_begin; f_line++)
						{
							if (fgets(buffer, 255, fin) == NULL)
								goto exit;
						}
						break;
					case KEY_RIGHT:
					case KEY_PGDN:
					case Ctrl('F'):
					case KEY_SPACE:
						c_line_begin += (max_lines - begin_line - 1);
						if (c_line_begin + (max_lines - begin_line) >
							c_line_total)
							c_line_begin =
								c_line_total - (max_lines - begin_line);
						rewind(fin);
						for (f_line = 0; f_line < c_line_begin; f_line++)
						{
							if (fgets(buffer, 255, fin) == NULL)
								goto exit;
						}
						break;
					case KEY_NULL:
					case KEY_TIMEOUT:
					case KEY_LEFT:
					case 'q':
					case 'Q':
						goto exit;
						break;
					case '?':
					case 'h':
					case 'H':
						// Display help information
						strcpy(temp, app_home_dir);
						strcat(temp, "data/read_help.txt");
						display_file_ex(temp, begin_line, 1);

						// Refresh after display help information
						rewind(fin);
						for (f_line = 0; f_line < c_line_begin; f_line++)
						{
							if (fgets(buffer, 255, fin) == NULL)
								goto exit;
						}
						break;
					default:
						input_ok = 0;
						break;
					}
				}

				clrline(begin_line, screen_lines);
				line = begin_line;
				moveto(line, 0);

				continue;
			}

			i = strlen(buffer);
			if (buffer[i - 1] == '\n' && buffer[i - 2] != '\r')
			{
				buffer[i - 1] = '\r';
				buffer[i] = '\n';
				buffer[i + 1] = '\0';
			}
			prints(buffer);
			iflush();

			line++;
		}
		if (wait)
			ch = press_any_key();
		else
			ch = 0;

	exit:
		fclose(fin);

		return ch;
	}

	return -1;
}

int show_top(char *status)
{
	char buffer[256];

	str_space(buffer, 20 - strlen(BBS_current_section_name));

	moveto(1, 0);
	prints("\033[1;44;33m%-20s \033[37m%20s"
		   "         %s\033[33m讨论区 [%s]\033[m",
		   status, BBS_name, buffer, BBS_current_section_name);
	iflush();

	return 0;
}

int show_bottom(char *msg)
{
	char str_time[256], str_time_onine[20], buffer[256];
	time_t time_online;
	struct tm *tm_online;

	get_time_str(str_time, 256);
	str_space(buffer, 33 - strlen(BBS_username));

	time_online = time(0) - BBS_login_tm;
	tm_online = gmtime(&time_online);

	moveto(screen_lines, 0);
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
	char filename[256], buffer[260];
	FILE *fin;
	int i, j;
	static long int line;

	sprintf(filename, "%sdata/active_board.txt", app_home_dir);

	clrline(3, 2 + ACTIVE_BOARD_HEIGHT);

	moveto(3, 0);

	if ((fin = fopen(filename, "r")) != NULL)
	{
		for (j = 0; j < line; j++)
		{
			if (fgets(buffer, 255, fin) == NULL)
			{
				line = 0;
				rewind(fin);
				break;
			}
		}

		for (j = 0; j < ACTIVE_BOARD_HEIGHT; j++)
		{
			if (fgets(buffer, 255, fin) == NULL)
			{
				line = 0;
				if (j == 0)
				{
					rewind(fin);
					if (fgets(buffer, 255, fin) == NULL)
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			line++;
			i = strlen(buffer);
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
	}

	return 0;
}
