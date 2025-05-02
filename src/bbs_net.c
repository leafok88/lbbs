/***************************************************************************
						  bbs_net.c  -  description
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
#include "menu.h"
#include "tcplib.h"
#include <stdio.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>

#define TIME_OUT 15
#define MAX_PROCESS_BAR_LEN 30
#define MAXSTATION 26 * 2
#define STATION_PER_LINE 4

struct _bbsnet_conf
{
	char host1[20];
	char host2[40];
	char ip[40];
	int port;
} bbsnet_conf[MAXSTATION];

MENU_SET bbsnet_menu;

int load_bbsnet_conf(const char *file_config)
{
	FILE *fp;
	MENU *p_menu;
	MENU_ITEM *p_menuitem;
	char t[256], *t1, *t2, *t3, *t4;
	int item_count = 0;

	fp = fopen(file_config, "r");
	if (fp == NULL)
		return -1;

	p_menu = bbsnet_menu.p_menu[0] = malloc(sizeof(MENU));
	strcpy(p_menu->name, "BBSNET");
	p_menu->title.show = 0;
	p_menu->screen.show = 0;

	while (fgets(t, 255, fp) && item_count < MAXSTATION)
	{
		t1 = strtok(t, " \t");
		t2 = strtok(NULL, " \t\n");
		t3 = strtok(NULL, " \t\n");
		t4 = strtok(NULL, " \t\n");

		if (t1[0] == '#' || t1[0] == '*' || t1 == NULL || t2 == NULL || t3 == NULL)
			continue;
		strncpy(bbsnet_conf[item_count].host1, t2, 18);
		bbsnet_conf[item_count].host1[18] = 0;
		strncpy(bbsnet_conf[item_count].host2, t1, 36);
		bbsnet_conf[item_count].host2[36] = 0;
		strncpy(bbsnet_conf[item_count].ip, t3, 36);
		bbsnet_conf[item_count].ip[36] = 0;
		bbsnet_conf[item_count].port = t4 ? atoi(t4) : 23;

		p_menuitem = p_menu->items[item_count] = malloc(sizeof(MENU_ITEM));
		p_menuitem->row = 2 + item_count / STATION_PER_LINE;
		p_menuitem->col = 5 + item_count % STATION_PER_LINE * 20;
		sprintf(p_menuitem->action, "%d", item_count);
		p_menuitem->submenu = 0;
		p_menuitem->priv = 0;
		p_menuitem->level = 0;
		p_menuitem->display = 0;
		p_menuitem->name[0] =
			(item_count < MAXSTATION / 2 ? 'A' + item_count : 'a' + item_count);
		p_menuitem->name[1] = '\0';
		sprintf(p_menuitem->text, "[1;36m%c.[m %s",
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

static void
process_bar(int n, int len)
{
	char buf[256];
	char buf2[256];
	char *ptr;
	char *ptr2;
	char *ptr3;

	moveto(4, 0);
	prints("©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´\r\n");
	sprintf(buf2, "            %3d%%              ", n * 100 / len);
	ptr = buf;
	ptr2 = buf2;
	ptr3 = buf + n;
	while (ptr != ptr3)
		*ptr++ = *ptr2++;
	*ptr++ = '\x1b';
	*ptr++ = '[';
	*ptr++ = '4';
	*ptr++ = '4';
	*ptr++ = 'm';
	while (*ptr2 != '\0')
		*ptr++ = *ptr2++;
	*ptr++ = '\0';
	prints("©¦\033[46m%s\033[m©¦\r\n", buf);
	prints("©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼\r\n");
	iflush();
}

int bbsnet_connect(int n)
{
	int sock, ch, result, len, loop;
	struct sockaddr_in sin;
	char buf[256];
	fd_set inputs, testfds;
	struct timeval timeout;
	struct hostent *pHost = NULL;
	int rc, rv, tos = 020, i;
	char remote_addr[256];
	int remote_port;
	time_t t_used;
	struct tm *tm_used;

	clearscr();

	moveto(0, 0);
	prints("\033[1;32mÕýÔÚ²âÊÔÍù %s (%s) µÄÁ¬½Ó£¬ÇëÉÔºò... \033[m\r\n",
		   bbsnet_conf[n].host1, bbsnet_conf[n].ip);
	prints("\033[1;32mÈç¹ûÔÚ %d ÃëÄÚÎÞ·¨Á¬ÉÏ£¬´©Ëó³ÌÐò½«·ÅÆúÁ¬½Ó¡£\033[m\r\n",
		   TIME_OUT);
	iflush();

	pHost = gethostbyname(bbsnet_conf[n].ip);

	if (pHost == NULL)
	{
		prints("\033[1;31m²éÕÒÖ÷»úÃûÊ§°Ü£¡\033[m\r\n");
		press_any_key();
		return -1;
	}

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock < 0)
	{
		prints("\033[1;31mÎÞ·¨´´½¨socket£¡\033[m\r\n");
		press_any_key();
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr =
		(strlen(hostaddr_server) > 0 ? inet_addr(hostaddr_server) : INADDR_ANY);
	sin.sin_port = 0;

	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		log_error("Bind address %s:%u failed\n",
				  inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		return -2;
	}

	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr = *(struct in_addr *)pHost->h_addr_list[0];
	sin.sin_port = htons(bbsnet_conf[n].port);

	strcpy(remote_addr, inet_ntoa(sin.sin_addr));
	remote_port = ntohs(sin.sin_port);

	prints("\033[1;32m´©Ëó½ø¶ÈÌõÌáÊ¾Äúµ±Ç°ÒÑÊ¹ÓÃµÄÊ±¼ä¡£\033[m\r\n");
	process_bar(0, MAX_PROCESS_BAR_LEN);
	for (i = 0; i < MAX_PROCESS_BAR_LEN; i++)
	{
		if (i == 0)
			rv =
				NonBlockConnectEx(sock, (struct sockaddr *)&sin, sizeof(sin),
								  500, 1);
		else
			rv =
				NonBlockConnectEx(sock, (struct sockaddr *)&sin, sizeof(sin),
								  500, 0);
		if (rv == ERR_TCPLIB_TIMEOUT)
		{
			process_bar(i + 1, MAX_PROCESS_BAR_LEN);
			continue;
		}
		else if (rv == 0)
			break;
		else
		{
			prints("\033[1;31mÁ¬½ÓÊ§°Ü£¡\033[m\r\n");
			press_any_key();
			return -1;
		}
	}
	if (i == MAX_PROCESS_BAR_LEN)
	{
		prints("\033[1;31mÁ¬½Ó³¬Ê±£¡\033[m\r\n");
		press_any_key();
		return -1;
	}
	setsockopt(sock, IPPROTO_IP, IP_TOS, &tos, sizeof(int));

	prints("\033[1;31mÁ¬½Ó³É¹¦£¡\033[m\r\n");
	log_std("BBSNET connect to %s:%d\n", remote_addr, remote_port);

	FD_ZERO(&inputs);
	FD_SET(0, &inputs);
	FD_SET(sock, &inputs);

	BBS_last_access_tm = t_used = time(0);

	loop = 1;

	while (loop)
	{
		testfds = inputs;
		timeout.tv_sec = TIME_OUT;
		timeout.tv_usec = 0;

		result = SignalSafeSelect(FD_SETSIZE, &testfds, (fd_set *)NULL,
								  (fd_set *)NULL, &timeout);

		if (result == 0)
		{
			if (time(0) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				loop = 0;
			}
		}
		if (result < 0)
		{
			log_error("select() error (%d) !\n", result);
			loop = 0;
		}
		if (result > 0)
		{
			if (FD_ISSET(0, &testfds))
			{
				len = read(0, buf, 255);
				if (len == 0)
				{
					loop = 0;
				}
				write(sock, buf, len);
			}
			if (FD_ISSET(sock, &testfds))
			{
				len = read(sock, buf, 255);
				if (len == 0)
				{
					loop = 0;
				}
				write(1, buf, len);
			}
			BBS_last_access_tm = time(0);
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
	int i;

	clearscr();
	moveto(1, 0);
	prints("¨q¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨r");
	for (i = 2; i < 19; i++)
	{
		moveto(i, 0);
		prints("¨U");
		moveto(i, 79);
		prints("¨U");
	}
	moveto(19, 0);
	prints("¨U¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¡ª¨U");
	moveto(22, 0);
	prints("¨t¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨s");
	moveto(23, 0);
	prints(" [\x1b[1;32mCtrl+C\x1b[m]ÍË³ö");

	iflush();

	return 0;
}

int bbsnet_selchange(int new_pos)
{
	moveto(20, 0);
	clrtoeol();
	prints("¨U\x1b[1mµ¥Î»:\x1b[1;33m%-18s\x1b[m  Õ¾Ãû:\x1b[1;33m%s\x1b[m",
		   bbsnet_conf[new_pos].host2, bbsnet_conf[new_pos].host1);
	moveto(20, 79);
	prints("¨U");
	moveto(21, 0);
	clrtoeol();
	prints("¨U\x1b[1mÁ¬Íù:\x1b[1;33m%-20s", bbsnet_conf[new_pos].ip);
	if (bbsnet_conf[new_pos].port != 23)
		prints("  %d", bbsnet_conf[new_pos].port);
	prints("\x1b[m");
	moveto(21, 79);
	prints("¨U");
	iflush();

	return 0;
}

int bbs_net()
{
	int ch, pos, i;
	char file_config[256];

	strcpy(file_config, app_home_dir);
	strcat(file_config, "conf/bbsnet.conf");

	load_bbsnet_conf(file_config);

	BBS_last_access_tm = time(0);

	clearscr();
	bbsnet_refresh();
	pos = bbsnet_menu.p_menu[0]->item_cur_pos;
	display_menu(get_menu(&bbsnet_menu, "BBSNET"));
	bbsnet_selchange(pos);

	while (1)
	{
		ch = igetch(0);
		switch (ch)
		{
		case KEY_NULL:
		case Ctrl('C'):
			return 0;
		case KEY_TIMEOUT:
			if (time(0) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				return -1;
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
				menu_control(&bbsnet_menu, KEY_UP);
			pos = bbsnet_menu.p_menu[0]->item_cur_pos;
			bbsnet_selchange(pos);
			break;
		case KEY_DOWN:
			for (i = 0; i < STATION_PER_LINE; i++)
				menu_control(&bbsnet_menu, KEY_DOWN);
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
