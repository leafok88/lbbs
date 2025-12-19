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
	MAXSTATION = 26 * 2,
	STATION_PER_LINE = 4,
	USERNAME_MAX_LEN = 20,
	PASSWORD_MAX_LEN = 20,
	REMOTE_CONNECT_TIMEOUT = 10, // seconds
	SSH_CONNECT_TIMEOUT = 5,	 // seconds
	PROGRESS_BAR_LEN = 30,
};

struct _bbsnet_conf
{
	char org_name[40];
	char site_name[40];
	char host_name[IP_ADDR_LEN];
	char port[6];
	int8_t use_ssh;
	char charset[CHARSET_MAX_LEN + 1];
} bbsnet_conf[MAXSTATION];

static MENU_SET bbsnet_menu;

static void unload_bbsnet_conf(void);

static int load_bbsnet_conf(const char *file_config)
{
	FILE *fp;
	MENU *p_menu;
	MENU_ITEM *p_menu_item;
	MENU_ITEM_ID menu_item_id;
	char line[LINE_BUFFER_LEN], *t1, *t2, *t3, *t4, *t5, *t6, *saveptr;
	long port;
	char *endptr;

	unload_bbsnet_conf();

	bbsnet_menu.p_menu_pool = calloc(1, sizeof(MENU));
	if (bbsnet_menu.p_menu_pool == NULL)
	{
		log_error("calloc(p_menu_pool) error");
		return -1;
	}
	bbsnet_menu.menu_count = 1;

	bbsnet_menu.p_menu_item_pool = calloc(MAXSTATION, sizeof(MENU_ITEM));
	if (bbsnet_menu.p_menu_item_pool == NULL)
	{
		log_error("calloc(p_menu_item_pool) error");
		unload_bbsnet_conf();
		return -1;
	}
	bbsnet_menu.menu_item_count = MAXSTATION;

	p_menu = (MENU *)get_menu_by_id(&bbsnet_menu, 0);

	strncpy(p_menu->name, "BBSNET", sizeof(p_menu->name) - 1);
	p_menu->name[sizeof(p_menu->name) - 1] = '\0';
	p_menu->title.show = 0;
	p_menu->screen_show = 0;

	fp = fopen(file_config, "r");
	if (fp == NULL)
	{
		unload_bbsnet_conf();
		return -2;
	}

	menu_item_id = 0;
	while (fgets(line, sizeof(line), fp) && menu_item_id < MAXSTATION)
	{
		t1 = strtok_r(line, MENU_CONF_DELIM, &saveptr);
		t2 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);
		t3 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);
		t4 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);
		t5 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);
		t6 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);

		if (t1 == NULL || t2 == NULL || t3 == NULL || t4 == NULL ||
			t5 == NULL || t6 == NULL || t1[0] == '#')
		{
			continue;
		}

		strncpy(bbsnet_conf[menu_item_id].site_name, t2, sizeof(bbsnet_conf[menu_item_id].site_name) - 1);
		bbsnet_conf[menu_item_id].site_name[sizeof(bbsnet_conf[menu_item_id].site_name) - 1] = '\0';
		strncpy(bbsnet_conf[menu_item_id].org_name, t1, sizeof(bbsnet_conf[menu_item_id].org_name) - 1);
		bbsnet_conf[menu_item_id].org_name[sizeof(bbsnet_conf[menu_item_id].org_name) - 1] = '\0';
		strncpy(bbsnet_conf[menu_item_id].host_name, t3, sizeof(bbsnet_conf[menu_item_id].host_name) - 1);
		bbsnet_conf[menu_item_id].host_name[sizeof(bbsnet_conf[menu_item_id].host_name) - 1] = '\0';
		port = strtol(t4, &endptr, 10);
		if (*endptr != '\0' || port <= 0 || port > 65535)
		{
			log_error("Invalid port value %ld of menu item %d", port, menu_item_id);
			fclose(fp);
			unload_bbsnet_conf();
			return -3;
		}
		strncpy(bbsnet_conf[menu_item_id].port, t4, sizeof(bbsnet_conf[menu_item_id].port) - 1);
		bbsnet_conf[menu_item_id].port[sizeof(bbsnet_conf[menu_item_id].port) - 1] = '\0';
		bbsnet_conf[menu_item_id].use_ssh = (toupper(t5[0]) == 'Y');
		strncpy(bbsnet_conf[menu_item_id].charset, t6, sizeof(bbsnet_conf[menu_item_id].charset) - 1);
		bbsnet_conf[menu_item_id].charset[sizeof(bbsnet_conf[menu_item_id].charset) - 1] = '\0';

		p_menu_item = get_menu_item_by_id(&bbsnet_menu, menu_item_id);
		if (p_menu_item == NULL)
		{
			log_error("get_menu_item_by_id(%d) return NULL pointer", menu_item_id);
			fclose(fp);
			unload_bbsnet_conf();
			return -3;
		}

		p_menu_item->row = (int16_t)(2 + menu_item_id / STATION_PER_LINE);
		p_menu_item->col = (int16_t)(5 + menu_item_id % STATION_PER_LINE * 20);
		snprintf(p_menu_item->action, sizeof(p_menu_item->action), "%d", (int16_t)menu_item_id);
		p_menu_item->submenu = 0;
		p_menu_item->priv = 0;
		p_menu_item->level = 0;
		p_menu_item->name[0] =
			(char)(menu_item_id < MAXSTATION / 2 ? 'A' + menu_item_id : 'a' + menu_item_id - MAXSTATION / 2);
		p_menu_item->name[1] = '\0';
		snprintf(p_menu_item->text, sizeof(p_menu_item->text), "\033[1;36m%c.\033[m %s",
				 p_menu_item->name[0], bbsnet_conf[menu_item_id].site_name);

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

	if (bbsnet_menu.p_menu_pool)
	{
		free(bbsnet_menu.p_menu_pool);
		bbsnet_menu.p_menu_pool = NULL;
	}

	if (bbsnet_menu.p_menu_item_pool)
	{
		free(bbsnet_menu.p_menu_item_pool);
		bbsnet_menu.p_menu_item_pool = NULL;
	}
}

static void progress_bar(int percent, int len)
{
	char line[LINE_BUFFER_LEN];
	char buf[LINE_BUFFER_LEN];
	char buf2[LINE_BUFFER_LEN];
	int pos;

	if (len < 4)
	{
		len = 4;
	}
	else if (len + 2 > LINE_BUFFER_LEN)
	{
		len = LINE_BUFFER_LEN - 3;
	}
	if (percent < 0)
	{
		percent = 0;
	}
	else if (percent > 100)
	{
		percent = 100;
	}

	pos = len * percent / 100;

	line[0] = ' ';
	for (int i = 1; i <= len; i++)
	{
		line[i] = '-';
	}
	line[len + 1] = ' ';
	line[len + 2] = '\0';

	snprintf(buf, sizeof(buf), "%*s%3d%%%*s",
			 (len - 4) / 2, "", percent, (len - 4 + 1) / 2, "");
	memcpy(buf2, buf, (size_t)pos);
	buf2[pos] = '\0';

	moveto(4, 1);
	prints("%s\r\n", line);
	prints("|\033[46m%s\033[44m%s\033[m|\r\n", buf2, buf + pos);
	prints("%s\r\n", line);
	iflush();
}

static int bbsnet_connect(int n)
{
	int sock = -1;
	int ret = 0;
	int loop;
	int error;
	int sock_connected = 0;
	int flags_sock = -1;
	int flags_stdin = -1;
	int flags_stdout = -1;
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
	iconv_t input_cd = (iconv_t)(-1);
	iconv_t output_cd = (iconv_t)(-1);
	char tocode[CHARSET_MAX_LEN + 20];

#ifdef HAVE_SYS_EPOLL_H
	struct epoll_event ev, events[MAX_EVENTS];
	int epollfd = -1;
#else
	struct pollfd pfds[3];
#endif

	int nfds;
	int stdin_read_wait = 0;
	int stdout_write_wait = 0;
	int sock_read_wait = 0;
	int sock_write_wait = 0;
	struct addrinfo hints, *res = NULL;
	int tos;
	char remote_addr[INET_ADDRSTRLEN];
	int remote_port;
	char local_addr[INET_ADDRSTRLEN];
	int local_port;
	socklen_t sock_len;
	time_t t_begin, t_used;
	struct timespec ts_begin, ts_now;
	int progress, progress_last;
	int ch;
	char remote_user[USERNAME_MAX_LEN + 1];
	char remote_pass[PASSWORD_MAX_LEN + 1];
	ssh_session outbound_session = NULL;
	ssh_channel outbound_channel = NULL;
	int ssh_process_config = 0;
	int ssh_log_level = SSH_LOG_NOLOG;

	if (user_online_update("BBS_NET") < 0)
	{
		log_error("user_online_update(BBS_NET) error");
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
		prints("通过SSH方式连接[%s]...", bbsnet_conf[n].site_name);
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
		   bbsnet_conf[n].site_name, bbsnet_conf[n].host_name);
	iflush();

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if ((ret = getaddrinfo(BBS_address, NULL, &hints, &res)) != 0)
	{
		log_error("getaddrinfo() error (%d): %s", ret, gai_strerror(ret));
		ret = -1;
		goto cleanup;
	}

	if (inet_ntop(AF_INET, &(((struct sockaddr_in *)res->ai_addr)->sin_addr), local_addr, sizeof(local_addr)) == NULL)
	{
		log_error("inet_ntop() error (%d)", errno);
		ret = -1;
		goto cleanup;
	}
	local_port = ntohs(((struct sockaddr_in *)res->ai_addr)->sin_port);

	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock < 0)
	{
		log_error("socket() error (%d)", errno);
		ret = -1;
		goto cleanup;
	}

	if (bind(sock, res->ai_addr, res->ai_addrlen) < 0)
	{
		log_error("bind(%s:%u) error (%d)", local_addr, local_port, errno);
		ret = -1;
		goto cleanup;
	}

	freeaddrinfo(res);
	res = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if ((ret = getaddrinfo(bbsnet_conf[n].host_name, bbsnet_conf[n].port, &hints, &res)) != 0)
	{
		log_error("getaddrinfo() error (%d): %s", ret, gai_strerror(ret));
		prints("\033[1;31m查找主机名失败！\033[m\r\n");
		press_any_key();
		ret = -1;
		goto cleanup;
	}

	if (inet_ntop(AF_INET, &(((struct sockaddr_in *)res->ai_addr)->sin_addr), remote_addr, sizeof(remote_addr)) == NULL)
	{
		log_error("inet_ntop() error (%d)", errno);
		ret = -1;
		goto cleanup;
	}
	remote_port = ntohs(((struct sockaddr_in *)res->ai_addr)->sin_port);

	// Set socket as non-blocking
	if ((flags_sock = fcntl(sock, F_GETFL, 0)) == -1)
	{
		log_error("fcntl(F_GETFL) error (%d)", errno);
		ret = -1;
		goto cleanup;
	}
	if ((fcntl(sock, F_SETFL, flags_sock | O_NONBLOCK)) == -1)
	{
		log_error("fcntl(F_SETFL) error (%d)", errno);
		ret = -1;
		goto cleanup;
	}

	// Set STDIN/STDOUT as non-blocking
	if ((flags_stdin = fcntl(STDIN_FILENO, F_GETFL, 0)) == -1)
	{
		log_error("fcntl(F_GETFL) error (%d)", errno);
		ret = -1;
		goto cleanup;
	}
	if ((flags_stdout = fcntl(STDOUT_FILENO, F_GETFL, 0)) == -1)
	{
		log_error("fcntl(F_GETFL) error (%d)", errno);
		ret = -1;
		goto cleanup;
	}
	if ((fcntl(STDIN_FILENO, F_SETFL, flags_stdin | O_NONBLOCK)) == -1)
	{
		log_error("fcntl(F_SETFL) error (%d)", errno);
		ret = -1;
		goto cleanup;
	}
	if ((fcntl(STDOUT_FILENO, F_SETFL, flags_stdout | O_NONBLOCK)) == -1)
	{
		log_error("fcntl(F_SETFL) error (%d)", errno);
		ret = -1;
		goto cleanup;
	}

#ifdef HAVE_SYS_EPOLL_H
	epollfd = epoll_create1(0);
	if (epollfd < 0)
	{
		log_error("epoll_create1() error (%d)", errno);
		ret = -1;
		goto cleanup;
	}

	ev.events = EPOLLOUT | EPOLLET;
	ev.data.fd = sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) == -1)
	{
		log_error("epoll_ctl(socket) error (%d)", errno);
		ret = -1;
		goto cleanup;
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = STDIN_FILENO;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1)
	{
		log_error("epoll_ctl(STDIN_FILENO) error (%d)", errno);
		ret = -1;
		goto cleanup;
	}
#endif

	while (!SYS_server_exit)
	{
		if ((ret = connect(sock, res->ai_addr, res->ai_addrlen)) == 0)
		{
			sock_connected = 1;
			break;
		}
		else if (ret < 0)
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
				log_error("connect(socket) error (%d)", errno);
				prints("\033[1;31m连接失败！\033[m\r\n");
				press_any_key();
				ret = -1;
				goto cleanup;
			}
		}
	}

	progress = progress_last = 0;
	prints("\033[1;32m连接进行中，按\033[1;33mCtrl+C\033[1;32m中断。\033[m\r\n");
	progress_bar(0, PROGRESS_BAR_LEN);

	if (clock_gettime(CLOCK_REALTIME, &ts_begin) == -1)
	{
		log_error("clock_gettime() error (%d)", errno);
		ret = -1;
		goto cleanup;
	}
	ts_now = ts_begin;

	while ((ts_now.tv_sec - ts_begin.tv_sec) * 1000 +
				   (ts_now.tv_nsec - ts_begin.tv_nsec) / 1000 / 1000 <
			   REMOTE_CONNECT_TIMEOUT * 1000 &&
		   !sock_connected && !SYS_server_exit)
	{
#ifdef HAVE_SYS_EPOLL_H
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, 100); // 0.1 second
		ret = nfds;
#else
		pfds[0].fd = sock;
		pfds[0].events = POLLOUT;
		pfds[1].fd = STDIN_FILENO;
		pfds[1].events = POLLIN;
		nfds = 2;
		ret = poll(pfds, (nfds_t)nfds, 100); // 0.1 second
#endif

		if (ret < 0)
		{
			if (errno != EINTR)
			{
#ifdef HAVE_SYS_EPOLL_H
				log_error("epoll_wait() error (%d)", errno);
#else
				log_error("poll() error (%d)", errno);
#endif
				break;
			}
		}
		else if (ret == 0) // timeout
		{
			if (clock_gettime(CLOCK_REALTIME, &ts_now) == -1)
			{
				log_error("clock_gettime() error (%d)", errno);
				ret = -1;
				goto cleanup;
			}

			progress = (int)((ts_now.tv_sec - ts_begin.tv_sec) * 1000 +
							 (ts_now.tv_nsec - ts_begin.tv_nsec) / 1000 / 1000) /
						   REMOTE_CONNECT_TIMEOUT / 10 +
					   1;
			if (progress < 0)
			{
				progress = 0;
			}
			if (progress > 100)
			{
				progress = 100;
			}

			if (progress > progress_last)
			{
				progress_last = progress;
				progress_bar(progress, PROGRESS_BAR_LEN);
			}
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
					socklen_t len = sizeof(error);
					if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
					{
						log_error("getsockopt() error (%d) !", errno);
						ret = -1;
						goto cleanup;
					}
					if (error == 0)
					{
						sock_connected = 1;
						break;
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
						ret = 0;
						goto cleanup;
					}
				}
			}
		}
	}
	if (SYS_server_exit)
	{
		ret = 0;
		goto cleanup;
	}
	if (!sock_connected)
	{
		progress_bar(100, PROGRESS_BAR_LEN);
		prints("\033[1;31m连接失败！\033[m\r\n");
		press_any_key();
		ret = -1;
		goto cleanup;
	}

	tos = IPTOS_LOWDELAY;
	if (setsockopt(sock, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0)
	{
		log_error("setsockopt IP_TOS=%d error (%d)", tos, errno);
	}

	sock_len = sizeof(sin);
	if (getsockname(sock, (struct sockaddr *)&sin, &sock_len) < 0)
	{
		log_error("getsockname() error: %d", errno);
		ret = -1;
		goto cleanup;
	}

	if (inet_ntop(AF_INET, &(sin.sin_addr), local_addr, sizeof(local_addr)) == NULL)
	{
		log_error("inet_ntop() error (%d)", errno);
		ret = -1;
		goto cleanup;
	}
	local_port = ntohs(sin.sin_port);

	if (bbsnet_conf[n].use_ssh)
	{
		outbound_session = ssh_new();
		if (outbound_session == NULL)
		{
			log_error("ssh_new() error");
			ret = -1;
			goto cleanup;
		}

		if (ssh_options_set(outbound_session, SSH_OPTIONS_FD, &sock) < 0 ||
			ssh_options_set(outbound_session, SSH_OPTIONS_PROCESS_CONFIG, &ssh_process_config) < 0 ||
			ssh_options_set(outbound_session, SSH_OPTIONS_KNOWNHOSTS, SSH_KNOWN_HOSTS_FILE) < 0 ||
			ssh_options_set(outbound_session, SSH_OPTIONS_HOST, bbsnet_conf[n].host_name) < 0 ||
			ssh_options_set(outbound_session, SSH_OPTIONS_USER, remote_user) < 0 ||
			ssh_options_set(outbound_session, SSH_OPTIONS_HOSTKEYS, "+ssh-ed25519,ecdsa-sha2-nistp256,ssh-rsa") < 0 ||
			ssh_options_set(outbound_session, SSH_OPTIONS_LOG_VERBOSITY, &ssh_log_level) < 0)
		{
			log_error("Error setting SSH options: %s", ssh_get_error(outbound_session));
			ret = -1;
			goto cleanup;
		}

		ssh_set_blocking(outbound_session, 0);

		t_begin = time(NULL);
		ret = SSH_ERROR;
		while (!SYS_server_exit && time(NULL) - t_begin < SSH_CONNECT_TIMEOUT)
		{
			ret = ssh_connect(outbound_session);
			if (ret == SSH_OK)
			{
				break;
			}
			else if (ret == SSH_AGAIN)
			{
				// log_debug("ssh_connect() error: SSH_AGAIN");
			}
			else // if (ret == SSH_ERROR)
			{
				log_error("ssh_connect() error: SSH_ERROR");
				ret = -1;
				goto cleanup;
			}
		}
		if (ret != SSH_OK)
		{
			prints("\033[1;31m连接超时！\033[m\r\n");
			press_any_key();
			ret = -1;
			goto cleanup;
		}

		ret = ssh_session_is_known_server(outbound_session);
		switch (ret)
		{
		case SSH_KNOWN_HOSTS_NOT_FOUND:
		case SSH_KNOWN_HOSTS_UNKNOWN:
			if (ssh_session_update_known_hosts(outbound_session) != SSH_OK)
			{
				log_error("ssh_session_update_known_hosts(%s) error", bbsnet_conf[n].host_name);
				prints("\033[1;31m无法添加服务器证书\033[m\r\n");
				press_any_key();
				ret = -1;
				goto cleanup;
			}
			log_common("SSH key of (%s) is added into %s", bbsnet_conf[n].host_name, SSH_KNOWN_HOSTS_FILE);
		case SSH_KNOWN_HOSTS_OK:
			break;
		case SSH_KNOWN_HOSTS_CHANGED:
		case SSH_KNOWN_HOSTS_OTHER:
			log_error("ssh_session_is_known_server(%s) error: %d", bbsnet_conf[n].host_name, ret);
			prints("\033[1;31m服务器证书已变更\033[m\r\n");
			press_any_key();
			ret = -1;
			goto cleanup;
		}

		ret = SSH_AUTH_ERROR;
		while (!SYS_server_exit && time(NULL) - t_begin < SSH_CONNECT_TIMEOUT)
		{
			ret = ssh_userauth_password(outbound_session, NULL, remote_pass);
			if (ret == SSH_AUTH_SUCCESS)
			{
				break;
			}
			else if (ret == SSH_AUTH_AGAIN)
			{
				// log_debug("ssh_userauth_password() error: SSH_AUTH_AGAIN");
			}
			else if (ret == SSH_AUTH_ERROR)
			{
				log_error("ssh_userauth_password() error: SSH_AUTH_ERROR");
				ret = -1;
				goto cleanup;
			}
			else // if (ret == SSH_AUTH_DENIED)
			{
				log_error("ssh_userauth_password() error: SSH_AUTH_DENIED");
				prints("\033[1;31m身份验证失败！\033[m\r\n");
				press_any_key();
				ret = -1;
				goto cleanup;
			}
		}
		if (ret != SSH_AUTH_SUCCESS)
		{
			prints("\033[1;31m连接超时！\033[m\r\n");
			press_any_key();
			ret = -1;
			goto cleanup;
		}

		outbound_channel = ssh_channel_new(outbound_session);
		if (outbound_channel == NULL)
		{
			log_error("ssh_channel_new() error");
			ret = -1;
			goto cleanup;
		}

		ret = SSH_ERROR;
		while (!SYS_server_exit && time(NULL) - t_begin < SSH_CONNECT_TIMEOUT)
		{
			ret = ssh_channel_open_session(outbound_channel);
			if (ret == SSH_OK)
			{
				break;
			}
			else if (ret == SSH_AGAIN)
			{
				// log_debug("ssh_channel_open_session() error: SSH_AGAIN");
			}
			else // if (ret == SSH_ERROR)
			{
				log_error("ssh_channel_open_session() error: SSH_ERROR");
				ret = -1;
				goto cleanup;
			}
		}
		if (ret != SSH_OK)
		{
			prints("\033[1;31m连接超时！\033[m\r\n");
			press_any_key();
			ret = -1;
			goto cleanup;
		}

		ret = SSH_ERROR;
		while (!SYS_server_exit && time(NULL) - t_begin < SSH_CONNECT_TIMEOUT)
		{
			ret = ssh_channel_request_pty(outbound_channel);
			if (ret == SSH_OK)
			{
				break;
			}
			else if (ret == SSH_AGAIN)
			{
				// log_debug("ssh_channel_request_pty() error: SSH_AGAIN");
			}
			else // if (ret == SSH_ERROR)
			{
				log_error("ssh_channel_request_pty() error: SSH_ERROR");
				ret = -1;
				goto cleanup;
			}
		}
		if (ret != SSH_OK)
		{
			prints("\033[1;31m连接超时！\033[m\r\n");
			press_any_key();
			ret = -1;
			goto cleanup;
		}

		ret = SSH_ERROR;
		while (!SYS_server_exit && time(NULL) - t_begin < SSH_CONNECT_TIMEOUT)
		{
			ret = ssh_channel_request_shell(outbound_channel);
			if (ret == SSH_OK)
			{
				break;
			}
			else if (ret == SSH_AGAIN)
			{
				// log_debug("ssh_channel_request_shell() error: SSH_AGAIN");
			}
			else // if (ret == SSH_ERROR)
			{
				log_error("ssh_channel_request_shell() error: SSH_ERROR");
				ret = -1;
				goto cleanup;
			}
		}
		if (ret != SSH_OK)
		{
			prints("\033[1;31m连接超时！\033[m\r\n");
			press_any_key();
			ret = -1;
			goto cleanup;
		}
	}

	prints("\033[1;31m连接成功！\033[m\r\n");
	iflush();
	log_common("BBSNET connect to %s:%d from %s:%d by [%s]",
			   remote_addr, remote_port, local_addr, local_port, BBS_username);

	snprintf(tocode, sizeof(tocode), "%s%s", bbsnet_conf[n].charset,
			 (strcasecmp(stdio_charset, bbsnet_conf[n].charset) == 0 ? "" : "//IGNORE"));
	input_cd = iconv_open(tocode, stdio_charset);
	if (input_cd == (iconv_t)(-1))
	{
		log_error("iconv_open(%s->%s) error: %d", stdio_charset, tocode, errno);
		ret = -1;
		goto cleanup;
	}

	snprintf(tocode, sizeof(tocode), "%s%s", stdio_charset,
			 (strcasecmp(bbsnet_conf[n].charset, stdio_charset) == 0 ? "" : "//TRANSLIT"));
	output_cd = iconv_open(tocode, bbsnet_conf[n].charset);
	if (output_cd == (iconv_t)(-1))
	{
		log_error("iconv_open(%s->%s) error: %d", bbsnet_conf[n].charset, tocode, errno);
		ret = -1;
		goto cleanup;
	}

#ifdef HAVE_SYS_EPOLL_H
	ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
	ev.data.fd = sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sock, &ev) == -1)
	{
		log_error("epoll_ctl(socket) error (%d)", errno);
		ret = -1;
		goto cleanup;
	}

	ev.events = EPOLLOUT | EPOLLET;
	ev.data.fd = STDOUT_FILENO;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDOUT_FILENO, &ev) == -1)
	{
		log_error("epoll_ctl(STDOUT_FILENO) error (%d)", errno);
		ret = -1;
		goto cleanup;
	}
#endif

	BBS_last_access_tm = t_begin = time(NULL);
	loop = 1;

	while (loop && !SYS_server_exit)
	{
		if (SSH_v2 && ssh_channel_is_closed(SSH_channel))
		{
			log_debug("SSH channel is closed");
			loop = 0;
			break;
		}

		if (bbsnet_conf[n].use_ssh && ssh_channel_is_closed(outbound_channel))
		{
			log_debug("Outbound channel is closed");
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
				log_error("epoll_wait() error (%d)", errno);
#else
				log_error("poll() error (%d)", errno);
#endif
				break;
			}
			continue;
		}
		else if (ret == 0) // timeout
		{
			if (time(NULL) - BBS_last_access_tm >= BBS_max_user_idle_time)
			{
				log_debug("User input timeout");
				break;
			}
		}

		for (int i = 0; i < nfds; i++)
		{
#ifdef HAVE_SYS_EPOLL_H
			if (events[i].events & (EPOLLHUP | EPOLLERR))
#else
			if (pfds[i].revents & (POLLHUP | POLLERR))
#endif
			{
#ifdef HAVE_SYS_EPOLL_H
				log_debug("FD (%d) error events (%d)", events[i].data.fd, events[i].events);
#else
				log_debug("FD (%d) error events (%d)", pfds[i].fd, pfds[i].revents);
#endif
				loop = 0;
				break;
			}

#ifdef HAVE_SYS_EPOLL_H
			if (events[i].data.fd == STDIN_FILENO && (events[i].events & EPOLLIN))
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
			if (events[i].data.fd == STDOUT_FILENO && (events[i].events & EPOLLOUT))
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
						log_debug("ssh_channel_read_nonblocking() error: %s", ssh_get_error(SSH_session));
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
						log_error("read(STDIN) error (%d)", errno);
						loop = 0;
						break;
					}
				}
				else if (ret == 0) // broken pipe
				{
					log_debug("read(STDIN) EOF");
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
						log_error("user_online_update(BBS_NET) error");
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
					log_debug("input: <--[%u]", (input_buf[j] + 256) % 256);
				}
#endif

				ret = io_buf_conv(input_cd, input_buf, &input_buf_len, &input_buf_offset, input_conv, sizeof(input_conv), &input_conv_len);
				if (ret < 0)
				{
					log_error("io_buf_conv(input, %d, %d, %d) error", input_buf_len, input_buf_offset, input_conv_len);
					input_buf_len = input_buf_offset; // Discard invalid sequence
				}

				// For debug
#ifdef _DEBUG
				for (int j = input_conv_offset; j < input_conv_len; j++)
				{
					log_debug("input_conv: <--[%u]", (input_conv[j] + 256) % 256);
				}
#endif
			}

			while (input_conv_offset < input_conv_len && !SYS_server_exit)
			{
				if (bbsnet_conf[n].use_ssh)
				{
					ret = ssh_channel_write(outbound_channel, input_conv + input_conv_offset, (uint32_t)(input_conv_len - input_conv_offset));
					if (ret == SSH_ERROR)
					{
						log_debug("ssh_channel_write() error: %s", ssh_get_error(outbound_session));
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
						log_debug("write(socket) error (%d)", errno);
						loop = 0;
						break;
					}
				}
				else if (ret == 0) // broken pipe
				{
					log_debug("write(socket) EOF");
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
					ret = ssh_channel_read_nonblocking(outbound_channel, output_buf + output_buf_len,
													   (uint32_t)(sizeof(output_buf) - (size_t)output_buf_len), 0);
					if (ret == SSH_ERROR)
					{
						log_debug("ssh_channel_read_nonblocking() error: %s", ssh_get_error(outbound_session));
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
						log_debug("read(socket) error (%d)", errno);
						loop = 0;
						break;
					}
				}
				else if (ret == 0) // broken pipe
				{
					log_debug("read(socket) EOF");
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
					log_error("io_buf_conv(output, %d, %d, %d) error", output_buf_len, output_buf_offset, output_conv_len);
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
						log_debug("ssh_channel_write() error: %s", ssh_get_error(SSH_session));
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
						log_debug("write(STDOUT) error (%d)", errno);
						loop = 0;
						break;
					}
				}
				else if (ret == 0) // broken pipe
				{
					log_debug("write(STDOUT) EOF");
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

	ret = 1; // Normal disconnect
	BBS_last_access_tm = time(NULL);
	t_used = BBS_last_access_tm - t_begin;
	log_common("BBSNET disconnect, %ld days %ld hours %ld minutes %ld seconds used",
			   t_used / 86400, t_used % 86400 / 3600, t_used % 3600 / 60, t_used % 60);

cleanup:
	// Clear sensitive data
	memset(remote_pass, 0, sizeof(remote_pass));
	memset(remote_user, 0, sizeof(remote_user));

	if (input_cd != (iconv_t)(-1))
	{
		iconv_close(input_cd);
	}
	if (output_cd != (iconv_t)(-1))
	{
		iconv_close(output_cd);
	}

#ifdef HAVE_SYS_EPOLL_H
	if (epollfd != -1 && close(epollfd) < 0)
	{
		log_error("close(epoll) error (%d)");
	}
#endif

	if (bbsnet_conf[n].use_ssh)
	{
		if (outbound_channel != NULL)
		{
			ssh_channel_send_eof(outbound_channel);
			ssh_channel_close(outbound_channel);
			ssh_channel_free(outbound_channel);
		}
		if (outbound_session != NULL)
		{
			ssh_disconnect(outbound_session);
			ssh_free(outbound_session);
		}
	}

	// Restore STDIN/STDOUT flags
	if (flags_stdin != -1 && fcntl(STDIN_FILENO, F_SETFL, flags_stdin) == -1)
	{
		log_error("fcntl(F_SETFL) error (%d)", errno);
	}
	if (flags_stdout != -1 && fcntl(STDOUT_FILENO, F_SETFL, flags_stdout) == -1)
	{
		log_error("fcntl(F_SETFL) error (%d)", errno);
	}

	if (sock != -1 && close(sock) == -1)
	{
		log_error("close(socket) error (%d)", errno);
	}

	if (res)
	{
		freeaddrinfo(res);
	}

	return ret;
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
	prints("|\033[1m单位: \033[1;33m%s\033[m%*s  站名: \033[1;33m%s\033[m%*s  类型: \033[1;33m%s\033[m",
		   bbsnet_conf[i].org_name, 20 - str_length(bbsnet_conf[i].org_name, 1), "",
		   bbsnet_conf[i].site_name, 20 - str_length(bbsnet_conf[i].site_name, 1), "",
		   (bbsnet_conf[i].use_ssh ? "SSH" : "Telnet"));
	moveto(20, 80);
	prints("|");
	moveto(21, 1);
	clrtoeol();
	prints("|\033[1m连往: \033[1;33m%-20s\033[m  端口: \033[1;33m%-5s\033[m                 编码: \033[1;33m%s\033[m",
		   bbsnet_conf[i].host_name, bbsnet_conf[i].port, bbsnet_conf[i].charset);
	moveto(21, 80);
	prints("|");
	iflush();

	return 0;
}

int bbs_net()
{
	int ch;

	if (load_bbsnet_conf(CONF_BBSNET) < 0)
	{
		clearscr();
		moveto(1, 1);
		prints("加载穿梭配置失败！");
		press_any_key();
		return -1;
	}

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
			log_debug("KEY_NULL");
			goto cleanup;
		case KEY_TIMEOUT:
			if (time(NULL) - BBS_last_access_tm >= BBS_max_user_idle_time)
			{
				log_debug("User input timeout");
				goto cleanup;
			}
			continue;
		case KEY_ESC:
		case Ctrl('C'): // user cancel
			goto cleanup;
		case CR:
			if (bbsnet_connect(bbsnet_menu.menu_item_pos[0]) < 0)
			{
				log_debug("bbsnet_connect() error");
			}
			// Force cleanup anything remaining in the output buffer
			clearscr();
			iflush();
			// Clear screen and redraw menu
			bbsnet_refresh();
			display_menu(&bbsnet_menu);
			bbsnet_selchange();
			break;
		case KEY_UP:
			for (int i = 0; i < STATION_PER_LINE; i++)
			{
				menu_control(&bbsnet_menu, KEY_UP);
			}
			bbsnet_selchange();
			break;
		case KEY_DOWN:
			for (int i = 0; i < STATION_PER_LINE; i++)
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
