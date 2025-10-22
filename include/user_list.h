/***************************************************************************
						 user_list.h  -  description
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

#ifndef _USER_LIST_H_
#define _USER_LIST_H_

#include "bbs.h"
#include <mysql/mysql.h>

#define BBS_user_limit_per_page 20
#define BBS_session_id_length 32

struct user_info_t
{
	int32_t id;
	int32_t uid;
	char username[BBS_username_max_len + 1];
	char nickname[BBS_nickname_max_len + 1];
	char gender;
	int8_t gender_pub;
	int32_t life;
	int32_t exp;
	time_t signup_dt;
	time_t last_login_dt;
	time_t birthday;
};
typedef struct user_info_t USER_INFO;

struct user_info_index_uid_t
{
	int32_t uid;
	int32_t id;
};
typedef struct user_info_index_uid_t USER_INFO_INDEX_UID;

struct user_list_t
{
	USER_INFO users[BBS_max_user_count];
	USER_INFO_INDEX_UID index_uid[BBS_max_user_count];
	int32_t user_count;
};
typedef struct user_list_t USER_LIST;

struct user_online_info_t
{
	int32_t id;
	char session_id[BBS_session_id_length + 1];
	USER_INFO user_info;
	char ip[IP_ADDR_LEN];
	char current_action[BBS_current_action_max_len + 1];
	const char *current_action_title;
	time_t login_tm;
	time_t last_tm;
};
typedef struct user_online_info_t USER_ONLINE_INFO;

struct user_online_list_t
{
	USER_ONLINE_INFO users[BBS_max_user_online_count];
	USER_INFO_INDEX_UID index_uid[BBS_max_user_count];
	int32_t user_count;
};
typedef struct user_online_list_t USER_ONLINE_LIST;

extern int user_list_pool_init(void);
extern void user_list_pool_cleanup(void);
extern int set_user_list_pool_shm_readonly(void);
extern int detach_user_list_pool_shm(void);

extern int user_list_pool_reload(int online_user);

extern int query_user_list(int page_id, USER_INFO *p_users, int *p_user_count, int *p_page_count);
extern int query_user_online_list(int page_id, USER_ONLINE_INFO *p_online_users, int *p_user_count, int *p_page_count);

extern int query_user_info(int32_t id, USER_INFO *p_user);
extern int query_user_info_by_uid(int32_t uid, USER_INFO *p_user);

extern int query_user_online_info(int32_t id, USER_ONLINE_INFO *p_user);
extern int query_user_online_info_by_uid(int32_t uid, USER_ONLINE_INFO *p_users, int *p_user_cnt, int start_id);

#endif //_USER_LIST_H_
