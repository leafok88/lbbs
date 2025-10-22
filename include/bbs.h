/***************************************************************************
							bbs.h  -  description
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

#ifndef _BBS_H_
#define _BBS_H_

#include <time.h>
#include <netinet/in.h>

// BBS
#define BBS_max_section 200
#define BBS_section_name_max_len 20
#define BBS_section_title_max_len 40
#define BBS_username_max_len 12
#define BBS_password_max_len 12
#define BBS_nickname_max_len 40
#define BBS_user_tz_max_len 50

#define BBS_msg_max_len 1024

#define BBS_id_max_len 20
#define BBS_name_max_len 50
#define BBS_server_max_len 255
#define BBS_address_max_len 50
#define BBS_start_dt_max_len 50

extern const int BBS_section_list_load_interval;

extern char BBS_id[BBS_id_max_len + 1];
extern char BBS_name[BBS_name_max_len + 1];
extern char BBS_server[BBS_server_max_len + 1];
extern char BBS_address[BBS_address_max_len + 1];
extern in_port_t BBS_port[2];
extern int BBS_max_client;
extern int BBS_max_client_per_ip;
extern char BBS_start_dt[BBS_start_dt_max_len + 1];
extern int BBS_sys_id;

// User
#define BBS_max_user_count 100000
#define BBS_max_user_online_count 10000
extern const int BBS_user_list_load_interval;
extern const int BBS_user_online_list_load_interval;

// Session
#define MAX_DELAY_TIME 600 // 10 minutes
extern const int BBS_user_off_line;

extern char BBS_username[BBS_username_max_len + 1];
extern char BBS_nickname[BBS_nickname_max_len + 1];
extern char BBS_user_tz[BBS_user_tz_max_len + 1];
extern int BBS_user_exp;

extern time_t BBS_login_tm;
extern time_t BBS_last_access_tm;

#define BBS_current_action_max_len 20

extern char BBS_current_action[BBS_current_action_max_len + 1];
extern time_t BBS_current_action_tm;
extern const int BBS_current_action_refresh_interval;

extern char *setuserfile(char *buf, size_t len, const char *filename);

#endif //_BBS_H_
