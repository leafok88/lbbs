/***************************************************************************
						  common.h  -  description
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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stddef.h>

#define LINE_BUFFER_LEN 1024
#define FILE_PATH_LEN 4096
#define MAX_FILE_LINES 65536

// Version
extern char app_version[256];

// Enviroment
#define CONF_BBSD "conf/bbsd.conf"
#define CONF_MENU "conf/menu.conf"
#define CONF_BBSNET "conf/bbsnet.conf"

#define LOG_FILE_INFO "log/bbsd.log"
#define LOG_FILE_ERROR "log/error.log"

#define DATA_WELCOME "data/welcome.txt"
#define DATA_REGISTER "data/register.txt"
#define DATA_GOODBYE "data/goodbye.txt"
#define DATA_LICENSE "data/license.txt"
#define DATA_COPYRIGHT "data/copyright.txt"
#define DATA_LOGIN_ERROR "data/login_error.txt"
#define DATA_ACTIVE_BOARD "data/active_board.txt"
#define DATA_READ_HELP "data/read_help.txt"

#define VAR_MAX_USER_ONLINE "var/max_user_online.dat"

// Network
#define IP_ADDR_LEN 50

extern int socket_server;
extern int socket_client;
extern char hostaddr_server[IP_ADDR_LEN];
extern char hostaddr_client[IP_ADDR_LEN];
extern int port_server;
extern int port_client;

// Signal
#define SIG_RELOAD_MENU 0x22

extern const char *str_space(char *string, int length);
extern const char *get_time_str(char *string, size_t length);

// Signal handler
extern void reload_bbs_menu(int);
extern void system_exit(int);
extern void child_exit(int);

// System
extern int SYS_exit;
extern int SYS_child_process_count;

// Network
extern const char * ip_mask(char * s, int level, char mask);

#endif //_COMMON_H_
