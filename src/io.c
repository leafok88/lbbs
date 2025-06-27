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

#include "common.h"
#include "io.h"
#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

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
			log_error("Output buffer is full, additional %d is required\n", ret);
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

int iflush(void)
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
		if (close(epollfd) < 0)
		{
			log_error("close(epoll) error (%d)\n");
		}
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
					if (SSH_v2)
					{
						ret = ssh_channel_write(SSH_channel, stdout_buf + stdout_buf_offset, (uint32_t)(stdout_buf_len - stdout_buf_offset));
						if (ret == SSH_ERROR)
						{
							log_error("ssh_channel_write() error: %s\n", ssh_get_error(SSH_session));
							retry = 0;
							break;
						}
					}
					else
					{
						ret = (int)write(STDOUT_FILENO, stdout_buf + stdout_buf_offset, (size_t)(stdout_buf_len - stdout_buf_offset));
					}
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
#ifdef _DEBUG
							log_error("write(STDOUT) error (%d)\n", errno);
#endif
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

	if (close(epollfd) < 0)
	{
		log_error("close(epoll) error (%d)\n");
	}

	return ret;
}

int igetch(int timeout)
{
	// static input buffer
	static unsigned char buf[LINE_BUFFER_LEN];
	static int len = 0;
	static int pos = 0;

	struct epoll_event ev, events[MAX_EVENTS];
	int nfds, epollfd;
	int ret;
	int loop;

	unsigned char tmp[LINE_BUFFER_LEN];
	int out = KEY_NULL;
	int in_esc = 0;
	int in_ascii = 0;
	int in_control = 0;
	int i = 0;
	int flags;

	if (pos >= len)
	{
		len = 0;
		pos = 0;

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

			if (close(epollfd) < 0)
			{
				log_error("close(epoll) error (%d)\n");
			}
			return -1;
		}

		flags = fcntl(STDIN_FILENO, F_GETFL, 0);
		fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

		for (loop = 1; loop && !SYS_server_exit;)
		{
			if (SSH_v2 && ssh_channel_is_closed(SSH_channel))
			{
				log_error("SSH channel is closed\n");
				loop = 0;
				break;
			}

			nfds = epoll_wait(epollfd, events, MAX_EVENTS, timeout);

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
				out = KEY_TIMEOUT;
				break;
			}

			for (int i = 0; i < nfds; i++)
			{
				if (events[i].data.fd == STDIN_FILENO)
				{
					while (len < sizeof(buf) && !SYS_server_exit) // read until complete or error
					{
						if (SSH_v2)
						{
							ret = ssh_channel_read_nonblocking(SSH_channel, buf + len, sizeof(buf) - (uint32_t)len, 0);
							if (ret == SSH_ERROR)
							{
								log_error("ssh_channel_read_nonblocking() error: %s\n", ssh_get_error(SSH_session));
								loop = 0;
								break;
							}
							else if (ret == SSH_EOF)
							{
								loop = 0;
								break;
							}
							else if (ret == 0)
							{
								out = 0;
								break; // Check whether channel is still open
							}
						}
						else
						{
							ret = (int)read(STDIN_FILENO, buf + len, sizeof(buf) - (size_t)len);
						}
						if (ret < 0)
						{
							if (errno == EAGAIN || errno == EWOULDBLOCK)
							{
								out = 0;
								loop = 0;
								break;
							}
							else if (errno == EINTR)
							{
								continue;
							}
							else
							{
#ifdef _DEBUG
								log_error("read(STDIN) error (%d)\n", errno);
#endif
								loop = 0;
								break;
							}
						}
						else if (ret == 0) // broken pipe
						{
							loop = 0;
							break;
						}
						else
						{
							len += ret;
							continue;
						}
					}
				}
			}

			// For debug
#ifdef _DEBUG
			for (int j = pos; j < len; j++)
			{
				log_common("Debug: <--[%u]\n", (buf[j] + 256) % 256);
			}
#endif
		}

		fcntl(STDIN_FILENO, F_SETFL, flags);

		if (close(epollfd) < 0)
		{
			log_error("close(epoll) error (%d)\n");
		}
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

		if (c == KEY_ESC)
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
				out = KEY_CSI;
				in_esc = 0;
				break;
			}
		}

		in_esc = 0;

		if (in_ascii)
		{
			tmp[i++] = c;
			if (i == 2 && (tmp[0] == 79 || tmp[0] == 91))
			{
				in_ascii = 0;
				switch (tmp[1])
				{
				case 65:
					out = KEY_UP;
					break;
				case 66:
					out = KEY_DOWN;
					break;
				case 67:
					out = KEY_RIGHT;
					break;
				case 68:
					out = KEY_LEFT;
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 2 && tmp[0] == 91) // Fterm
			{
				in_ascii = 0;
				switch (tmp[1])
				{
				case 86:
					out = KEY_SHIFT_F1;
					break;
				case 90:
					out = KEY_SHIFT_F2;
					break;
				case 97:
					out = KEY_SHIFT_F3;
					break;
				case 98:
					out = KEY_SHIFT_F4;
					break;
				case 99:
					out = KEY_SHIFT_F5;
					break;
				case 100:
					out = KEY_SHIFT_F6;
					break;
				case 101:
					out = KEY_SHIFT_F7;
					break;
				case 102:
					out = KEY_SHIFT_F8;
					break;
				case 103:
					out = KEY_SHIFT_F9;
					break;
				case 104:
					out = KEY_SHIFT_F10;
					break;
				case 107:
					out = KEY_CTRL_F1;
					break;
				case 108:
					out = KEY_CTRL_F2;
					break;
				case 109:
					out = KEY_CTRL_F3;
					break;
				case 112:
					out = KEY_CTRL_F6;
					break;
				case 113:
					out = KEY_CTRL_F7;
					break;
				case 114:
					out = KEY_CTRL_F8;
					break;
				case 115:
					out = KEY_CTRL_F9;
					break;
				case 116:
					out = KEY_CTRL_F10;
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 2 && tmp[0] == 79) // Xterm
			{
				in_ascii = 0;
				switch (tmp[1])
				{
				case 80:
					out = KEY_F1;
					break;
				case 81:
					out = KEY_F2;
					break;
				case 82:
					out = KEY_F3;
					break;
				case 83:
					out = KEY_F4;
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 3 && tmp[0] == 91 && tmp[2] == 126)
			{
				in_ascii = 0;
				switch (tmp[1])
				{
				case 49:
					out = KEY_HOME;
					break;
				case 50:
					out = KEY_INS;
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
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 4 && tmp[0] == 91 && tmp[1] == 49 && tmp[3] == 126) // Fterm
			{
				in_ascii = 0;
				switch (tmp[2])
				{
				case 49:
					out = KEY_F1;
					break;
				case 50:
					out = KEY_F2;
					break;
				case 51:
					out = KEY_F3;
					break;
				case 52:
					out = KEY_F4;
					break;
				case 53:
					out = KEY_F5;
					break;
				case 55:
					out = KEY_F6;
					break;
				case 56:
					out = KEY_F7;
					break;
				case 57:
					out = KEY_F8;
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 4 && tmp[0] == 91 && tmp[1] == 50 && tmp[3] == 126) // Fterm
			{
				in_ascii = 0;
				switch (tmp[2])
				{
				case 48:
					out = KEY_F9;
					break;
				case 49:
					out = KEY_F10;
					break;
				case 50:
					out = KEY_F11; // Fterm
					break;
				case 51:
					out = KEY_F11; // Xterm
					break;
				case 52:
					out = KEY_F12; // Xterm
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 5 && tmp[0] == 91 && tmp[1] == 49 && tmp[2] == 59 && tmp[3] == 53) // Xterm
			{
				in_ascii = 0;
				switch (tmp[4])
				{
				case 65:
					out = KEY_CTRL_UP;
					break;
				case 66:
					out = KEY_CTRL_DOWN;
					break;
				case 67:
					out = KEY_CTRL_RIGHT;
					break;
				case 68:
					out = KEY_CTRL_LEFT;
					break;
				case 70:
					out = KEY_CTRL_END;
					break;
				case 72:
					out = KEY_CTRL_HOME;
					break;
				case 80:
					out = KEY_CTRL_F1;
					break;
				case 81:
					out = KEY_CTRL_F2;
					break;
				case 82:
					out = KEY_CTRL_F3;
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 6 && tmp[0] == 91 && tmp[1] == 49 && tmp[3] == 59 && tmp[4] == 53 && tmp[5] == 126) // Xterm
			{
				in_ascii = 0;
				switch (tmp[2])
				{
				case 53:
					out = KEY_CTRL_F5;
					break;
				case 55:
					out = KEY_CTRL_F6;
					break;
				case 56:
					out = KEY_CTRL_F7;
					break;
				case 57:
					out = KEY_CTRL_F8;
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 6 && tmp[0] == 91 && tmp[1] == 50 && tmp[3] == 59 && tmp[4] == 53 && tmp[5] == 126) // Xterm
			{
				in_ascii = 0;
				switch (tmp[2])
				{
				case 48:
					out = KEY_CTRL_F9;
					break;
				case 49:
					out = KEY_CTRL_F10;
					break;
				case 51:
					out = KEY_CTRL_F11;
					break;
				case 52:
					out = KEY_CTRL_F12;
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 5 && tmp[0] == 91 && tmp[1] == 49 && tmp[2] == 59 && tmp[3] == 50) // Xterm
			{
				in_ascii = 0;
				switch (tmp[4])
				{
				case 80:
					out = KEY_SHIFT_F1;
					break;
				case 81:
					out = KEY_SHIFT_F2;
					break;
				case 82:
					out = KEY_SHIFT_F3;
					break;
				case 83:
					out = KEY_SHIFT_F4;
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 6 && tmp[0] == 91 && tmp[1] == 49 && tmp[3] == 59 && tmp[4] == 50 && tmp[5] == 126) // Xterm
			{
				in_ascii = 0;
				switch (tmp[2])
				{
				case 53:
					out = KEY_SHIFT_F5;
					break;
				case 55:
					out = KEY_SHIFT_F6;
					break;
				case 56:
					out = KEY_SHIFT_F7;
					break;
				case 57:
					out = KEY_SHIFT_F8;
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}
			if (i == 6 && tmp[0] == 91 && tmp[1] == 50 && tmp[3] == 59 && tmp[4] == 50 && tmp[5] == 126) // Xterm
			{
				in_ascii = 0;
				switch (tmp[2])
				{
				case 48:
					out = KEY_SHIFT_F9;
					break;
				case 49:
					out = KEY_SHIFT_F10;
					break;
				case 51:
					out = KEY_SHIFT_F11;
					break;
				case 52:
					out = KEY_SHIFT_F12;
					break;
				default:
					in_ascii = 1;
				}
				if (!in_ascii)
				{
					break;
				}
			}

			if (c == 'm')
			{
				in_ascii = 0;
			}
			continue;
		}

		out = ((int)c + 256) % 256;
		break;
	}

	// For ESC key
	if (out == 0 && in_esc)
	{
		out = KEY_ESC;
	}

	// for debug
#ifdef _DEBUG
	if (out != KEY_TIMEOUT && out != KEY_NULL)
	{
		log_common("Debug: -->[0x %x]\n", out);
	}
#endif

	return out;
}

int igetch_t(int sec)
{
	int ch;
	time_t t_begin = time(NULL);

	do
	{
		ch = igetch(100);
	} while (!SYS_server_exit && ch == KEY_TIMEOUT && (time(NULL) - t_begin < sec));

	return ch;
}

void igetch_reset()
{
	int ch;
	do
	{
		ch = igetch(0);
	} while (ch != KEY_NULL && ch != KEY_TIMEOUT);
}
