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
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>

static char stdout_buf[BUFSIZ];
static int stdout_buf_len = 0;
static int stdout_buf_offset = 0;

int prints(const char *format, ...)
{
	char buf[BUFSIZ];
	va_list args;
	int ret;

	va_start(args, format);
	ret = vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	if (ret > 0)
	{
		if (stdout_buf_len + ret > BUFSIZ)
		{
			iflush();
		}
		
		if (stdout_buf_len + ret <= BUFSIZ)
		{
			memcpy(stdout_buf + stdout_buf_len, buf, (size_t)ret);
			stdout_buf_len += ret;
		}
		else
		{
			errno = EAGAIN;
			ret = (BUFSIZ - stdout_buf_len - ret);
		}
	}

	return ret;
}

int outc(char c)
{
	int ret;

	if (stdout_buf_len + 1 > BUFSIZ)
	{
		iflush();
	}

	if (stdout_buf_len + 1 <= BUFSIZ)
	{
		stdout_buf[stdout_buf_len] = c;
		stdout_buf_len++;
	}
	else
	{
		errno = EAGAIN;
		ret = -1;
	}

	return ret;
}

int iflush()
{
	int flags;
	struct epoll_event ev, events[MAX_EVENTS];
	int nfds, epollfd;
	int retry;
	int ret = 0;

	epollfd = epoll_create1(0);
	if (epollfd < 0)
	{
		log_error("epoll_create1() error (%d)\n", errno);
		return -1;
	}

	ev.events = EPOLLOUT;
	ev.data.fd = STDOUT_FILENO;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDOUT_FILENO, &ev) == -1)
	{
		log_error("epoll_ctl(STDOUT_FILENO) error (%d)\n", errno);
		return -1;
	}

	// Set STDOUT as non-blocking
	flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
	fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK);

	// Retry wait / flush for at most 3 times
	retry = 3;
	while (retry > 0 && !SYS_server_exit)
	{
		retry--;

		nfds = epoll_wait(epollfd, events, MAX_EVENTS, 100); // 0.1 second

		if (nfds < 0)
		{
			if (errno != EINTR)
			{
				log_error("epoll_wait() error (%d)\n", errno);
				break;
			}
			continue;
		}
		else if (nfds == 0) // timeout
		{
			continue;
		}

		for (int i = 0; i < nfds; i++)
		{
			if (events[i].data.fd == STDOUT_FILENO)
			{
				while (stdout_buf_offset < stdout_buf_len && !SYS_server_exit) // write until complete or error
				{
					ret = (int)write(STDOUT_FILENO, stdout_buf + stdout_buf_offset, (size_t)(stdout_buf_len - stdout_buf_offset));
					if (ret < 0)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
							break;
						}
						else if (errno == EINTR)
						{
							continue;
						}
						else
						{
							log_error("write(STDOUT) error (%d)\n", errno);
							retry = 0;
							break;
						}
					}
					else if (ret == 0) // broken pipe
					{
						retry = 0;
						break;
					}
					else
					{
						stdout_buf_offset += ret;
						if (stdout_buf_offset >= stdout_buf_len) // flush buffer completely
						{
							ret = 0;
							stdout_buf_offset = 0;
							stdout_buf_len = 0;
							retry = 0;
							break;
						}
						continue;
					}
				}
			}
		}
	}

	// Restore STDOUT flags
	fcntl(STDOUT_FILENO, F_SETFL, flags);

	return ret;
}

int igetch(int clear_buf)
{
	// static input buffer
	static unsigned char buf[LINE_BUFFER_LEN];
	static ssize_t len = 0;
	static int pos = 0;

	struct epoll_event ev, events[MAX_EVENTS];
	int nfds, epollfd;

	unsigned char tmp[LINE_BUFFER_LEN];
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

	epollfd = epoll_create1(0);
	if (epollfd < 0)
	{
		log_error("epoll_create1() error (%d)\n", errno);
		return -1;
	}

	ev.events = EPOLLIN;
	ev.data.fd = STDIN_FILENO;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1)
	{
		log_error("epoll_ctl(STDIN_FILENO) error (%d)\n", errno);
		return -1;
	}

	flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	while (!SYS_server_exit && pos >= len)
	{
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, 100); // 0.1 second

		if (nfds < 0)
		{
			if (errno != EINTR)
			{
				log_error("epoll_wait() error (%d)\n", errno);
				fcntl(STDIN_FILENO, F_SETFL, flags);
				return KEY_NULL;
			}
			continue;
		}
		else if (nfds == 0)
		{
			fcntl(STDIN_FILENO, F_SETFL, flags);
			return KEY_TIMEOUT;
		}

		for (int i = 0; i < nfds; i++)
		{
			if (events[i].data.fd == STDIN_FILENO)
			{
				while (!SYS_server_exit) // read until complete or error
				{
					len = read(STDIN_FILENO, buf, sizeof(buf));
					if (len < 0)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
							fcntl(STDIN_FILENO, F_SETFL, flags);
							return KEY_TIMEOUT;
						}
						else if (errno == EINTR)
						{
							continue;
						}
						else
						{
							log_error("read(STDIN) error (%d)\n", errno);
							fcntl(STDIN_FILENO, F_SETFL, flags);
							return KEY_NULL;
						}
					}
					else if (len == 0) // broken pipe
					{
						fcntl(STDIN_FILENO, F_SETFL, flags);
						return KEY_NULL;
					}

					pos = 0;
					break;
				}
			}
		}

		// For debug
		// for (j = 0; j < len; j++)
		//   log_std ("<--[%u]\n", (buf[j] + 256) % 256);
	}

	fcntl(STDIN_FILENO, F_SETFL, flags);

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
