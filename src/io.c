/***************************************************************************
							io.c  -  description
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

#include "io.h"
#include "log.h"
#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>

int outc(char c)
{
	int retval;

	retval = fprintf(stdout, "%c", c);

	return retval;
}

int prints(const char *format, ...)
{
	va_list args;
	int retval;

	va_start(args, format);
	retval = vfprintf(stdout, format, args);
	va_end(args);

	return retval;
}

int iflush()
{
	int retval;

	retval = fflush(stdout);

	return retval;
}

int igetch(int clear_buf)
{
	// static input buffer
	static unsigned char buf[LINE_BUFFER_LEN];
	static ssize_t len = 0;
	static int pos = 0;

	fd_set testfds;
	struct timeval timeout;

	unsigned char tmp[LINE_BUFFER_LEN];
	int ret;
	int out = '\0';
	int in_esc = 0;
	int in_ascii = 0;
	int in_control = 0;
	int i = 0;
	int flags;

	if (clear_buf)
	{
		pos = 0;
		len = 0;

		return '\0';
	}

	while (!SYS_server_exit && pos >= len)
	{
		FD_ZERO(&testfds);
		FD_SET(STDIN_FILENO, &testfds);

		timeout.tv_sec = 0;
		timeout.tv_usec = 100 * 1000; // 0.1 second

		ret = select(STDIN_FILENO + 1, &testfds, NULL, NULL, &timeout);

		if (ret < 0)
		{
			if (errno != EINTR)
			{
				log_error("select() error (%d) !\n", errno);
				return KEY_NULL;
			}
			continue;
		}
		else if (ret == 0)
		{
			return KEY_TIMEOUT;
		}

		if (FD_ISSET(STDIN_FILENO, &testfds))
		{
			flags = fcntl(STDIN_FILENO, F_GETFL, 0);
			fcntl(socket_server, F_SETFL, flags | O_NONBLOCK);

			len = read(STDIN_FILENO, buf, sizeof(buf));
			if (len < 0)
			{
				if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
				{
					log_error("read(STDIN) error (%d)\n", errno);
				}
			}
			else if (len == 0)
			{
				out = KEY_NULL; // broken pipe
			}

			pos = 0;

			fcntl(STDIN_FILENO, F_SETFL, flags);

			break;
		}

		// For debug
		// for (j = 0; j < len; j++)
		//   log_std ("<--[%u]\n", (buf[j] + 256) % 256);
	}

	while (pos < len)
	{
		unsigned char c = buf[pos++];

		if (c == KEY_CONTROL)
		{
			if (in_control == 0)
			{
				in_control = 1;
				i = 0;
				continue;
			}
		}

		if (in_control)
		{
			tmp[i++] = c;
			if (i >= 2)
			{
				out = (int)tmp[0] * 256 + tmp[1];
				in_control = 0;
				break;
			}
			continue;
		}

		if (c == ESC_KEY)
		{
			if (in_esc == 0)
			{
				in_esc = 1;
				in_ascii = 1;
				i = 0;
				continue;
			}
			else
			{
				out = ESC_KEY;
				in_esc = 0;
				break;
			}
		}

		in_esc = 0;

		if (in_ascii)
		{
			tmp[i++] = c;
			if (c == 'm')
			{
				in_ascii = 0;
				continue;
			}
			if (i == 2 && c >= 'A' && c <= 'D')
			{
				in_ascii = 0;
				switch (c)
				{
				case 'A':
					out = KEY_UP;
					break;
				case 'B':
					out = KEY_DOWN;
					break;
				case 'C':
					out = KEY_RIGHT;
					break;
				case 'D':
					out = KEY_LEFT;
					break;
				}
				break;
			}
			if (i == 3 && tmp[0] == 91 && tmp[2] == 126)
			{
				in_ascii = 0;
				switch (tmp[1])
				{
				case 49:
					out = KEY_HOME;
					break;
				case 51:
					out = KEY_DEL;
					break;
				case 52:
					out = KEY_END;
					break;
				case 53:
					out = KEY_PGUP;
					break;
				case 54:
					out = KEY_PGDN;
					break;
				}
				break;
			}
			continue;
		}

		out = ((int)c + 256) % 256;
		break;
	}

	// for debug
	// log_std ("-->[%u]\n", out);

	return out;
}

int igetch_t(long int sec)
{
	int ch;
	time_t t_begin = time(0);

	do
	{
		ch = igetch(0);
	} while (!SYS_server_exit && ch == KEY_TIMEOUT && (time(0) - t_begin < sec));

	return ch;
}
