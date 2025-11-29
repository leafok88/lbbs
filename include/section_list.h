/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * section_list
 *   - data models and basic operations of section and article
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _SECTION_LIST_H_
#define _SECTION_LIST_H_

#include "bbs.h"
#include "common.h"
#include "menu.h"
#include <time.h>

#ifdef HAVE_SYSTEM_V
#include <sys/sem.h>
#else
#include <semaphore.h>
#endif

enum section_list_constant_t
{
	BBS_article_title_max_len = 160,
	BBS_article_limit_per_section = 50000,
	BBS_article_limit_per_page = 20,

	BBS_ontop_article_limit_per_section = 10,

	BBS_article_count_per_block = 1000,
};

struct article_t
{
	int32_t aid;
	int32_t tid;
	int32_t sid;
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
	int8_t transship;
	char username[BBS_username_max_len + 1];
	char nickname[BBS_nickname_max_len + 1];
	char title[BBS_article_title_max_len + 1];
	time_t sub_dt;
};
typedef struct article_t ARTICLE;

struct section_list_t
{
	int32_t sid;
	int32_t class_id;
	int8_t enable;
	char sname[BBS_section_name_max_len + 1];
	char stitle[BBS_section_title_max_len + 1];
	char master_list[(BBS_username_max_len + 1) * 3 + 1];
	int read_user_level;
	int write_user_level;
	int32_t article_count;
	int32_t topic_count;
	int32_t visible_article_count;
	int32_t visible_topic_count;
	ARTICLE *p_article_head;
	ARTICLE *p_article_tail;
	ARTICLE *p_page_first_article[BBS_article_limit_per_section / BBS_article_limit_per_page];
	int32_t page_count;
	int32_t last_page_visible_article_count;
	ARTICLE *p_ontop_articles[BBS_ontop_article_limit_per_section];
	int32_t ontop_article_count;
	MENU_SET ex_menu_set;
	time_t ex_menu_tm;
};
typedef struct section_list_t SECTION_LIST;

struct section_list_pool_t
{
	size_t shm_size;
	SECTION_LIST sections[BBS_max_section];
	int section_count;
#ifndef HAVE_SYSTEM_V
	sem_t sem[BBS_max_section + 1];
	uint16_t read_lock_count[BBS_max_section + 1];
	uint16_t write_lock_count[BBS_max_section + 1];
#else
	int semid;
#endif
	TRIE_NODE *p_trie_dict_section_by_name;
	TRIE_NODE *p_trie_dict_section_by_sid;
};
typedef struct section_list_pool_t SECTION_LIST_POOL;

extern SECTION_LIST_POOL *p_section_list_pool;

// article_block_count * BBS_article_count_per_block should be
// no less than BBS_article_limit_per_section * BBS_max_section,
// in order to allocate enough memory for blocks
extern int article_block_init(const char *filename, int block_count);
extern void article_block_cleanup(void);

extern int set_article_block_shm_readonly(void);
extern int detach_article_block_shm(void);

extern int article_block_reset(void);

extern ARTICLE *article_block_find_by_aid(int32_t aid);
extern ARTICLE *article_block_find_by_index(int index);

extern int32_t article_block_last_aid(void);
extern int article_block_article_count(void);

extern int article_count_of_topic(int32_t aid);

extern int section_list_init(const char *filename);
extern void section_list_cleanup(void);

extern int set_section_list_shm_readonly(void);
extern int detach_section_list_shm(void);

extern SECTION_LIST *section_list_create(int32_t sid, const char *sname, const char *stitle, const char *master_list);
extern int section_list_update(SECTION_LIST *p_section, const char *sname, const char *stitle, const char *master_list);
extern void section_list_reset_articles(SECTION_LIST *p_section);
extern SECTION_LIST *section_list_find_by_name(const char *sname);
extern SECTION_LIST *section_list_find_by_sid(int32_t sid);
extern int get_section_index(SECTION_LIST *p_section);

// sname could be NULL, or pointer to char[] in size >= sizeof(SECTION_LIST.sname)
// stitle could be NULL, or pointer to char[] in size >= sizeof(SECTION_LIST.stitle)
// master_list could be NULL, or pointer to char[] in size >= sizeof(SECTION_LIST.master_list)
extern int get_section_info(SECTION_LIST *p_section, char *sname, char *stitle, char *master_list);

extern int section_list_append_article(SECTION_LIST *p_section, const ARTICLE *p_article_src);
extern int section_list_set_article_visible(SECTION_LIST *p_section, int32_t aid, int8_t visible);
extern int section_list_set_article_excerption(SECTION_LIST *p_section, int32_t aid, int8_t excerption);

extern int section_list_update_article_ontop(SECTION_LIST *p_section, ARTICLE *p_article);
extern int section_list_page_count_with_ontop(SECTION_LIST *p_section);
extern int section_list_page_article_count_with_ontop(SECTION_LIST *p_section, int32_t page_id);

// *p_page, *p_offset will be set as page / offset of the article with aid in *p_section (including both visible and invisible articles)
// *pp_next will be set as pointer to the next article of the article with aid
// *pp_next will be set as head article of the section if the article with aid locates at the tail of the section
extern ARTICLE *section_list_find_article_with_offset(SECTION_LIST *p_section, int32_t aid, int32_t *p_page, int32_t *p_offset, ARTICLE **pp_next);

extern int section_list_calculate_page(SECTION_LIST *p_section, int32_t start_aid);
extern int section_list_move_topic(SECTION_LIST *p_section_src, SECTION_LIST *p_section_dest, int32_t aid);

extern int section_list_try_rd_lock(SECTION_LIST *p_section, int wait_sec);
extern int section_list_try_rw_lock(SECTION_LIST *p_section, int wait_sec);
extern int section_list_rd_unlock(SECTION_LIST *p_section);
extern int section_list_rw_unlock(SECTION_LIST *p_section);
extern int section_list_rd_lock(SECTION_LIST *p_section);
extern int section_list_rw_lock(SECTION_LIST *p_section);

#endif //_SECTION_LIST_H_
