/***************************************************************************
					   section_list.h  -  description
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

#include "common.h"
#include "bbs.h"

#define BBS_article_title_max_len 80

// Limit of articles (including those marked as deleted) per section = BBS_article_limit_per_block * BBS_article_block_limit_per_section
#define BBS_article_limit_per_block 1000
#define BBS_article_block_limit_per_section 50
#define BBS_article_limit_per_page 20

struct article_t
{
	int32_t aid;
	int32_t tid;
	int32_t cid;
	int32_t uid;
	struct article_t *p_prior; // prior article
	struct article_t *p_next; // next article
	struct article_t * p_topic_prior; // same topic
	struct article_t * p_topic_next; // same topic
	int8_t visible;
	int8_t excerption;
	int8_t ontop;
	int8_t lock;
	char username[BBS_username_max_len + 1];
	char nickname[BBS_nickname_max_len + 1];
	char title[BBS_article_title_max_len + 1];
};
typedef struct article_t ARTICLE;

struct article_block_t
{
	ARTICLE articles[BBS_article_limit_per_block];
	int32_t article_count;
	struct article_block_t *p_next_block;
};
typedef struct article_block_t ARTICLE_BLOCK;

struct section_data_t
{
	int32_t sid;
	char sname[BBS_section_name_max_len + 1];
	char stitle[BBS_section_title_max_len + 1];
	char master_name[BBS_username_max_len + 1];
	ARTICLE_BLOCK *p_head_block;
	ARTICLE_BLOCK *p_tail_block;
	ARTICLE_BLOCK *p_block[BBS_article_block_limit_per_section];
	int32_t block_count;
	int32_t block_head_aid[BBS_article_block_limit_per_section];
	int32_t article_count;
	ARTICLE *p_article_head;
	ARTICLE *p_article_tail;
	int32_t page_head_aid[BBS_article_limit_per_block * BBS_article_block_limit_per_section / BBS_article_limit_per_page];
	ARTICLE *p_page_head_article[BBS_article_limit_per_block * BBS_article_block_limit_per_section / BBS_article_limit_per_page];
	int32_t page_count;
	int32_t last_page_article_count;
};
typedef struct section_data_t SECTION_DATA;

extern int section_data_pool_init(const char *filename, int article_block_count);
extern void section_data_pool_cleanup(void);

extern SECTION_DATA *section_data_create(const char *sname, const char *stitle, const char *master_name);
extern int section_data_free_block(SECTION_DATA *p_section);
extern SECTION_DATA *section_data_find_section_by_name(const char *sname);

extern int section_data_append_article(SECTION_DATA *p_section, const ARTICLE *p_article_src);
extern ARTICLE *section_data_find_article_by_aid(SECTION_DATA *p_section, int32_t aid);
extern ARTICLE *section_data_find_article_by_index(SECTION_DATA *p_section, int index);
extern int section_data_mark_del_article(SECTION_DATA *p_section, int32_t aid);
