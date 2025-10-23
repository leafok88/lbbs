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
#include "common.h"
#include "user_priv.h"
#include <limits.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

#define BBS_port_default 23
#define BBS_ssh_port_default 22

// BBS enviroment
char BBS_id[BBS_id_max_len + 1] = "Example BBS";
char BBS_name[BBS_name_max_len + 1] = "Example Site Name";
char BBS_server[BBS_server_max_len + 1] = "bbs.example.com";
char BBS_address[BBS_address_max_len + 1] = "0.0.0.0";
in_port_t BBS_port[2] = {BBS_port_default, BBS_ssh_port_default};
int BBS_max_client = MAX_CLIENT_LIMIT;
int BBS_max_client_per_ip = MAX_CLIENT_PER_IP_LIMIT;
char BBS_start_dt[BBS_start_dt_max_len + 1] = "2000年 1月 1日";
int BBS_sys_id = 1;

const int BBS_section_list_load_interval = 5; // second

// User
const int BBS_user_list_load_interval = 60;		  // 1 minute
const int BBS_user_online_list_load_interval = 5; // 5 seconds
const int BBS_user_off_line = 900;				  // 15 minutes

char BBS_username[BBS_username_max_len + 1];
char BBS_nickname[BBS_nickname_max_len + 1];
char BBS_user_tz[BBS_user_tz_max_len + 1];
int BBS_user_exp;

time_t BBS_login_tm;
time_t BBS_last_access_tm;

char BBS_current_action[BBS_current_action_max_len + 1];
time_t BBS_current_action_tm;

const int BBS_current_action_refresh_interval = 60; // 1 minute

static const int astro_dates[] = {
	21, 20, 21, 21, 22, 22, 23, 24, 24, 24, 23, 22};

static const char *astro_names[] = {
	"摩羯", "水瓶", "双鱼", "白羊", "金牛", "双子", "巨蟹", "狮子", "处女", "天秤", "天蝎", "射手", "摩羯"};

static const int BBS_user_level_points[] = {
	INT_MIN, // 0
	50,		 // 1
	200,	 // 2
	500,	 // 3
	1000,	 // 4
	2000,	 // 5
	5000,	 // 6
	10000,	 // 7
	20000,	 // 8
	30000,	 // 9
	50000,	 // 10
	60000,	 // 11
	70000,	 // 12
	80000,	 // 13
	90000,	 // 14
	100000,	 // 15
	INT_MAX, // 16
};

static const char *BBS_user_level_names[] = {
	"新手上路", // 0
	"初来乍练", // 1
	"白手起家", // 2
	"略懂一二", // 3
	"小有作为", // 4
	"对答如流", // 5
	"精于此道", // 6
	"博大精深", // 7
	"登峰造极", // 8
	"论坛砥柱", // 9
	"☆☆☆☆☆",	// 10
	"★☆☆☆☆",	// 11
	"★★☆☆☆",	// 12
	"★★★☆☆",	// 13
	"★★★★☆",	// 14
	"★★★★★",	// 15
};

static const int BBS_user_level_cnt = sizeof(BBS_user_level_names) / sizeof(const char *);

const char *get_astro_name(time_t birthday)
{
	struct tm tm_birth;

	gmtime_r(&birthday, &tm_birth);

	if (tm_birth.tm_mday < astro_dates[tm_birth.tm_mon])
	{
		return astro_names[tm_birth.tm_mon];
	}

	return astro_names[tm_birth.tm_mon + 1];
}

int get_user_level(int point)
{
	int left;
	int right;
	int mid;

	left = 0;
	right = BBS_user_level_cnt - 1;

	while (left < right)
	{
		mid = (left + right) / 2;
		if (point < BBS_user_level_points[mid + 1])
		{
			right = mid;
		}
		else if (point > BBS_user_level_points[mid + 1])
		{
			left = mid + 1;
		}
		else // if (point == user_level_points[mid])
		{
			left = mid + 1;
			break;
		}
	}

	return left;
}

const char *get_user_level_name(int level)
{
	return BBS_user_level_names[level];
}

char *setuserfile(char *buf, size_t len, const char *filename)
{
	snprintf(buf, len, "%s/%d", filename, BBS_priv.uid);
	return buf;
}
