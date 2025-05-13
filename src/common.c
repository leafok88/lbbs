/***************************************************************************
						  common.c  -  description
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
#include "log.h"
#include "menu.h"
#include <string.h>
#include <time.h>
#include <sys/types.h>

// Version information
char app_version[256] = "LBBS-devel version 1.0";

// Global declaration for sockets
int socket_server;
int socket_client;
char hostaddr_server[50];
char hostaddr_client[50];
int port_server;
int port_client;

// Global declaration for system
volatile int SYS_server_exit = 0;
volatile int SYS_child_process_count = 0;
volatile int SYS_child_exit = 0;
volatile int SYS_menu_reload = 0;

// Common function
const char *str_space(char *string, int length)
{
	int i;
	for (i = 0; i < length; i++)
	{
		string[i] = ' ';
	}
	string[length] = '\0';
	return string;
}

const char *get_time_str(char *s, size_t len)
{
	time_t curtime = time(NULL);
	struct tm *loctime;
	loctime = localtime(&curtime);

	size_t j = strftime(s, len, "%Y年%m月%d日%H:%M:%S ", loctime);

	if (j == 0)
	{
		return NULL;
	}

	switch (loctime->tm_wday)
	{
	case 0:
		strncat(s, "星期天", len - j);
		break;
	case 1:
		strncat(s, "星期一", len - j);
		break;
	case 2:
		strncat(s, "星期二", len - j);
		break;
	case 3:
		strncat(s, "星期三", len - j);
		break;
	case 4:
		strncat(s, "星期四", len - j);
		break;
	case 5:
		strncat(s, "星期五", len - j);
		break;
	case 6:
		strncat(s, "星期六", len - j);
		break;
	}

	return s;
}

void sig_hup_handler(int i)
{
	SYS_menu_reload = 1;
}

void sig_term_handler(int i)
{
	SYS_server_exit = 1;
}

void sig_chld_handler(int i)
{
	SYS_child_exit = 1;
}

const char * ip_mask(char * s, int level, char mask)
{
	char * p = s;

	if (level <= 0)
	{
		return s;
	}
	if (level > 4)
	{
		level = 4;
	}

	for (int i = 0; i < 4 - level; i++)
	{
		p = strchr(p, '.');
		if (p == NULL)
		{
			return s;
		}
		p++;
	}

	for (int i = 0; i < level; i++)
	{
		*p = mask;
		p++;
		if (i < level - 1)
		{
			*p = '.';
			p++;
		}
	}
	*p = '\0';

	return s;
}
