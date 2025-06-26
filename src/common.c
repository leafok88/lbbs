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
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

// File loader
const char *data_files_load_startup[] = {
	DATA_WELCOME,
	DATA_REGISTER,
	DATA_GOODBYE,
	DATA_LICENSE,
	DATA_COPYRIGHT,
	DATA_LOGIN_ERROR,
	DATA_ACTIVE_BOARD,
	DATA_READ_HELP,
	VAR_BBS_TOP};
int data_files_load_startup_count = 9; // Count of data_files_load_startup[]

// Global declaration for sockets
int socket_server[2];
int socket_client;
char hostaddr_client[IP_ADDR_LEN];
int port_client;

// SSHv2
int SSH_v2 = 0;
ssh_bind sshbind;
ssh_session SSH_session;
ssh_channel SSH_channel;

// Global declaration for system
volatile int SYS_server_exit = 0;
volatile int SYS_child_process_count = 0;
volatile int SYS_child_exit = 0;
volatile int SYS_conf_reload = 0;

static const char *weekday[] = {
	"天", "一", "二", "三", "四", "五", "六"};

// Common function
const char *get_time_str(char *s, size_t len)
{
	time_t curtime;
	struct tm local_tm;

	time(&curtime);
	localtime_r(&curtime, &local_tm);
	size_t j = strftime(s, len, "%b %d %H:%M 星期", &local_tm);

	if (j == 0 || j + strlen(weekday[local_tm.tm_wday]) + 1 > len)
	{
		return NULL;
	}

	strncat(s, weekday[local_tm.tm_wday], len - 1 - j);

	return s;
}

void sig_hup_handler(int i)
{
	SYS_conf_reload = 1;
}

void sig_term_handler(int i)
{
	SYS_server_exit = 1;
}

void sig_chld_handler(int i)
{
	SYS_child_exit = 1;
}

const char *ip_mask(char *s, int level, char mask)
{
	char *p = s;

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
