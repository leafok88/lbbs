/***************************************************************************
					 article_view_log.h  -  description
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

#ifndef _ARTICLE_VIEW_LOG_H_
#define _ARTICLE_VIEW_LOG_H_

#include <stdint.h>

#define MAX_AID_INC_CNT 100000

struct article_view_log_t
{
	int32_t *aid_base;
	int aid_base_cnt;
	int32_t aid_inc[MAX_AID_INC_CNT];
	int aid_inc_cnt;
};
typedef struct article_view_log_t ARTICLE_VIEW_LOG;

// Load baseline view log from DB
extern int article_view_log_load(int uid, ARTICLE_VIEW_LOG *p_view_log, int keep_inc);
// Clear data
extern int article_view_log_unload(ARTICLE_VIEW_LOG *p_view_log);
// Save incremental view log to DB
extern int article_view_log_save_inc(int uid, const ARTICLE_VIEW_LOG *p_view_log);
// Merge incremental view log to baseline, without DB read / write
extern int article_view_log_merge_inc(ARTICLE_VIEW_LOG *p_view_log);

// Check if specific article was viewed
extern int article_view_log_is_viewed(int32_t aid, const ARTICLE_VIEW_LOG *p_view_log);
// Set specific article as viewed
extern int article_view_log_set_viewed(int32_t aid, ARTICLE_VIEW_LOG *p_view_log);

#endif //_ARTICLE_VIEW_LOG_H_
