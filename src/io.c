/***************************************************************************
							io.c  -  description
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

#include "io.h"
#include "common.h"
#include "tcplib.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
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

int igetch()
{
	static char buf[256];
	unsigned char c, tmp[256];
	int out = KEY_NULL, loop = 1, in_esc = 0, in_ascii = 0, in_control = 0, i = 0, j, result;
	static int len = 0, pos = 0;
	fd_set testfds;
	struct timeval timeout;

	// Stop on system exit
	if (SYS_exit)
		return KEY_NULL;

	if (pos >= len)
	{
		pos = 0;
		len = 0;

		FD_ZERO(&testfds);
		FD_SET(0, &testfds);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		result = SignalSafeSelect(FD_SETSIZE, &testfds, (fd_set *)NULL,
								  (fd_set *)NULL, &timeout);

		if (result == 0)
		{
			return KEY_TIMEOUT;
		}
		if (result < 0)
		{
			log_error("select() error (%d) !\n", result);
			return KEY_NULL;
		}
		if (result > 0)
		{
			if (FD_ISSET(0, &testfds))
			{
				len = read(0, buf, 255);
			}
		}

		// For debug
		// for (j = 0; j < len; j++)
		//   log_std ("<--[%u]\n", (buf[j] + 256) % 256);
	}

	while (pos < len)
	{
		c = buf[pos++];

		if (c == '\0')
		{
			out = c;
			break;
		}

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
		ch = igetch();
	} while ((ch == KEY_TIMEOUT || ch == 0xa) && (time(0) - t_begin < sec));

	return ch;
}

int ikbhit()
{
	int len;

	ioctl(0, FIONREAD, &len);

	return len;
}
