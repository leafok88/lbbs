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
#include "user_priv.h"
#include <stdio.h>
#include <signal.h>
#include <time.h>

// BBS enviroment
char BBS_id[20] = "";
char BBS_name[50] = "";
char BBS_server[256] = "";
char BBS_address[50] = "";
in_port_t BBS_port = 23;
unsigned int BBS_max_client = 256;
unsigned int BBS_max_user = 10000;
char BBS_start_dt[50] = "2004Äê 1ÔÂ 1ÈÕ";

char BBS_username[BBS_username_max_len + 1];

time_t BBS_login_tm;
time_t BBS_last_access_tm;

char BBS_current_section_name[BBS_section_name_max_len + 1];

char *setuserfile(char *buf, size_t len, const char *filename)
{
	snprintf(buf, len, "%s/%ld", filename, BBS_priv.uid);
	return buf;
}
