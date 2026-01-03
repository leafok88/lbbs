/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bbs
 *   - BBS related common definitions
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifndef _BBS_H_
#define _BBS_H_

#include <time.h>
#include <netinet/in.h>

enum bbs_const_t
{
    BBS_max_section = 200,
    BBS_section_name_max_len = 20,
    BBS_section_title_max_len = 40,
    BBS_username_max_len = 12,
    BBS_password_max_len = 12,
    BBS_nickname_max_len = 40,
    BBS_user_tz_max_len = 50,

    BBS_msg_max_len = 1024,

    BBS_id_max_len = 20,
    BBS_name_max_len = 50,
    BBS_server_max_len = 255,
    BBS_address_max_len = 50,
    BBS_start_dt_max_len = 50,

    BBS_max_user_count = 100000,
    BBS_max_user_online_count = 10000,

    BBS_max_user_idle_time = 600, // 10 minutes

    BBS_current_action_max_len = 20,
};

// BBS
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
extern const int BBS_user_list_load_interval;
extern const int BBS_user_online_list_load_interval;

// Session
extern const int BBS_user_off_line;

extern char BBS_username[BBS_username_max_len + 1];
extern char BBS_nickname[BBS_nickname_max_len + 1];
extern char BBS_user_tz[BBS_user_tz_max_len + 1];
extern int BBS_user_exp;

extern time_t BBS_eula_tm;
extern int BBS_update_eula;

extern time_t BBS_login_tm;
extern time_t BBS_last_access_tm;

extern char BBS_current_action[BBS_current_action_max_len + 1];
extern time_t BBS_current_action_tm;
extern const int BBS_current_action_refresh_interval;

extern const char *get_astro_name(time_t birthday);
extern int get_user_level(int point);
extern const char *get_user_level_name(int level);

extern char *setuserfile(char *buf, size_t len, const char *filename);

#endif //_BBS_H_
