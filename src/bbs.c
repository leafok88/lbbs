/***************************************************************************
							bbs.c  -  description
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

#include "bbs.h"
#include <signal.h>
#include <time.h>

// BBS enviroment
char BBS_id[20] = "";
char BBS_name[50] = "";
char BBS_server[256] = "";
char BBS_address[50] = "";
unsigned int BBS_port = 23;
long BBS_max_client = 256;
long BBS_max_user = 10000;
char BBS_start_dt[50] = "2004Äê 1ÔÂ 1ÈÕ";

char BBS_username[BBS_max_username_length];
BBS_user_priv BBS_priv;
int BBS_passwd_complex = 0;
int BBS_user_money = 0;

time_t BBS_login_tm;
time_t BBS_last_access_tm;
time_t BBS_last_sub_tm;

char BBS_current_section_name[20];

char *
setuserfile(char *buf, const char *filename)
{
	sprintf(buf, "data/%s/%ld", filename, BBS_priv.uid);
	return buf;
}

char *
sethomefile(char *buf, long int uid, char *filename)
{
	sprintf(buf, "data/%s/%ld", filename, uid);
	return buf;
}
