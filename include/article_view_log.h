/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * article_view_log
 *   - data persistence and query of article view log
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _ARTICLE_VIEW_LOG_H_
#define _ARTICLE_VIEW_LOG_H_

#include <stdint.h>

enum article_view_log_constant_t
{
	MAX_VIEWED_AID_INC_CNT = 1000,
};

struct article_view_log_t
{
	int uid;
	int32_t *aid_base;
	int aid_base_cnt;
	int32_t aid_inc[MAX_VIEWED_AID_INC_CNT];
	int aid_inc_cnt;
};
typedef struct article_view_log_t ARTICLE_VIEW_LOG;

extern ARTICLE_VIEW_LOG BBS_article_view_log;

// Load baseline view log from DB
extern int article_view_log_load(int uid, ARTICLE_VIEW_LOG *p_view_log, int keep_inc);
// Clear data
extern int article_view_log_unload(ARTICLE_VIEW_LOG *p_view_log);
// Save incremental view log to DB
extern int article_view_log_save_inc(const ARTICLE_VIEW_LOG *p_view_log);
// Merge incremental view log to baseline, without DB read / write
extern int article_view_log_merge_inc(ARTICLE_VIEW_LOG *p_view_log);

// Check if specific article was viewed
extern int article_view_log_is_viewed(int32_t aid, const ARTICLE_VIEW_LOG *p_view_log);
// Set specific article as viewed
extern int article_view_log_set_viewed(int32_t aid, ARTICLE_VIEW_LOG *p_view_log);

#endif //_ARTICLE_VIEW_LOG_H_
