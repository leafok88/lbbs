/***************************************************************************
						  common.c  -  description
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

#include "common.h"
#include "log.h"
#include "menu.h"
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

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
int SYS_exit;
int SYS_child_process_count;

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

const char *get_time_str(char *string, size_t length)
{
	char week[20];
	char buffer[LINE_BUFFER_LEN];
	time_t curtime;
	struct tm *loctime;

	curtime = time(NULL);
	loctime = localtime(&curtime);

	strftime(buffer, sizeof(buffer), "%Y年%m月%d日%H:%M:%S ", loctime);

	switch (loctime->tm_wday)
	{
	case 0:
		strncpy(week, "星期天", sizeof(week));
		break;
	case 1:
		strncpy(week, "星期一", sizeof(week));
		break;
	case 2:
		strncpy(week, "星期二", sizeof(week));
		break;
	case 3:
		strncpy(week, "星期三", sizeof(week));
		break;
	case 4:
		strncpy(week, "星期四", sizeof(week));
		break;
	case 5:
		strncpy(week, "星期五", sizeof(week));
		break;
	case 6:
		strncpy(week, "星期六", sizeof(week));
		break;
	}
	strncat(buffer, week, sizeof(buffer) - 1 - strnlen(buffer, sizeof(buffer)));

	strncpy(string, buffer, length);

	return string;
}

void reload_bbs_menu(int i)
{
	if (reload_menu(&bbs_menu) < 0)
		log_error("Reload menu failed\n");
	else
		log_std("Reload menu successfully\n");
}

void system_exit(int i)
{
	SYS_exit = 1;
}

void child_exit(int i)
{
	int pid;

	pid = wait(0);

	if (pid > 0)
	{
		SYS_child_process_count--;
		log_std("Child process (%d) exited\n", pid);
	}
}
