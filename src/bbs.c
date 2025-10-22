/***************************************************************************
							bbs.c  -  description
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
#include "user_priv.h"
#include <stdio.h>
#include <signal.h>
#include <time.h>

#define BBS_port_default 23
#define BBS_ssh_port_default 22

// BBS enviroment
char BBS_id[BBS_id_max_len + 1] = "Example BBS";
char BBS_name[BBS_name_max_len + 1] = "Example Site Name";
char BBS_server[BBS_server_max_len + 1] = "bbs.example.com";
char BBS_address[BBS_address_max_len + 1] = "0.0.0.0";
in_port_t BBS_port[2] = {BBS_port_default, BBS_ssh_port_default};
int BBS_max_client = MAX_CLIENT_LIMIT;
int BBS_max_client_per_ip = MAX_CLIENT_PER_IP_LIMIT;
char BBS_start_dt[BBS_start_dt_max_len + 1] = "2000年 1月 1日";
int BBS_sys_id = 1;

const int BBS_section_list_load_interval = 5; // second

// User
const int BBS_user_list_load_interval = 60;		  // 1 minute
const int BBS_user_online_list_load_interval = 5; // 5 seconds
const int BBS_user_off_line = 900;				  // 15 minutes

char BBS_username[BBS_username_max_len + 1];
char BBS_nickname[BBS_nickname_max_len + 1];
char BBS_user_tz[BBS_user_tz_max_len + 1];
int BBS_user_exp;

time_t BBS_login_tm;
time_t BBS_last_access_tm;

char BBS_current_action[BBS_current_action_max_len + 1];
time_t BBS_current_action_tm;

const int BBS_current_action_refresh_interval = 60; // 1 minute

char *setuserfile(char *buf, size_t len, const char *filename)
{
	snprintf(buf, len, "%s/%d", filename, BBS_priv.uid);
	return buf;
}
