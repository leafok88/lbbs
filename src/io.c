/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * io
 *   - basic terminal-based user input / output features
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

const char BBS_default_charset[CHARSET_MAX_LEN + 1] = "UTF-8";
char stdio_charset[CHARSET_MAX_LEN + 1] = "UTF-8";

// epoll for STDIO
static int stdin_epollfd = -1;
static int stdout_epollfd = -1;
static int stdin_flags;
static int stdout_flags;

// static input / output buffer
static char stdin_buf[LINE_BUFFER_LEN];
static char stdout_buf[BUFSIZ];
static int stdin_buf_len = 0;
static int stdout_buf_len = 0;
static int stdin_buf_offset = 0;
static int stdout_buf_offset = 0;

static char stdin_conv[LINE_BUFFER_LEN * 2];
static char stdout_conv[BUFSIZ * 2];
static int stdin_conv_len = 0;
static int stdout_conv_len = 0;
static int stdin_conv_offset = 0;
static int stdout_conv_offset = 0;

static iconv_t stdin_cd = NULL;
static iconv_t stdout_cd = NULL;

int io_init(void)
{
	struct epoll_event ev;

	if (stdin_epollfd == -1)
	{
		stdin_epollfd = epoll_create1(0);
		if (stdin_epollfd == -1)
		{
			log_error("epoll_create1() error (%d)\n", errno);
			return -1;
		}

		ev.events = EPOLLIN;
		ev.data.fd = STDIN_FILENO;
		if (epoll_ctl(stdin_epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1)
		{
			log_error("epoll_ctl(STDIN_FILENO) error (%d)\n", errno);
			if (close(stdin_epollfd) < 0)
			{
				log_error("close(stdin_epollfd) error (%d)\n");
			}
			stdin_epollfd = -1;
			return -1;
		}

		stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
		fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);
	}

	if (stdout_epollfd == -1)
	{
		stdout_epollfd = epoll_create1(0);
		if (stdout_epollfd == -1)
		{
			log_error("epoll_create1() error (%d)\n", errno);
			return -1;
		}

		ev.events = EPOLLOUT;
		ev.data.fd = STDOUT_FILENO;
		if (epoll_ctl(stdout_epollfd, EPOLL_CTL_ADD, STDOUT_FILENO, &ev) == -1)
		{
			log_error("epoll_ctl(STDOUT_FILENO) error (%d)\n", errno);
			if (close(stdout_epollfd) < 0)
			{
				log_error("close(stdout_epollfd) error (%d)\n");
			}
			stdout_epollfd = -1;
			return -1;
		}

		// Set STDOUT as non-blocking
		stdout_flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
		fcntl(STDOUT_FILENO, F_SETFL, stdout_flags | O_NONBLOCK);
	}

	return 0;
}

void io_cleanup(void)
{
	if (stdin_epollfd != -1)
	{
		fcntl(STDIN_FILENO, F_SETFL, stdin_flags);

		if (close(stdin_epollfd) < 0)
		{
			log_error("close(stdin_epollfd) error (%d)\n");
		}
		stdin_epollfd = -1;
	}

	if (stdout_epollfd != -1)
	{
		fcntl(STDOUT_FILENO, F_SETFL, stdout_flags);

		if (close(stdout_epollfd) < 0)
		{
			log_error("close(stdout_epollfd) error (%d)\n");
		}
		stdout_epollfd = -1;
	}
}

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
	struct epoll_event events[MAX_EVENTS];
	int nfds;
	int retry;
	int ret = 0;

	// Retry wait / flush for at most 3 times
	retry = 3;
	while (retry > 0 && !SYS_server_exit)
	{
		retry--;

		nfds = epoll_wait(stdout_epollfd, events, MAX_EVENTS, 100); // 0.1 second

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
				if (stdout_buf_offset < stdout_buf_len)
				{
					ret = io_buf_conv(stdout_cd, stdout_buf, &stdout_buf_len, &stdout_buf_offset, stdout_conv, sizeof(stdout_conv), &stdout_conv_len);
					if (ret < 0)
					{
						log_error("io_buf_conv(stdout, %d, %d, %d) error\n", stdout_buf_len, stdout_buf_offset, stdout_conv_len);
						stdout_buf_len = stdout_buf_offset; // Discard invalid sequence
					}
				}

				while (stdout_conv_offset < stdout_conv_len && !SYS_server_exit) // write until complete or error
				{
					if (SSH_v2)
					{
						ret = ssh_channel_write(SSH_channel, stdout_conv + stdout_conv_offset, (uint32_t)(stdout_conv_len - stdout_conv_offset));
						if (ret == SSH_ERROR)
						{
							log_error("ssh_channel_write() error: %s\n", ssh_get_error(SSH_session));
							retry = 0;
							break;
						}
					}
					else
					{
						ret = (int)write(STDOUT_FILENO, stdout_conv + stdout_conv_offset, (size_t)(stdout_conv_len - stdout_conv_offset));
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
						stdout_conv_offset += ret;
						if (stdout_conv_offset >= stdout_conv_len) // flush buffer completely
						{
							ret = 0;
							stdout_conv_offset = 0;
							stdout_conv_len = 0;
							retry = 0;
							break;
						}
						continue;
					}
				}
			}
		}
	}

	return ret;
}

int igetch(int timeout)
{
	static int stdin_read_wait = 0;

	struct epoll_event events[MAX_EVENTS];
	int nfds;
	int ret;
	int loop;

	unsigned char tmp[LINE_BUFFER_LEN];
	int out = KEY_NULL;
	int in_esc = 0;
	int in_ascii = 0;
	int in_control = 0;
	int i = 0;

	if (stdin_conv_offset >= stdin_conv_len)
	{
		stdin_conv_len = 0;
		stdin_conv_offset = 0;

		for (loop = 1; loop && stdin_buf_len < sizeof(stdin_buf) && stdin_conv_offset >= stdin_conv_len && !SYS_server_exit;)
		{
			if (SSH_v2 && ssh_channel_is_closed(SSH_channel))
			{
				log_error("SSH channel is closed\n");
				loop = 0;
				break;
			}

			if (!stdin_read_wait)
			{
				nfds = epoll_wait(stdin_epollfd, events, MAX_EVENTS, timeout);

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
						stdin_read_wait = 1;
					}
				}
			}

			if (stdin_read_wait)
			{
				while (stdin_buf_len < sizeof(stdin_buf) && !SYS_server_exit) // read until complete or error
				{
					if (SSH_v2)
					{
						ret = ssh_channel_read_nonblocking(SSH_channel, stdin_buf + stdin_buf_len, sizeof(stdin_buf) - (uint32_t)stdin_buf_len, 0);
						if (ret == SSH_ERROR)
						{
							log_error("ssh_channel_read_nonblocking() error: %s\n", ssh_get_error(SSH_session));
							loop = 0;
							break;
						}
						else if (ret == SSH_EOF)
						{
							stdin_read_wait = 0;
							loop = 0;
							break;
						}
						else if (ret == 0)
						{
							out = 0;
							stdin_read_wait = 0;
							loop = 0;
							break;
						}
					}
					else
					{
						ret = (int)read(STDIN_FILENO, stdin_buf + stdin_buf_len, sizeof(stdin_buf) - (size_t)stdin_buf_len);
					}
					if (ret < 0)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
							out = 0;
							stdin_read_wait = 0;
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
						stdin_read_wait = 0;
						loop = 0;
						break;
					}
					else
					{
						stdin_buf_len += ret;
						continue;
					}
				}
			}

			// For debug
#ifdef _DEBUG
			for (int j = stdin_buf_offset; j < stdin_buf_len; j++)
			{
				log_error("Debug input: <--[%u]\n", (stdin_buf[j] + 256) % 256);
			}
#endif
		}

		if (stdin_buf_offset < stdin_buf_len)
		{
			ret = io_buf_conv(stdin_cd, stdin_buf, &stdin_buf_len, &stdin_buf_offset, stdin_conv, sizeof(stdin_conv), &stdin_conv_len);
			if (ret < 0)
			{
				log_error("io_buf_conv(stdin, %d, %d, %d) error\n", stdin_buf_len, stdin_buf_offset, stdin_conv_len);
				stdin_buf_len = stdin_buf_offset; // Discard invalid sequence
			}

			// For debug
#ifdef _DEBUG
			for (int j = stdin_conv_offset; j < stdin_conv_len; j++)
			{
				log_error("Debug input_conv: <--[%u]\n", (stdin_conv[j] + 256) % 256);
			}
#endif
		}
	}

	while (stdin_conv_offset < stdin_conv_len)
	{
		unsigned char c = (unsigned char)stdin_conv[stdin_conv_offset++];

		// Convert \r\n to \r
		if (c == CR && stdin_conv_offset < stdin_conv_len && stdin_conv[stdin_conv_offset] == LF)
		{
			stdin_conv_offset++;
		}

		// Convert single \n to \r
		if (c == LF)
		{
			c = CR;
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
		log_error("Debug: -->[0x %x]\n", out);
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
		ch = igetch(100);
	} while (ch != KEY_NULL && ch != KEY_TIMEOUT);
}

int io_buf_conv(iconv_t cd, char *p_buf, int *p_buf_len, int *p_buf_offset, char *p_conv, size_t conv_size, int *p_conv_len)
{
	char *in_buf;
	char *out_buf;
	size_t in_bytes;
	size_t out_bytes;
	int ret;
	int in_control = 0;
	size_t i = 0;

	if (cd == NULL || p_buf == NULL || p_buf_len == NULL || p_buf_offset == NULL || p_conv == NULL || p_conv_len == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	in_buf = p_buf + *p_buf_offset;
	in_bytes = (size_t)(*p_buf_len - *p_buf_offset);
	out_buf = p_conv + *p_conv_len;
	out_bytes = conv_size - (size_t)(*p_conv_len);

	while (in_bytes > 0)
	{
		if ((unsigned char)(*in_buf) == KEY_CONTROL)
		{
			if (in_control == 0)
			{
				in_control = 1;
				i = 0;
			}
		}

		if (in_control)
		{
			if (out_bytes <= 0)
			{
				log_error("No enough free space in p_conv, conv_len=%d, conv_size=%d\n", *p_conv_len, conv_size);
				return -2;
			}

			*out_buf = *in_buf;
			in_buf++;
			out_buf++;
			in_bytes--;
			out_bytes--;

			(*p_buf_offset)++;
			*p_conv_len = (int)(conv_size - out_bytes);

			i++;
			if (i >= 2)
			{
				in_control = 0;
			}
			continue;
		}

		ret = (int)iconv(cd, &in_buf, &in_bytes, &out_buf, &out_bytes);
		if (ret == -1)
		{
			if (errno == EINVAL) // Incomplete
			{
#ifdef _DEBUG
				log_error("iconv(inbytes=%d, outbytes=%d) error: EINVAL, in_buf[0]=%d\n", in_bytes, out_bytes, in_buf[0]);
#endif
				if (p_buf != in_buf)
				{
					*p_buf_len = (int)(p_buf + *p_buf_len - in_buf);
					*p_buf_offset = 0;
					*p_conv_len = (int)(conv_size - out_bytes);
					memmove(p_buf, in_buf, (size_t)(*p_buf_len));
				}

				break;
			}
			else if (errno == E2BIG)
			{
				log_error("iconv(inbytes=%d, outbytes=%d) error: E2BIG\n", in_bytes, out_bytes);
				return -1;
			}
			else if (errno == EILSEQ)
			{
				if (in_bytes > out_bytes || out_bytes <= 0)
				{
					log_error("iconv(inbytes=%d, outbytes=%d) error: EILSEQ and E2BIG\n", in_bytes, out_bytes);
					return -2;
				}

				// reset in_bytes when "//IGNORE" is applied
				if (in_bytes == 0)
				{
					in_bytes = (size_t)(*p_buf_len - *p_buf_offset);
				}

				*out_buf = *in_buf;
				in_buf++;
				out_buf++;
				in_bytes--;
				out_bytes--;

				continue;
			}
		}
		else
		{
			*p_buf_len = 0;
			*p_buf_offset = 0;
			*p_conv_len = (int)(conv_size - out_bytes);

			break;
		}
	}

	return 0;
}

int io_conv_init(const char *charset)
{
	char tocode[CHARSET_MAX_LEN + 20];

	if (charset == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	io_conv_cleanup();

	strncpy(stdio_charset, charset, sizeof(stdio_charset) - 1);
	stdio_charset[sizeof(stdio_charset) - 1] = '\0';

	snprintf(tocode, sizeof(tocode), "%s%s", BBS_default_charset,
			 (strcasecmp(stdio_charset, BBS_default_charset) == 0 ? "" : "//IGNORE"));
	stdin_cd = iconv_open(tocode, stdio_charset);
	if (stdin_cd == (iconv_t)(-1))
	{
		log_error("iconv_open(%s->%s) error: %d\n", stdio_charset, tocode, errno);
		return -2;
	}

	snprintf(tocode, sizeof(tocode), "%s%s", stdio_charset,
			 (strcasecmp(BBS_default_charset, stdio_charset) == 0 ? "" : "//TRANSLIT"));
	stdout_cd = iconv_open(tocode, BBS_default_charset);
	if (stdout_cd == (iconv_t)(-1))
	{
		log_error("iconv_open(%s->%s) error: %d\n", BBS_default_charset, tocode, errno);
		iconv_close(stdin_cd);
		return -2;
	}

	return 0;
}

int io_conv_cleanup(void)
{
	if (stdin_cd != NULL)
	{
		iconv_close(stdin_cd);
		stdin_cd = NULL;
	}
	if (stdout_cd != NULL)
	{
		iconv_close(stdout_cd);
		stdout_cd = NULL;
	}

	return 0;
}
