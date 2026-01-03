/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * article_favor
 *   - data model and basic operations of user favorite articles
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifndef _ARTICLE_FAVOR_H_
#define _ARTICLE_FAVOR_H_

#include "section_list.h"
#include <stdint.h>

enum article_favor_constant_t
{
	MAX_FAVOR_AID_BASE_CNT = 10000,
	MAX_FAVOR_AID_INC_CNT = 1000,
};

struct article_favor_t
{
	int uid;
	int32_t aid_base[MAX_FAVOR_AID_BASE_CNT];
	int aid_base_cnt;
	int32_t aid_inc[MAX_FAVOR_AID_INC_CNT];
	int aid_inc_cnt;
};
typedef struct article_favor_t ARTICLE_FAVOR;

extern ARTICLE_FAVOR BBS_article_favor;

// Load baseline article favorite from DB
extern int article_favor_load(int uid, ARTICLE_FAVOR *p_favor, int keep_inc);
// Clear data
extern int article_favor_unload(ARTICLE_FAVOR *p_favor);
// Save incremental article favorite to DB
extern int article_favor_save_inc(const ARTICLE_FAVOR *p_favor);
// Merge incremental article favorite to baseline, without DB read / write
extern int article_favor_merge_inc(ARTICLE_FAVOR *p_favor);

// Check if specific article was set as favorite
extern int article_favor_check(int32_t aid, const ARTICLE_FAVOR *p_favor);
// Set specific article as favorite
extern int article_favor_set(int32_t aid, ARTICLE_FAVOR *p_favor, int state);

extern int query_favor_articles(ARTICLE_FAVOR *p_favor, int page_id, const ARTICLE **p_articles,
								char p_snames[][BBS_section_name_max_len + 1], int *p_article_count, int *p_page_count);

#endif //_ARTICLE_FAVOR_H_
