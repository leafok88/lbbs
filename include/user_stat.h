/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * user_stat
 *   - data model and basic operations of user related statistics
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifndef _USER_STAT_H_
#define _USER_STAT_H_

#include "bbs.h"
#include <sys/types.h>

struct user_stat_t
{
	int32_t uid;
	int article_count;
};
typedef struct user_stat_t USER_STAT;

struct user_stat_map_t
{
	USER_STAT stat_list[BBS_max_user_count];
	int user_count;
	int32_t last_uid;
	int last_article_index;
};
typedef struct user_stat_map_t USER_STAT_MAP;

extern int user_stat_map_init(USER_STAT_MAP *p_map);
extern int user_stat_map_update(USER_STAT_MAP *p_map);

extern int user_stat_article_cnt_inc(USER_STAT_MAP *p_map, int32_t uid, int n);
extern int user_stat_get(USER_STAT_MAP *p_map, int32_t uid, const USER_STAT **pp_stat);

#endif //_USER_STAT_H_
