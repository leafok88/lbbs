/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * login
 *   - user authentication and online status manager
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _LOGIN_H_
#define _LOGIN_H_

#include <mysql.h>

extern const int BBS_login_retry_times;

extern void login_fail(void);

extern int bbs_login(void);

extern int check_user(const char *username, const char *password);

extern int load_user_info(MYSQL *db, int BBS_uid);

extern int load_guest_info(void);

extern int user_online_add(MYSQL *db);
extern int user_online_del(MYSQL *db);
extern int user_online_exp(MYSQL *db);

extern int user_online_update(const char *action);

#endif //_LOGIN_H_
