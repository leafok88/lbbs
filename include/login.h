/***************************************************************************
						  login.h  -  description
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

#ifndef _LOGIN_H_
#define _LOGIN_H_

#include <mysql/mysql.h>

#define BBS_login_retry_times 3
#define BBS_allowed_login_failures_within_interval 10
#define BBS_login_failures_count_interval 10 // minutes
#define BBS_allowed_login_failures_per_account 3

extern void login_fail(void);

extern int bbs_login(void);

extern int check_user(const char *username, const char *password);

extern int load_user_info(MYSQL *db, int BBS_uid);

extern int load_guest_info(void);

extern int user_online_add(MYSQL *db);
extern int user_online_del(MYSQL *db);

extern int user_online_update(const char *action);

#endif //_LOGIN_H_
