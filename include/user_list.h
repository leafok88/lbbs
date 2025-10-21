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

struct user_info_t
{
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

struct user_list_t
{
	USER_INFO users[BBS_max_user_count];
	int32_t user_count;
};
typedef struct user_list_t USER_LIST;

extern int user_list_load(MYSQL *db, USER_LIST *p_list);

extern int user_list_pool_init(void);
extern void user_list_pool_cleanup(void);
extern int set_user_list_pool_shm_readonly(void);
extern int detach_user_list_pool_shm(void);

extern int user_list_pool_reload(void);

extern int user_list_try_rd_lock(int wait_sec);
extern int user_list_try_rw_lock(int wait_sec);
extern int user_list_rd_unlock(void);
extern int user_list_rw_unlock(void);
extern int user_list_rd_lock(void);
extern int user_list_rw_lock(void);

#endif //_USER_LIST_H_
