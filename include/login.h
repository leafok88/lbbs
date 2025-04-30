/***************************************************************************
						  login.h  -  description
							 -------------------
	begin                : Wed Mar 16 2004
	copyright            : (C) 2005 by Leaflet
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

#ifndef _LOGIN_H_
#define _LOGIN_H_

#include <mysql.h>

extern void login_fail();

extern int bbs_login();

extern int check_user(MYSQL *db, char *username, char *password);

extern int load_user_info(MYSQL *db, long int BBS_uid);

extern int load_guest_info(MYSQL *db);

#endif //_LOGIN_H_
