/***************************************************************************
							bbs.h  -  description
							 -------------------
	begin                : Mon Oct 18 2004
	copyright            : (C) 2004 by Leaflet
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

#ifndef _BBS_H_
#define _BBS_H_

#include <time.h>
#include <netinet/in.h>

// BBS
#define BBS_max_section 1024
#define BBS_username_max_len 12
#define BBS_password_max_len 12

extern char BBS_id[20];
extern char BBS_name[50];
extern char BBS_server[256];
extern char BBS_address[50];
extern in_port_t BBS_port;
extern unsigned int BBS_max_client;
extern unsigned int BBS_max_user;
extern char BBS_start_dt[50];

// Session
#define MAX_DELAY_TIME 600

extern char BBS_username[BBS_username_max_len + 1];

extern time_t BBS_login_tm;
extern time_t BBS_last_access_tm;

extern char BBS_current_section_name[20];

extern char *setuserfile(char *buf, size_t len, const char *filename);

#endif //_BBS_H_
