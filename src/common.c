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

#include "menu.h"
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

// Version information
char app_version[256] = "LBBS-devel version 1.0";

// Global declaration for enviroment
char app_home_dir[256];
char app_temp_dir[256];

// Global declaration for sockets
int socket_server;
int socket_client;
char hostaddr_server[50];
char hostaddr_client[50];
int port_server;
int port_client;

// Global declaration for database
char DB_host[256];
char DB_username[50];
char DB_password[50];
char DB_database[50];
char DB_timezone[50];

// Global declaration for system
int SYS_exit;
int SYS_child_process_count;

// Common function
const char *
str_space(char *string, int length)
{
	int i;
	for (i = 0; i < length; i++)
	{
		string[i] = ' ';
	}
	string[length] = '\0';
	return string;
}

const char *
get_time_str(char *string, size_t length)
{
	char week[10], buffer[256];
	time_t curtime;
	struct tm *loctime;

	curtime = time(NULL);
	loctime = localtime(&curtime);

	strftime(buffer, 256, "%Y年%m月%d日%H:%M:%S ", loctime);

	switch (loctime->tm_wday)
	{
	case 0:
		strcpy(week, "星期天");
		break;
	case 1:
		strcpy(week, "星期一");
		break;
	case 2:
		strcpy(week, "星期二");
		break;
	case 3:
		strcpy(week, "星期三");
		break;
	case 4:
		strcpy(week, "星期四");
		break;
	case 5:
		strcpy(week, "星期五");
		break;
	case 6:
		strcpy(week, "星期六");
		break;
	}
	strcat(buffer, week);

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
