/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bbs_net
 *   - user interactive feature of site shuttle
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bbs.h"
#include "bbs_net.h"
#include "common.h"
#include "io.h"
#include "log.h"
#include "login.h"
#include "menu.h"
#include "screen.h"
#include "str_process.h"
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
#include <ctype.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#ifdef HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#else
#include <poll.h>
#endif

static const char MENU_CONF_DELIM[] = " \t\r\n";

enum _bbs_net_constant_t
{
	MAX_PROCESS_BAR_LEN = 30,
	MAXSTATION = 26 * 2,
	STATION_PER_LINE = 4,
	USERNAME_MAX_LEN = 20,
	PASSWORD_MAX_LEN = 20,
};

struct _bbsnet_conf
{
	char host1[20];
	char host2[40];
	char ip[40];
	in_port_t port;
	int8_t use_ssh;
	char charset[CHARSET_MAX_LEN + 1];
} bbsnet_conf[MAXSTATION];

static MENU_SET bbsnet_menu;

static int load_bbsnet_conf(const char *file_config)
{
	FILE *fp;
	MENU *p_menu;
	MENU_ITEM *p_menu_item;
	MENU_ITEM_ID menu_item_id;
	char t[256], *t1, *t2, *t3, *t4, *t5, *t6, *saveptr;

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
		t6 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);

		if (t1 == NULL || t2 == NULL || t3 == NULL || t4 == NULL ||
			t5 == NULL || t6 == NULL || t[0] == '#' || t[0] == '*')
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
		bbsnet_conf[menu_item_id].use_ssh = (toupper(t5[0]) == 'Y');
		strncpy(bbsnet_conf[menu_item_id].charset, t6, sizeof(bbsnet_conf[menu_item_id].charset) - 1);
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

static void unload_bbsnet_conf(void)
{
	bbsnet_menu.menu_count = 0;
	bbsnet_menu.menu_item_count = 0;

	free(bbsnet_menu.p_menu_pool);
	bbsnet_menu.p_menu_pool = NULL;
	free(bbsnet_menu.p_menu_item_pool);
	bbsnet_menu.p_menu_item_pool = NULL;
}

static void process_bar(int n, int len)
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

	moveto(4, 1);
	prints(" ------------------------------ \r\n");
	snprintf(buf, sizeof(buf), "            %3d%%              ", n * 100 / len);
	memcpy(buf2, buf, (size_t)n);
	buf2[n] = '\0';
	prints("|\033[46m%s\033[44m%s\033[m|\r\n", buf2, buf + n);
	prints(" ------------------------------ \r\n");
	iflush();
}

static int bbsnet_connect(int n)
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
	char input_conv[LINE_BUFFER_LEN * 2];
	char output_conv[LINE_BUFFER_LEN * 2];
	int input_conv_len = 0;
	int output_conv_len = 0;
	int input_conv_offset = 0;
	int output_conv_offset = 0;
	iconv_t input_cd = NULL;
	iconv_t output_cd = NULL;
	char tocode[32];

#ifdef HAVE_SYS_EPOLL_H
	struct epoll_event ev, events[MAX_EVENTS];
	int epollfd;
#else
	struct pollfd pfds[3];
#endif

	int nfds;
	int stdin_read_wait = 0;
	int stdout_write_wait = 0;
	int sock_read_wait = 0;
	int sock_write_wait = 0;
	struct hostent *p_host = NULL;
	int tos;
	char remote_addr[IP_ADDR_LEN];
	int remote_port;
	char local_addr[IP_ADDR_LEN];
	int local_port;
	socklen_t sock_len;
	time_t t_used = time(NULL);
	struct tm *tm_used;
	int ch;
	char remote_user[USERNAME_MAX_LEN + 1];
	char remote_pass[PASSWORD_MAX_LEN + 1];
	ssh_session session = NULL;
	ssh_channel channel = NULL;
	int ssh_process_config = 0;
	int ssh_log_level = SSH_LOG_PROTOCOL;

	if (user_online_update("BBS_NET") < 0)
	{
		log_error("user_online_update(BBS_NET) error\n");
	}

	if (bbsnet_conf[n].use_ssh)
	{
		clearscr();

		if (!SSH_v2)
		{
			moveto(1, 1);
			prints("只有在以SSH方式登陆本站时，才能使用SSH站点穿梭。");
			press_any_key();
			return 0;
		}

		moveto(1, 1);
		prints("通过SSH方式连接[%s]...", bbsnet_conf[n].host1);
		moveto(2, 1);
		prints("请输入用户名: ");
		iflush();
		if (str_input(remote_user, sizeof(remote_user), DOECHO) < 0)
		{
			return -1;
		}
		if (remote_user[0] == '\0')
		{
			return 0;
		}

		moveto(3, 1);
		prints("请输入密码: ");
		iflush();
		if (str_input(remote_pass, sizeof(remote_pass), NOECHO) < 0)
		{
			return -1;
		}
		if (remote_pass[0] == '\0')
		{
			return 0;
		}
	}

	clearscr();

	moveto(1, 1);
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

#ifdef HAVE_SYS_EPOLL_H
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
#endif

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
#ifdef HAVE_SYS_EPOLL_H
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, 500); // 0.5 second
		ret = nfds;
#else
		pfds[0].fd = sock;
		pfds[0].events = POLLOUT;
		pfds[1].fd = STDIN_FILENO;
		pfds[1].events = POLLIN;
		nfds = 2;
		ret = poll(pfds, (nfds_t)nfds, 500); // 0.5 second
#endif

		if (ret < 0)
		{
			if (errno != EINTR)
			{
#ifdef HAVE_SYS_EPOLL_H
				log_error("epoll_wait() error (%d)\n", errno);
#else
				log_error("poll() error (%d)\n", errno);
#endif
				break;
			}
		}
		else if (ret == 0) // timeout
		{
			process_bar(j + 1, MAX_PROCESS_BAR_LEN);
		}
		else // ret > 0
		{
			for (int i = 0; i < nfds; i++)
			{
#ifdef HAVE_SYS_EPOLL_H
				if (events[i].data.fd == sock)
#else
				if (pfds[i].fd == sock && (pfds[i].revents & POLLOUT))
#endif
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
#ifdef HAVE_SYS_EPOLL_H
				else if (events[i].data.fd == STDIN_FILENO)
#else
				else if (pfds[i].fd == STDIN_FILENO && (pfds[i].revents & POLLIN))
#endif
				{
					do
					{
						ch = igetch(0);
					} while (ch == 0);
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

	sock_len = sizeof(sin);
	if (getsockname(sock, (struct sockaddr *)&sin, &sock_len) < 0)
	{
		log_error("getsockname() error: %d", errno);
		goto cleanup;
	}

	strncpy(local_addr, inet_ntoa(sin.sin_addr), sizeof(local_addr) - 1);
	local_addr[sizeof(local_addr) - 1] = '\0';
	local_port = ntohs(sin.sin_port);

	if (bbsnet_conf[n].use_ssh)
	{
		session = ssh_new();
		if (session == NULL)
		{
			log_error("ssh_new() error\n");
			goto cleanup;
		}

		if (ssh_options_set(session, SSH_OPTIONS_FD, &sock) < 0 ||
			ssh_options_set(session, SSH_OPTIONS_PROCESS_CONFIG, &ssh_process_config) < 0 ||
			ssh_options_set(session, SSH_OPTIONS_HOST, bbsnet_conf[n].ip) < 0 ||
			ssh_options_set(session, SSH_OPTIONS_USER, remote_user) < 0 ||
			ssh_options_set(session, SSH_OPTIONS_HOSTKEYS, "+ssh-rsa") < 0 ||
			ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &ssh_log_level) < 0)
		{
			log_error("Error setting SSH options: %s\n", ssh_get_error(session));
			goto cleanup;
		}

		while (!SYS_server_exit)
		{
			ret = ssh_connect(session);
			if (ret == SSH_OK)
			{
				break;
			}
			else if (ret == SSH_ERROR)
			{
				log_error("ssh_connect() error\n");
				goto cleanup;
			}
		}

		ssh_set_blocking(session, 0);

		for (int i = 0; !SYS_server_exit;)
		{
			ret = ssh_userauth_password(session, NULL, remote_pass);
			if (ret == SSH_AUTH_SUCCESS)
			{
				break;
			}
			else if (ret == SSH_AUTH_AGAIN)
			{
#ifdef _DEBUG
				log_error("ssh_userauth_password() error: SSH_AUTH_AGAIN\n");
#endif
			}
			else if (ret == SSH_AUTH_ERROR)
			{
				log_error("ssh_userauth_password() error: %d\n", ret);
				goto cleanup;
			}
			else // if (ret == SSH_AUTH_DENIED)
			{
				prints("\033[1;31m身份验证失败！\033[m\r\n");
				i++;
				if (i < BBS_login_retry_times)
				{
					prints("请输入密码: ");
					iflush();
					if (str_input(remote_pass, sizeof(remote_pass), NOECHO) < 0)
					{
						goto cleanup;
					}
					if (remote_pass[0] == '\0')
					{
						goto cleanup;
					}
				}
				else
				{
					goto cleanup;
				}
			}
		}

		channel = ssh_channel_new(session);
		if (channel == NULL)
		{
			log_error("ssh_channel_new() error\n");
			goto cleanup;
		}

		while (!SYS_server_exit)
		{
			ret = ssh_channel_open_session(channel);
			if (ret == SSH_OK)
			{
				break;
			}
			else if (ret == SSH_ERROR)
			{
				log_error("ssh_channel_open_session() error\n");
				goto cleanup;
			}
		}

		while (!SYS_server_exit)
		{
			ret = ssh_channel_request_pty(channel);
			if (ret == SSH_OK)
			{
				break;
			}
			else if (ret == SSH_ERROR)
			{
				log_error("ssh_channel_request_pty() error\n");
				goto cleanup;
			}
		}

		while (!SYS_server_exit)
		{
			ret = ssh_channel_request_shell(channel);
			if (ret == SSH_OK)
			{
				break;
			}
			else if (ret == SSH_ERROR)
			{
				log_error("ssh_channel_request_shell() error\n");
				goto cleanup;
			}
		}
	}

	prints("\033[1;31m连接成功！\033[m\r\n");
	iflush();
	log_common("BBSNET connect to %s:%d from %s:%d by [%s]\n",
			   remote_addr, remote_port, local_addr, local_port, BBS_username);

	snprintf(tocode, sizeof(tocode), "%s%s", bbsnet_conf[n].charset,
			 (strcasecmp(stdio_charset, bbsnet_conf[n].charset) == 0 ? "" : "//IGNORE"));
	input_cd = iconv_open(tocode, stdio_charset);
	if (input_cd == (iconv_t)(-1))
	{
		log_error("iconv_open(%s->%s) error: %d\n", stdio_charset, tocode, errno);
		goto cleanup;
	}

	snprintf(tocode, sizeof(tocode), "%s%s", stdio_charset,
			 (strcasecmp(bbsnet_conf[n].charset, stdio_charset) == 0 ? "" : "//TRANSLIT"));
	output_cd = iconv_open(tocode, bbsnet_conf[n].charset);
	if (output_cd == (iconv_t)(-1))
	{
		log_error("iconv_open(%s->%s) error: %d\n", bbsnet_conf[n].charset, tocode, errno);
		iconv_close(input_cd);
		goto cleanup;
	}

#ifdef HAVE_SYS_EPOLL_H
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
#endif

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

		if (bbsnet_conf[n].use_ssh && ssh_channel_is_closed(channel))
		{
			log_error("Remote SSH channel is closed\n");
			loop = 0;
			break;
		}

#ifdef HAVE_SYS_EPOLL_H
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, 100); // 0.1 second
		ret = nfds;
#else
		pfds[0].fd = STDIN_FILENO;
		pfds[0].events = POLLIN;
		pfds[1].fd = sock;
		pfds[1].events = POLLIN | POLLOUT;
		pfds[2].fd = STDOUT_FILENO;
		pfds[2].events = POLLOUT;
		nfds = 3;
		ret = poll(pfds, (nfds_t)nfds, 100); // 0.1 second
#endif

		if (ret < 0)
		{
			if (errno != EINTR)
			{
#ifdef HAVE_SYS_EPOLL_H
				log_error("epoll_wait() error (%d)\n", errno);
#else
				log_error("poll() error (%d)\n", errno);
#endif
				break;
			}
			continue;
		}
		else if (ret == 0) // timeout
		{
			if (time(NULL) - BBS_last_access_tm >= BBS_max_user_idle_time)
			{
				break;
			}
		}

		for (int i = 0; i < nfds; i++)
		{
#ifdef HAVE_SYS_EPOLL_H
			if (events[i].data.fd == STDIN_FILENO)
#else
			if (pfds[i].fd == STDIN_FILENO && (pfds[i].revents & POLLIN))
#endif
			{
				stdin_read_wait = 1;
			}

#ifdef HAVE_SYS_EPOLL_H
			if (events[i].data.fd == sock)
#else
			if (pfds[i].fd == sock)
#endif
			{
#ifdef HAVE_SYS_EPOLL_H
				if (events[i].events & EPOLLIN)
#else
				if (pfds[i].revents & POLLIN)
#endif
				{
					sock_read_wait = 1;
				}

#ifdef HAVE_SYS_EPOLL_H
				if (events[i].events & EPOLLOUT)
#else
				if (pfds[i].revents & POLLOUT)
#endif
				{
					sock_write_wait = 1;
				}
			}

#ifdef HAVE_SYS_EPOLL_H
			if (events[i].data.fd == STDOUT_FILENO)
#else
			if (pfds[i].fd == STDOUT_FILENO && (pfds[i].revents & POLLOUT))
#endif
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
						// Send NO-OP to remote server
						input_buf[input_buf_len] = '\0';
						input_buf_len++;
						BBS_last_access_tm = time(NULL);

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
				// For debug
#ifdef _DEBUG
				for (int j = input_buf_offset; j < input_buf_len; j++)
				{
					log_error("Debug input: <--[%u]\n", (input_buf[j] + 256) % 256);
				}
#endif

				ret = io_buf_conv(input_cd, input_buf, &input_buf_len, &input_buf_offset, input_conv, sizeof(input_conv), &input_conv_len);
				if (ret < 0)
				{
					log_error("io_buf_conv(input, %d, %d, %d) error\n", input_buf_len, input_buf_offset, input_conv_len);
					input_buf_len = input_buf_offset; // Discard invalid sequence
				}

				// For debug
#ifdef _DEBUG
				for (int j = input_conv_offset; j < input_conv_len; j++)
				{
					log_error("Debug input_conv: <--[%u]\n", (input_conv[j] + 256) % 256);
				}
#endif
			}

			while (input_conv_offset < input_conv_len && !SYS_server_exit)
			{
				if (bbsnet_conf[n].use_ssh)
				{
					ret = ssh_channel_write(channel, input_conv + input_conv_offset, (uint32_t)(input_conv_len - input_conv_offset));
					if (ret == SSH_ERROR)
					{
						log_error("ssh_channel_write() error: %s\n", ssh_get_error(session));
						loop = 0;
						break;
					}
				}
				else
				{
					ret = (int)write(sock, input_conv + input_conv_offset, (size_t)(input_conv_len - input_conv_offset));
				}
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
				if (bbsnet_conf[n].use_ssh)
				{
					ret = ssh_channel_read_nonblocking(channel, output_buf + output_buf_len,
													   (uint32_t)(sizeof(output_buf) - (size_t)output_buf_len), 0);
					if (ret == SSH_ERROR)
					{
						log_error("ssh_channel_read_nonblocking() error: %s\n", ssh_get_error(session));
						loop = 0;
						break;
					}
					else if (ret == SSH_EOF)
					{
						sock_read_wait = 0;
						loop = 0;
						break;
					}
					else if (ret == 0)
					{
						sock_read_wait = 0;
						break;
					}
				}
				else
				{
					ret = (int)read(sock, output_buf + output_buf_len, sizeof(output_buf) - (size_t)output_buf_len);
				}
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
				ret = io_buf_conv(output_cd, output_buf, &output_buf_len, &output_buf_offset, output_conv, sizeof(output_conv), &output_conv_len);
				if (ret < 0)
				{
					log_error("io_buf_conv(output, %d, %d, %d) error\n", output_buf_len, output_buf_offset, output_conv_len);
					output_buf_len = output_buf_offset; // Discard invalid sequence
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
#ifdef HAVE_SYS_EPOLL_H
	if (close(epollfd) < 0)
	{
		log_error("close(epoll) error (%d)\n");
	}
#endif

	if (bbsnet_conf[n].use_ssh)
	{
		ssh_channel_free(channel);
		ssh_disconnect(session);
		ssh_free(session);
	}

	// Restore STDIN/STDOUT flags
	fcntl(STDIN_FILENO, F_SETFL, flags_stdin);
	fcntl(STDOUT_FILENO, F_SETFL, flags_stdout);

	if (close(sock) == -1)
	{
		log_error("Close socket failed\n");
	}

	t_used = time(NULL) - t_used;
	tm_used = gmtime(&t_used);

	log_common("BBSNET disconnect, %d days %d hours %d minutes %d seconds used\n",
			   tm_used->tm_mday - 1, tm_used->tm_hour, tm_used->tm_min,
			   tm_used->tm_sec);

	BBS_last_access_tm = time(NULL);

	return 0;
}

static int bbsnet_refresh()
{
	clearscr();

	moveto(1, 1);
	prints(" ------------------------------------------------------------------------------ ");
	for (int i = 2; i < 19; i++)
	{
		moveto(i, 1);
		prints("|");
		moveto(i, 80);
		prints("|");
	}
	moveto(19, 1);
	prints("|------------------------------------------------------------------------------|");
	moveto(22, 1);
	prints(" ------------------------------------------------------------------------------ ");
	moveto(23, 1);
	prints(" [\033[1;32mCtrl+C\033[m]退出");

	iflush();

	return 0;
}

static int bbsnet_selchange()
{
	int i = bbsnet_menu.menu_item_pos[0];

	moveto(20, 1);
	clrtoeol();
	prints("|\033[1m单位: \033[1;33m%s\033[m%*s  站名: \033[1;33m%s\033[m",
		   bbsnet_conf[i].host2, 20 - str_length(bbsnet_conf[i].host2, 1), "", bbsnet_conf[i].host1);
	moveto(20, 80);
	prints("|");
	moveto(21, 1);
	clrtoeol();
	prints("|\033[1m连往: \033[1;33m%-20s\033[m  端口: \033[1;33m%-5d\033[m            类型: \033[1;33m%s\033[m",
		   bbsnet_conf[i].ip, bbsnet_conf[i].port, (bbsnet_conf[i].use_ssh ? "SSH" : "Telnet"));
	moveto(21, 80);
	prints("|");
	iflush();

	return 0;
}

int bbs_net()
{
	int ch, i;

	load_bbsnet_conf(CONF_BBSNET);

	bbsnet_refresh();
	display_menu(&bbsnet_menu);
	bbsnet_selchange();

	while (!SYS_server_exit)
	{
		ch = igetch(100);

		if (ch != KEY_NULL && ch != KEY_TIMEOUT)
		{
			BBS_last_access_tm = time(NULL);
		}

		switch (ch)
		{
		case KEY_NULL: // broken pipe
			log_error("KEY_NULL\n");
			goto cleanup;
		case KEY_TIMEOUT:
			if (time(NULL) - BBS_last_access_tm >= BBS_max_user_idle_time)
			{
				log_error("User input timeout\n");
				goto cleanup;
			}
			continue;
		case KEY_ESC:
		case Ctrl('C'): // user cancel
			goto cleanup;
		case CR:
			bbsnet_connect(bbsnet_menu.menu_item_pos[0]);
			// Force cleanup anything remaining in the output buffer
			clearscr();
			iflush();
			// Clear screen and redraw menu
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
	}

cleanup:
	unload_bbsnet_conf();

	return 0;
}
