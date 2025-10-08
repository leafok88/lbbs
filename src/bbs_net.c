/***************************************************************************
						  bbs_net.c  -  description
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
#include "io.h"
#include "log.h"
#include "login.h"
#include "menu.h"
#include "screen.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iconv.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define MENU_CONF_DELIM " \t\r\n"

#define MAX_PROCESS_BAR_LEN 30
#define MAXSTATION 26 * 2
#define STATION_PER_LINE 4

#define BBS_NET_DEFAULT_CHARSET "UTF-8"

struct _bbsnet_conf
{
	char host1[20];
	char host2[40];
	char ip[40];
	in_port_t port;
	char charset[20];
} bbsnet_conf[MAXSTATION];

MENU_SET bbsnet_menu;

int load_bbsnet_conf(const char *file_config)
{
	FILE *fp;
	MENU *p_menu;
	MENU_ITEM *p_menu_item;
	MENU_ITEM_ID menu_item_id;
	char t[256], *t1, *t2, *t3, *t4, *t5, *saveptr;

	fp = fopen(file_config, "r");
	if (fp == NULL)
	{
		return -1;
	}

	bbsnet_menu.p_menu_pool = calloc(1, sizeof(MENU));
	if (bbsnet_menu.p_menu_pool == NULL)
	{
		log_error("calloc(p_menu_pool) error\n");
		return -3;
	}
	bbsnet_menu.menu_count = 1;

	bbsnet_menu.p_menu_item_pool = calloc(MAXSTATION, sizeof(MENU_ITEM));
	if (bbsnet_menu.p_menu_item_pool == NULL)
	{
		log_error("calloc(p_menu_item_pool) error\n");
		return -3;
	}
	bbsnet_menu.menu_item_count = MAXSTATION;

	p_menu = (MENU *)get_menu_by_id(&bbsnet_menu, 0);

	strncpy(p_menu->name, "BBSNET", sizeof(p_menu->name) - 1);
	p_menu->name[sizeof(p_menu->name) - 1] = '\0';
	p_menu->title.show = 0;
	p_menu->screen_show = 0;

	menu_item_id = 0;
	while (fgets(t, 255, fp) && menu_item_id < MAXSTATION)
	{
		t1 = strtok_r(t, MENU_CONF_DELIM, &saveptr);
		t2 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);
		t3 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);
		t4 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);
		t5 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);

		if (t1 == NULL || t2 == NULL || t3 == NULL || t4 == NULL || t5 == NULL || t[0] == '#' || t[0] == '*')
		{
			continue;
		}

		strncpy(bbsnet_conf[menu_item_id].host1, t2, sizeof(bbsnet_conf[menu_item_id].host1) - 1);
		bbsnet_conf[menu_item_id].host1[sizeof(bbsnet_conf[menu_item_id].host1) - 1] = '\0';
		strncpy(bbsnet_conf[menu_item_id].host2, t1, sizeof(bbsnet_conf[menu_item_id].host2) - 1);
		bbsnet_conf[menu_item_id].host2[sizeof(bbsnet_conf[menu_item_id].host2) - 1] = '\0';
		strncpy(bbsnet_conf[menu_item_id].ip, t3, sizeof(bbsnet_conf[menu_item_id].ip) - 1);
		bbsnet_conf[menu_item_id].ip[sizeof(bbsnet_conf[menu_item_id].ip) - 1] = '\0';
		bbsnet_conf[menu_item_id].port = (in_port_t)(t4 ? atoi(t4) : 23);
		strncpy(bbsnet_conf[menu_item_id].charset, t5, sizeof(bbsnet_conf[menu_item_id].charset) - 1);
		bbsnet_conf[menu_item_id].charset[sizeof(bbsnet_conf[menu_item_id].charset) - 1] = '\0';

		p_menu_item = get_menu_item_by_id(&bbsnet_menu, menu_item_id);
		if (p_menu_item == NULL)
		{
			log_error("get_menu_item_by_id(%d) return NULL pointer\n", menu_item_id);
			return -1;
		}

		p_menu_item->row = (int16_t)(2 + menu_item_id / STATION_PER_LINE);
		p_menu_item->col = (int16_t)(5 + menu_item_id % STATION_PER_LINE * 20);
		snprintf(p_menu_item->action, sizeof(p_menu_item->action), "%d", (int16_t)menu_item_id);
		p_menu_item->submenu = 0;
		p_menu_item->priv = 0;
		p_menu_item->level = 0;
		p_menu_item->name[0] =
			(char)(menu_item_id < MAXSTATION / 2 ? 'A' + menu_item_id : 'a' + menu_item_id);
		p_menu_item->name[1] = '\0';
		snprintf(p_menu_item->text, sizeof(p_menu_item->text), "[1;36m%c.[m %s",
				 p_menu_item->name[0], bbsnet_conf[menu_item_id].host1);

		p_menu->items[p_menu->item_count] = menu_item_id;
		p_menu->item_count++;
		menu_item_id++;
	}

	bbsnet_menu.menu_item_count = (int16_t)menu_item_id;
	bbsnet_menu.menu_id_path[0] = 0;
	bbsnet_menu.menu_item_pos[0] = 0;
	bbsnet_menu.choose_step = 0;

	fclose(fp);

	return 0;
}

void unload_bbsnet_conf(void)
{
	bbsnet_menu.menu_count = 0;
	bbsnet_menu.menu_item_count = 0;

	free(bbsnet_menu.p_menu_pool);
	bbsnet_menu.p_menu_pool = NULL;
	free(bbsnet_menu.p_menu_item_pool);
	bbsnet_menu.p_menu_item_pool = NULL;
}

void process_bar(int n, int len)
{
	char buf[LINE_BUFFER_LEN];
	char buf2[LINE_BUFFER_LEN];

	if (len > LINE_BUFFER_LEN)
	{
		len = LINE_BUFFER_LEN - 1;
	}
	if (n < 0)
	{
		n = 0;
	}
	else if (n > len)
	{
		n = len;
	}

	moveto(4, 0);
	prints(" ------------------------------ \r\n");
	snprintf(buf, sizeof(buf), "            %3d%%              ", n * 100 / len);
	strncpy(buf2, buf, (size_t)n);
	buf2[n] = '\0';
	prints("|\033[46m%s\033[44m%s\033[m|\r\n", buf2, buf + n);
	prints(" ------------------------------ \r\n");
	iflush();
}

int bbsnet_io_buf_conv(iconv_t cd, char *p_buf, int *p_buf_len, int *p_buf_offset, char *p_conv, size_t conv_size, int *p_conv_len)
{
	char *in_buf;
	char *out_buf;
	size_t in_bytes;
	size_t out_bytes;
	int ret;

	in_buf = p_buf + *p_buf_offset;
	in_bytes = (size_t)(*p_buf_len - *p_buf_offset);
	out_buf = p_conv + *p_conv_len;
	out_bytes = conv_size - (size_t)(*p_conv_len);

	while (in_bytes > 0)
	{
		ret = (int)iconv(cd, &in_buf, &in_bytes, &out_buf, &out_bytes);
		if (ret == -1)
		{
			if (errno == EINVAL) // Incomplete
			{
#ifdef _DEBUG
				log_error("iconv(inbytes=%d, outbytes=%d) error: EINVAL\n", in_bytes, out_bytes);
#endif
				*p_buf_len = (int)(p_buf + *p_buf_len - in_buf);
				*p_buf_offset = 0;
				*p_conv_len = (int)(conv_size - out_bytes);
				memmove(p_buf, in_buf, (size_t)(*p_buf_len));

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

int bbsnet_connect(int n)
{
	int sock, ret, loop, error;
	int sock_connected = 0;
	int flags_sock;
	int flags_stdin;
	int flags_stdout;
	int len;
	struct sockaddr_in sin;
	char input_buf[LINE_BUFFER_LEN];
	char output_buf[LINE_BUFFER_LEN];
	int input_buf_len = 0;
	int output_buf_len = 0;
	int input_buf_offset = 0;
	int output_buf_offset = 0;
	iconv_t input_cd = NULL;
	char input_conv[LINE_BUFFER_LEN * 2];
	char output_conv[LINE_BUFFER_LEN * 2];
	int input_conv_len = 0;
	int output_conv_len = 0;
	int input_conv_offset = 0;
	int output_conv_offset = 0;
	iconv_t output_cd = NULL;
	struct epoll_event ev, events[MAX_EVENTS];
	int nfds, epollfd;
	int stdin_read_wait = 0;
	int stdout_write_wait = 0;
	int sock_read_wait = 0;
	int sock_write_wait = 0;
	struct hostent *p_host = NULL;
	int tos;
	char remote_addr[IP_ADDR_LEN];
	int remote_port;
	time_t t_used = time(NULL);
	struct tm *tm_used;
	int ch;

	if (user_online_update("BBS_NET") < 0)
	{
		log_error("user_online_update(BBS_NET) error\n");
	}

	clearscr();

	moveto(0, 0);
	prints("\033[1;32m正在测试往 %s (%s) 的连接，请稍候... \033[m\r\n",
		   bbsnet_conf[n].host1, bbsnet_conf[n].ip);
	iflush();

	p_host = gethostbyname(bbsnet_conf[n].ip);

	if (p_host == NULL)
	{
		prints("\033[1;31m查找主机名失败！\033[m\r\n");
		press_any_key();
		return -1;
	}

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock < 0)
	{
		prints("\033[1;31m无法创建socket！\033[m\r\n");
		press_any_key();
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = (BBS_address[0] != '\0' ? inet_addr(BBS_address) : INADDR_ANY);
	sin.sin_port = 0;

	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		log_error("Bind address %s:%u failed (%d)\n",
				  inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), errno);
		return -2;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr = *(struct in_addr *)p_host->h_addr_list[0];
	sin.sin_port = htons(bbsnet_conf[n].port);

	strncpy(remote_addr, inet_ntoa(sin.sin_addr), sizeof(remote_addr) - 1);
	remote_addr[sizeof(remote_addr) - 1] = '\0';
	remote_port = ntohs(sin.sin_port);

	prints("\033[1;32m穿梭进度条提示您当前已使用的时间，按\033[1;33mCtrl+C\033[1;32m中断。\033[m\r\n");
	process_bar(0, MAX_PROCESS_BAR_LEN);

	// Set socket as non-blocking
	flags_sock = fcntl(sock, F_GETFL, 0);
	fcntl(sock, F_SETFL, flags_sock | O_NONBLOCK);

	// Set STDIN/STDOUT as non-blocking
	flags_stdin = fcntl(STDIN_FILENO, F_GETFL, 0);
	flags_stdout = fcntl(STDOUT_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags_stdin | O_NONBLOCK);
	fcntl(STDOUT_FILENO, F_SETFL, flags_stdout | O_NONBLOCK);

	epollfd = epoll_create1(0);
	if (epollfd < 0)
	{
		log_error("epoll_create1() error (%d)\n", errno);
		return -1;
	}

	ev.events = EPOLLOUT | EPOLLET;
	ev.data.fd = sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) == -1)
	{
		log_error("epoll_ctl(socket) error (%d)\n", errno);
		goto cleanup;
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = STDIN_FILENO;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1)
	{
		log_error("epoll_ctl(STDIN_FILENO) error (%d)\n", errno);
		goto cleanup;
	}

	while (!SYS_server_exit)
	{
		if ((ret = connect(sock, (struct sockaddr *)&sin, sizeof(sin))) < 0)
		{
			if (errno == EAGAIN || errno == EALREADY || errno == EINPROGRESS)
			{
				// Use select / epoll to check writability of the socket,
				// then use getsockopt to check the status of the socket.
				// See man connect(2)
				break;
			}
			else if (errno == EINTR)
			{
				continue;
			}
			else
			{
				log_error("connect(socket) error (%d)\n", errno);

				prints("\033[1;31m连接失败！\033[m\r\n");
				press_any_key();

				goto cleanup;
			}
		}
	}

	for (int j = 0; j < MAX_PROCESS_BAR_LEN && !sock_connected && !SYS_server_exit; j++)
	{
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, 500); // 0.5 second

		if (nfds < 0)
		{
			if (errno != EINTR)
			{
				log_error("epoll_wait() error (%d)\n", errno);
				break;
			}
		}
		else if (nfds == 0) // timeout
		{
			process_bar(j + 1, MAX_PROCESS_BAR_LEN);
		}
		else // ret > 0
		{
			for (int i = 0; i < nfds; i++)
			{
				if (events[i].data.fd == sock)
				{
					len = sizeof(error);
					if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len) < 0)
					{
						log_error("getsockopt() error (%d) !\n", error);
						goto cleanup;
					}
					if (error == 0)
					{
						sock_connected = 1;
					}
				}
				else if (events[i].data.fd == STDIN_FILENO)
				{
					ch = igetch(0);
					if (ch == Ctrl('C') || ch == KEY_ESC)
					{
						goto cleanup;
					}
				}
			}
		}
	}
	if (SYS_server_exit)
	{
		goto cleanup;
	}
	if (!sock_connected)
	{
		prints("\033[1;31m连接失败！\033[m\r\n");
		press_any_key();

		goto cleanup;
	}

	tos = IPTOS_LOWDELAY;
	if (setsockopt(sock, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0)
	{
		log_error("setsockopt IP_TOS=%d error (%d)\n", tos, errno);
	}

	prints("\033[1;31m连接成功！\033[m\r\n");
	iflush();
	log_common("BBSNET connect to %s:%d\n", remote_addr, remote_port);

	input_cd = iconv_open(bbsnet_conf[n].charset, BBS_NET_DEFAULT_CHARSET);
	if (input_cd == (iconv_t)(-1))
	{
		log_error("iconv_open(%s->%s) error: %d\n", BBS_NET_DEFAULT_CHARSET, bbsnet_conf[n].charset, errno);
		goto cleanup;
	}
	output_cd = iconv_open(BBS_NET_DEFAULT_CHARSET, bbsnet_conf[n].charset);
	if (input_cd == (iconv_t)(-1))
	{
		log_error("iconv_open(%s->%s) error: %d\n", bbsnet_conf[n].charset, BBS_NET_DEFAULT_CHARSET, errno);
		iconv_close(input_cd);
		goto cleanup;
	}

	ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
	ev.data.fd = sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sock, &ev) == -1)
	{
		log_error("epoll_ctl(socket) error (%d)\n", errno);
		goto cleanup;
	}

	ev.events = EPOLLOUT | EPOLLET;
	ev.data.fd = STDOUT_FILENO;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDOUT_FILENO, &ev) == -1)
	{
		log_error("epoll_ctl(STDOUT_FILENO) error (%d)\n", errno);
		goto cleanup;
	}

	BBS_last_access_tm = t_used = time(NULL);
	loop = 1;

	while (loop && !SYS_server_exit)
	{
		if (SSH_v2 && ssh_channel_is_closed(SSH_channel))
		{
			log_error("SSH channel is closed\n");
			loop = 0;
			break;
		}

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
			if (time(NULL) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				break;
			}
		}

		for (int i = 0; i < nfds; i++)
		{
			if (events[i].data.fd == STDIN_FILENO)
			{
				stdin_read_wait = 1;
			}

			if (events[i].data.fd == sock)
			{
				if (events[i].events & EPOLLIN)
				{
					sock_read_wait = 1;
				}
				if (events[i].events & EPOLLOUT)
				{
					sock_write_wait = 1;
				}
			}

			if (events[i].data.fd == STDOUT_FILENO)
			{
				stdout_write_wait = 1;
			}
		}

		if (stdin_read_wait)
		{
			while (input_buf_len < sizeof(input_buf) && !SYS_server_exit)
			{
				if (SSH_v2)
				{
					ret = ssh_channel_read_nonblocking(SSH_channel, input_buf + input_buf_len, sizeof(input_buf) - (uint32_t)input_buf_len, 0);
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
						stdin_read_wait = 0;
						break; // Check whether channel is still open
					}
				}
				else
				{
					ret = (int)read(STDIN_FILENO, input_buf + input_buf_len, sizeof(input_buf) - (size_t)input_buf_len);
				}
				if (ret < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
					{
						stdin_read_wait = 0;
						break;
					}
					else if (errno == EINTR)
					{
						continue;
					}
					else
					{
						log_error("read(STDIN) error (%d)\n", errno);
						loop = 0;
						break;
					}
				}
				else if (ret == 0) // broken pipe
				{
#ifdef _DEBUG
					log_error("read(STDIN) EOF\n");
#endif
					stdin_read_wait = 0;
					loop = 0;
					break;
				}
				else
				{
					input_buf_len += ret;
					BBS_last_access_tm = time(NULL);

					// Refresh current action while user input
					if (user_online_update("BBS_NET") < 0)
					{
						log_error("user_online_update(BBS_NET) error\n");
					}

					continue;
				}
			}
		}

		if (sock_write_wait)
		{
			if (input_buf_offset < input_buf_len)
			{
				ret = bbsnet_io_buf_conv(input_cd, input_buf, &input_buf_len, &input_buf_offset, input_conv, sizeof(input_conv), &input_conv_len);
				if (ret < 0)
				{
					log_error("bbsnet_io_buf_conv(input, %d, %d, %d) error\n", input_buf_len, input_buf_offset, input_conv_len);
				}
			}

			while (input_conv_offset < input_conv_len && !SYS_server_exit)
			{
				ret = (int)write(sock, input_conv + input_conv_offset, (size_t)(input_conv_len - input_conv_offset));
				if (ret < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
					{
						sock_write_wait = 0;
						break;
					}
					else if (errno == EINTR)
					{
						continue;
					}
					else
					{
						log_error("write(socket) error (%d)\n", errno);
						loop = 0;
						break;
					}
				}
				else if (ret == 0) // broken pipe
				{
#ifdef _DEBUG
					log_error("write(socket) EOF\n");
#endif
					sock_write_wait = 0;
					loop = 0;
					break;
				}
				else
				{
					input_conv_offset += ret;
					if (input_conv_offset >= input_conv_len) // Output buffer complete
					{
						input_conv_offset = 0;
						input_conv_len = 0;
						break;
					}
					continue;
				}
			}
		}

		if (sock_read_wait)
		{
			while (output_buf_len < sizeof(output_buf) && !SYS_server_exit)
			{
				ret = (int)read(sock, output_buf + output_buf_len, sizeof(output_buf) - (size_t)output_buf_len);
				if (ret < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
					{
						sock_read_wait = 0;
						break;
					}
					else if (errno == EINTR)
					{
						continue;
					}
					else
					{
						log_error("read(socket) error (%d)\n", errno);
						loop = 0;
						break;
					}
				}
				else if (ret == 0) // broken pipe
				{
#ifdef _DEBUG
					log_error("read(socket) EOF\n");
#endif
					sock_read_wait = 0;
					loop = 0;
					break;
				}
				else
				{
					output_buf_len += ret;
					continue;
				}
			}
		}

		if (stdout_write_wait)
		{
			if (output_buf_offset < output_buf_len)
			{
				ret = bbsnet_io_buf_conv(output_cd, output_buf, &output_buf_len, &output_buf_offset, output_conv, sizeof(output_conv), &output_conv_len);
				if (ret < 0)
				{
					log_error("bbsnet_io_buf_conv(output, %d, %d, %d) error\n", output_buf_len, output_buf_offset, output_conv_len);
				}
			}

			while (output_conv_offset < output_conv_len && !SYS_server_exit)
			{
				if (SSH_v2)
				{
					ret = ssh_channel_write(SSH_channel, output_conv + output_conv_offset, (uint32_t)(output_conv_len - output_conv_offset));
					if (ret == SSH_ERROR)
					{
						log_error("ssh_channel_write() error: %s\n", ssh_get_error(SSH_session));
						loop = 0;
						break;
					}
				}
				else
				{
					ret = (int)write(STDOUT_FILENO, output_conv + output_conv_offset, (size_t)(output_conv_len - output_conv_offset));
				}
				if (ret < 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
					{
						stdout_write_wait = 0;
						break;
					}
					else if (errno == EINTR)
					{
						continue;
					}
					else
					{
						log_error("write(STDOUT) error (%d)\n", errno);
						loop = 0;
						break;
					}
				}
				else if (ret == 0) // broken pipe
				{
#ifdef _DEBUG
					log_error("write(STDOUT) EOF\n");
#endif
					stdout_write_wait = 0;
					loop = 0;
					break;
				}
				else
				{
					output_conv_offset += ret;
					if (output_conv_offset >= output_conv_len) // Output buffer complete
					{
						output_conv_offset = 0;
						output_conv_len = 0;
						break;
					}
					continue;
				}
			}
		}
	}

	iconv_close(input_cd);
	iconv_close(output_cd);

cleanup:
	if (close(epollfd) < 0)
	{
		log_error("close(epoll) error (%d)\n");
	}

	// Restore STDIN/STDOUT flags
	fcntl(STDIN_FILENO, F_SETFL, flags_stdin);
	fcntl(STDOUT_FILENO, F_SETFL, flags_stdout);

	// Restore socket flags
	fcntl(sock, F_SETFL, flags_sock);

	if (close(sock) == -1)
	{
		log_error("Close socket failed\n");
	}

	t_used = time(NULL) - t_used;
	tm_used = gmtime(&t_used);

	log_common("BBSNET disconnect, %d days %d hours %d minutes %d seconds used\n",
			   tm_used->tm_mday - 1, tm_used->tm_hour, tm_used->tm_min,
			   tm_used->tm_sec);

	return 0;
}

static int
bbsnet_refresh()
{
	clearscr();
	moveto(1, 0);
	prints(" ----------------------------------------------------------------------------- ");
	for (int i = 2; i < 19; i++)
	{
		moveto(i, 0);
		prints("|");
		moveto(i, 79);
		prints("|");
	}
	moveto(19, 0);
	prints("|-----------------------------------------------------------------------------|");
	moveto(22, 0);
	prints(" ----------------------------------------------------------------------------- ");
	moveto(23, 0);
	prints(" [\x1b[1;32mCtrl+C\x1b[m]退出");

	iflush();

	return 0;
}

int bbsnet_selchange()
{
	int i = bbsnet_menu.menu_item_pos[0];

	moveto(20, 0);
	clrtoeol();
	prints("|\x1b[1m单位:\x1b[1;33m%-18s\x1b[m  站名:\x1b[1;33m%s\x1b[m",
		   bbsnet_conf[i].host2, bbsnet_conf[i].host1);
	moveto(20, 79);
	prints("|");
	moveto(21, 0);
	clrtoeol();
	prints("|\x1b[1m连往:\x1b[1;33m%-20s", bbsnet_conf[i].ip);
	if (bbsnet_conf[i].port != 23)
	{
		prints("  %d", bbsnet_conf[i].port);
	}
	prints("\x1b[m");
	moveto(21, 79);
	prints("|");
	iflush();

	return 0;
}

int bbs_net()
{
	int ch, i;

	load_bbsnet_conf(CONF_BBSNET);

	BBS_last_access_tm = time(NULL);

	clearscr();
	bbsnet_refresh();
	display_menu(&bbsnet_menu);
	bbsnet_selchange();

	while (!SYS_server_exit)
	{
		ch = igetch(100);

		switch (ch)
		{
		case KEY_NULL: // broken pipe
		case KEY_ESC:
		case Ctrl('C'): // user cancel
			goto cleanup;
		case KEY_TIMEOUT:
			if (time(NULL) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				goto cleanup;
			}
			continue;
		case CR:
			igetch_reset();
			bbsnet_connect(bbsnet_menu.menu_item_pos[0]);
			bbsnet_refresh();
			display_menu(&bbsnet_menu);
			bbsnet_selchange();
			break;
		case KEY_UP:
			for (i = 0; i < STATION_PER_LINE; i++)
			{
				menu_control(&bbsnet_menu, KEY_UP);
			}
			bbsnet_selchange();
			break;
		case KEY_DOWN:
			for (i = 0; i < STATION_PER_LINE; i++)
			{
				menu_control(&bbsnet_menu, KEY_DOWN);
			}
			bbsnet_selchange();
			break;
		case KEY_LEFT:
			menu_control(&bbsnet_menu, KEY_UP);
			bbsnet_selchange();
			break;
		case KEY_RIGHT:
			menu_control(&bbsnet_menu, KEY_DOWN);
			bbsnet_selchange();
			break;
		case KEY_HOME:
		case KEY_PGUP:
			menu_control(&bbsnet_menu, KEY_PGUP);
			bbsnet_selchange();
			break;
		case KEY_END:
		case KEY_PGDN:
			menu_control(&bbsnet_menu, KEY_PGDN);
			bbsnet_selchange();
			break;
		default:
			menu_control(&bbsnet_menu, ch);
			bbsnet_selchange();
			break;
		}
		BBS_last_access_tm = time(NULL);
	}

cleanup:
	unload_bbsnet_conf();

	return 0;
}
