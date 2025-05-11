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
#include "log.h"
#include "io.h"
#include "screen.h"
#include "menu.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define MENU_CONF_DELIM " \t\r\n"

#define MAX_PROCESS_BAR_LEN 30
#define MAXSTATION 26 * 2
#define STATION_PER_LINE 4

struct _bbsnet_conf
{
	char host1[20];
	char host2[40];
	char ip[40];
	in_port_t port;
} bbsnet_conf[MAXSTATION];

MENU_SET bbsnet_menu;

int load_bbsnet_conf(const char *file_config)
{
	FILE *fp;
	MENU *p_menu;
	MENU_ITEM *p_menuitem;
	char t[256], *t1, *t2, *t3, *t4, *saveptr;
	int item_count = 0;

	fp = fopen(file_config, "r");
	if (fp == NULL)
	{
		return -1;
	}

	p_menu = bbsnet_menu.p_menu[0] = malloc(sizeof(MENU));
	strncpy(p_menu->name, "BBSNET", sizeof(p_menu->name) - 1);
	p_menu->name[sizeof(p_menu->name) - 1] = '\0';
	p_menu->title.show = 0;
	p_menu->screen.show = 0;

	while (fgets(t, 255, fp) && item_count < MAXSTATION)
	{
		t1 = strtok_r(t, MENU_CONF_DELIM, &saveptr);
		t2 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);
		t3 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);
		t4 = strtok_r(NULL, MENU_CONF_DELIM, &saveptr);

		if (t1 == NULL || t2 == NULL || t3 == NULL || t4 == NULL || t[0] == '#' || t[0] == '*')
		{
			continue;
		}

		strncpy(bbsnet_conf[item_count].host1, t2, sizeof(bbsnet_conf[item_count].host1) - 1);
		bbsnet_conf[item_count].host1[sizeof(bbsnet_conf[item_count].host1) - 1] = '\0';
		strncpy(bbsnet_conf[item_count].host2, t1, sizeof(bbsnet_conf[item_count].host2) - 1);
		bbsnet_conf[item_count].host2[sizeof(bbsnet_conf[item_count].host2) - 1] = '\0';
		strncpy(bbsnet_conf[item_count].ip, t3, sizeof(bbsnet_conf[item_count].ip) - 1);
		bbsnet_conf[item_count].ip[sizeof(bbsnet_conf[item_count].ip) - 1] = '\0';
		bbsnet_conf[item_count].port = (in_port_t)(t4 ? atoi(t4) : 23);

		p_menuitem = p_menu->items[item_count] = malloc(sizeof(MENU_ITEM));
		p_menuitem->row = 2 + item_count / STATION_PER_LINE;
		p_menuitem->col = 5 + item_count % STATION_PER_LINE * 20;
		snprintf(p_menuitem->action, sizeof(p_menuitem->action), "%d", item_count);
		p_menuitem->submenu = 0;
		p_menuitem->priv = 0;
		p_menuitem->level = 0;
		p_menuitem->display = 0;
		p_menuitem->name[0] =
			(char)(item_count < MAXSTATION / 2 ? 'A' + item_count : 'a' + item_count);
		p_menuitem->name[1] = '\0';
		snprintf(p_menuitem->text, sizeof(p_menuitem->text), "[1;36m%c.[m %s",
				 p_menuitem->name[0], bbsnet_conf[item_count].host1);

		item_count++;
	}
	fclose(fp);

	p_menu->item_count = item_count;
	p_menu->item_cur_pos = 0;

	bbsnet_menu.menu_count = 1;
	bbsnet_menu.menu_select_depth = 0;
	bbsnet_menu.p_menu_select[0] = bbsnet_menu.p_menu[0];

	return 0;
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

	moveto(4, 0);
	prints(" ------------------------------ \r\n");
	snprintf(buf, sizeof(buf), "            %3d%%              ", n * 100 / len);
	strncpy(buf2, buf, (size_t)n);
	buf2[n] = '\0';
	prints("|\033[46m%s\033[44m%s\033[m|\r\n", buf2, buf + n);
	prints(" ------------------------------ \r\n");
	iflush();
}

int bbsnet_connect(int n)
{
	int sock, flags, ret, loop, error;
	ssize_t len;
	struct sockaddr_in sin;
	char buf[LINE_BUFFER_LEN];
	fd_set read_fds;
	fd_set write_fds;
	struct timeval timeout;
	struct hostent *p_host = NULL;
	int tos;
	int i;
	char remote_addr[IP_ADDR_LEN];
	int remote_port;
	time_t t_used;
	struct tm *tm_used;
	int ch;
	int offset;

	clearscr();

	moveto(0, 0);
	prints("\033[1;32m’˝‘⁄≤‚ ‘Õ˘ %s (%s) µƒ¡¨Ω”£¨«Î…‘∫Ú... \033[m\r\n",
		   bbsnet_conf[n].host1, bbsnet_conf[n].ip);
	iflush();

	p_host = gethostbyname(bbsnet_conf[n].ip);

	if (p_host == NULL)
	{
		prints("\033[1;31m≤È’“÷˜ª˙√˚ ß∞‹£°\033[m\r\n");
		press_any_key();
		return -1;
	}

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock < 0)
	{
		prints("\033[1;31mŒﬁ∑®¥¥Ω®socket£°\033[m\r\n");
		press_any_key();
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = (hostaddr_server[0] != '\0' ? inet_addr(hostaddr_server) : INADDR_ANY);
	sin.sin_port = 0;

	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		log_error("Bind address %s:%u failed (%d)\n",
				  inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), errno);
		return -2;
	}

	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr = *(struct in_addr *)p_host->h_addr_list[0];
	sin.sin_port = htons(bbsnet_conf[n].port);

	strncpy(remote_addr, inet_ntoa(sin.sin_addr), sizeof(remote_addr) - 1);
	remote_addr[sizeof(remote_addr) - 1] = '\0';
	remote_port = ntohs(sin.sin_port);

	prints("\033[1;32m¥©ÀÛΩ¯∂»ÃıÃ· æƒ˙µ±«∞“— π”√µƒ ±º‰£¨∞¥\033[1;33mCtrl+C\033[1;32m÷–∂œ°£\033[m\r\n");
	process_bar(0, MAX_PROCESS_BAR_LEN);

	// Set socket as non-blocking
	flags = fcntl(sock, F_GETFL, 0);
	fcntl(sock, F_SETFL, flags | O_NONBLOCK);

	if ((ret = connect(sock, (struct sockaddr *)&sin, sizeof(sin))) < 0)
	{
		if (errno != EINPROGRESS)
		{
			prints("\033[1;31m¡¨Ω” ß∞‹£°\033[m\r\n");
			press_any_key();
			return -1;
		}
	}

	for (i = 0; i < MAX_PROCESS_BAR_LEN; i++)
	{
		ch = igetch(0); // 0.1 second
		if (ch == Ctrl('C') || SYS_server_exit)
		{
			return 0;
		}

		FD_ZERO(&read_fds);
		FD_SET(sock, &read_fds);

		FD_ZERO(&write_fds);
		FD_SET(sock, &write_fds);

		timeout.tv_sec = 0;
		timeout.tv_usec = 400 * 1000; // 0.4 second

		ret = select(sock + 1, &read_fds, &write_fds, NULL, &timeout);

		if (ret == 0) // Timeout
		{
			process_bar(i + 1, MAX_PROCESS_BAR_LEN);
		}
		else if (ret < 0)
		{
			if (errno != EINTR)
			{
				log_error("select() error (%d) !\n", errno);
				return -1;
			}
		}
		// ret > 0
		else if (FD_ISSET(sock, &read_fds) || FD_ISSET(sock, &write_fds))
		{
			len = sizeof(error);
			if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len) < 0)
			{
				log_error("getsockopt() error (%d) !\n", error);
				return -1;
			}

			break; // connected
		}
	}
	if (i == MAX_PROCESS_BAR_LEN)
	{
		prints("\033[1;31m¡¨Ω”≥¨ ±£°\033[m\r\n");
		press_any_key();
		return -1;
	}

	fcntl(sock, F_SETFL, flags); /* restore file status flags */

	tos = IPTOS_LOWDELAY;
	if (setsockopt(sock, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0)
	{
		log_error("setsockopt IP_TOS=%d error (%d)\n", tos, errno);
	}

	prints("\033[1;31m¡¨Ω”≥…π¶£°\033[m\r\n");
	iflush();
	log_std("BBSNET connect to %s:%d\n", remote_addr, remote_port);

	BBS_last_access_tm = t_used = time(0);
	loop = 1;

	while (loop && !SYS_server_exit)
	{
		FD_ZERO(&read_fds);
		FD_SET(STDIN_FILENO, &read_fds);
		FD_SET(sock, &read_fds);

		FD_ZERO(&write_fds);
		FD_SET(STDIN_FILENO, &write_fds);
		FD_SET(sock, &write_fds);

		timeout.tv_sec = 0;
		timeout.tv_usec = 100 * 1000; // 0.1 second

		ret = select(sock + 1, &read_fds, &write_fds, NULL, &timeout);

		if (ret == 0) // timeout
		{
			if (time(0) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				loop = 0;
			}
		}
		else if (ret < 0)
		{
			if (errno != EINTR)
			{
				log_error("select() error (%d) !\n", errno);
				loop = 0;
			}
		}
		else if (ret > 0)
		{
			if (FD_ISSET(STDIN_FILENO, &read_fds) && FD_ISSET(sock, &write_fds))
			{
				// Set STDIN as non-blocking
				flags = fcntl(STDIN_FILENO, F_GETFL, 0);
				fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

				len = read(STDIN_FILENO, buf, sizeof(buf));
				if (len < 0)
				{
					if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
					{
						log_error("read(STDIN) error (%d)\n", errno);
						loop = 0;
					}
				}
				else if (len == 0)
				{
					log_error("read(STDIN) reach EOF\n");
					loop = 0;
				}
				else
				{
					offset = 0;
					do
					{
						ret = (int)write(sock, buf + offset, (size_t)(len - offset));
						if (ret < 0)
						{
							log_error("write(socket) error (%d)\n", errno);
							loop = 0;
							break;
						}
						offset += ret;
					} while (offset < len);

					BBS_last_access_tm = time(0);
				}

				// Restore STDIN flags
				fcntl(STDIN_FILENO, F_SETFL, flags);
			}
			if (FD_ISSET(sock, &read_fds) && FD_ISSET(STDIN_FILENO, &write_fds))
			{
				// Set socket as non-blocking
				flags = fcntl(sock, F_GETFL, 0);
				fcntl(sock, F_SETFL, flags | O_NONBLOCK);

				len = read(sock, buf, sizeof(buf));
				if (len < 0)
				{
					if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
					{
						log_error("read(socket) error (%d)\n", errno);
						loop = 0;
					}
				}
				else if (len == 0)
				{
					log_error("read(socket) reach EOF\n");
					loop = 0;
				}
				else
				{
					offset = 0;
					do
					{
						ret = (int)write(STDOUT_FILENO, buf + offset, (size_t)(len - offset));
						if (ret < 0)
						{
							log_error("write(STDOUT) error (%d)\n", errno);
							loop = 0;
							break;
						}
						offset += ret;
					} while (offset < len);
				}

				// Restore socket flags
				fcntl(sock, F_SETFL, flags);
			}
		}
	}

	if (close(sock) == -1)
	{
		log_error("Close socket failed\n");
	}

	t_used = time(0) - t_used;
	tm_used = gmtime(&t_used);

	log_std("BBSNET disconnect, %d days %d hours %d minutes %d seconds used\n",
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
	prints(" [\x1b[1;32mCtrl+C\x1b[m]ÕÀ≥ˆ");

	iflush();

	return 0;
}

int bbsnet_selchange(int new_pos)
{
	moveto(20, 0);
	clrtoeol();
	prints("|\x1b[1mµ•Œª:\x1b[1;33m%-18s\x1b[m  ’æ√˚:\x1b[1;33m%s\x1b[m",
		   bbsnet_conf[new_pos].host2, bbsnet_conf[new_pos].host1);
	moveto(20, 79);
	prints("|");
	moveto(21, 0);
	clrtoeol();
	prints("|\x1b[1m¡¨Õ˘:\x1b[1;33m%-20s", bbsnet_conf[new_pos].ip);
	if (bbsnet_conf[new_pos].port != 23)
	{
		prints("  %d", bbsnet_conf[new_pos].port);
	}
	prints("\x1b[m");
	moveto(21, 79);
	prints("|");
	iflush();

	return 0;
}

int bbs_net()
{
	int ch, pos, i;

	load_bbsnet_conf(CONF_BBSNET);

	BBS_last_access_tm = time(0);

	clearscr();
	bbsnet_refresh();
	pos = bbsnet_menu.p_menu[0]->item_cur_pos;
	display_menu(get_menu(&bbsnet_menu, "BBSNET"));
	bbsnet_selchange(pos);

	while (!SYS_server_exit)
	{
		ch = igetch(0);
		switch (ch)
		{
		case KEY_NULL: // broken pipe
		case Ctrl('C'): // user cancel
			return 0;
		case KEY_TIMEOUT:
			if (time(0) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				return 0;
			}
			continue;
		case CR:
			pos = bbsnet_menu.p_menu[0]->item_cur_pos;
			bbsnet_connect(pos);
			bbsnet_refresh();
			display_current_menu(&bbsnet_menu);
			bbsnet_selchange(pos);
			break;
		case KEY_UP:
			for (i = 0; i < STATION_PER_LINE; i++)
			{
				menu_control(&bbsnet_menu, KEY_UP);
			}
			pos = bbsnet_menu.p_menu[0]->item_cur_pos;
			bbsnet_selchange(pos);
			break;
		case KEY_DOWN:
			for (i = 0; i < STATION_PER_LINE; i++)
			{
				menu_control(&bbsnet_menu, KEY_DOWN);
			}
			pos = bbsnet_menu.p_menu[0]->item_cur_pos;
			bbsnet_selchange(pos);
			break;
		case KEY_LEFT:
			menu_control(&bbsnet_menu, KEY_UP);
			pos = bbsnet_menu.p_menu[0]->item_cur_pos;
			bbsnet_selchange(pos);
			break;
		case KEY_RIGHT:
			menu_control(&bbsnet_menu, KEY_DOWN);
			pos = bbsnet_menu.p_menu[0]->item_cur_pos;
			bbsnet_selchange(pos);
			break;
		default:
			menu_control(&bbsnet_menu, ch);
			pos = bbsnet_menu.p_menu[0]->item_cur_pos;
			bbsnet_selchange(pos);
			break;
		}
		BBS_last_access_tm = time(0);
	}

	return 0;
}
