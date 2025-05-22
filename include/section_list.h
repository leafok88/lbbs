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
#define BBS_article_limit_per_section 50000
#define BBS_article_limit_per_page 20

#define ARTICLE_PER_BLOCK 1000

struct article_t
{
	int32_t aid;
	int32_t tid;
	int32_t cid;
	int32_t uid;
	struct article_t *p_prior;		 // prior article
	struct article_t *p_next;		 // next article
	struct article_t *p_topic_prior; // same topic
	struct article_t *p_topic_next;	 // same topic
	int8_t visible;
	int8_t excerption;
	int8_t ontop;
	int8_t lock;
	char username[BBS_username_max_len + 1];
	char nickname[BBS_nickname_max_len + 1];
	char title[BBS_article_title_max_len + 1];
};
typedef struct article_t ARTICLE;

struct section_list_t
{
	int32_t sid;
	char sname[BBS_section_name_max_len + 1];
	char stitle[BBS_section_title_max_len + 1];
	char master_name[BBS_username_max_len + 1];
	int32_t article_count;
	ARTICLE *p_article_head;
	ARTICLE *p_article_tail;
	ARTICLE *p_page_first_article[BBS_article_limit_per_section / BBS_article_limit_per_page];
	int32_t page_count;
	int32_t last_page_article_count;
};
typedef struct section_list_t SECTION_LIST;

// article_block_count * ARTICLE_PER_BLOCK should be
// no less than BBS_article_limit_per_section * BBS_max_section,
// in order to allocate enough memory for blocks
extern int article_block_init(const char *filename, int block_count);
extern void article_block_cleanup(void);
extern int article_block_reset(void);

extern ARTICLE *article_block_find_by_aid(int32_t aid);
extern ARTICLE *article_block_find_by_index(int index);

extern SECTION_LIST *section_list_create(const char *sname, const char *stitle, const char *master_name);
extern void section_list_reset_articles(SECTION_LIST *p_section);
extern void section_list_cleanup(void);
extern SECTION_LIST *section_list_find_by_name(const char *sname);

extern int section_list_append_article(SECTION_LIST *p_section, const ARTICLE *p_article_src);
extern int section_list_set_article_visible(SECTION_LIST *p_section, int32_t aid, int8_t visible);
