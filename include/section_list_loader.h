/***************************************************************************
					section_list_loader.h  -  description
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

#ifndef _SECTION_LIST_LOADER_H_
#define _SECTION_LIST_LOADER_H_

#include "menu.h"
#include "section_list.h"
#include <mysql/mysql.h>

#define ERR_UNKNOWN_SECTION -101
#define LOAD_ARTICLE_COUNT_LIMIT 1000

extern int section_list_loader_pid;
extern int last_article_op_log_mid;

extern int load_section_config_from_db(int update_gen_ex);

// Input global_lock = 0 : lock / unlock corresponding section per article
//                     1 : lock / unlock all sections per invocation
// Return on success : count of appended articles (>= 0)
//           failure : lock / unlock error (-1)
//                   : unknown section found (ERR_UNKNOWN_SECTION)
extern int append_articles_from_db(int32_t start_aid, int global_lock, int article_count_limit);

extern int set_last_article_op_log_from_db(void);

// Return on success : count of appended articles (>= 0)
//           failure : lock / unlock error (-1)
//                   : unknown section found (ERR_UNKNOWN_SECTION)
extern int apply_article_op_log_from_db(int op_count_limit);

extern int section_list_loader_launch(void);

// Return on success : real page_id (>= 0)
//           failure : error number (< 0)
extern int query_section_articles(SECTION_LIST *p_section, int page_id, ARTICLE *p_articles[],
								  int *p_article_count, int *p_page_count, int *p_ontop_start_offset);

// Input direction =  0 : locate p_article_cur
//                   -1 :        p_article_cur->p_topic_prior
//                    1 :        p_article_cur->p_topic_next
// Return on success : found (1)
//                   : not found (0)
//           failure : error number (< 0)
extern int locate_article_in_section(SECTION_LIST *p_section, const ARTICLE *p_article_cur, int direction, int step,
									 int *p_page_id, int *p_visible_offset, int *p_article_count);

// Return on success : found (1)
//                   : not found (0)
//           failure : error number (< 0)
extern int scan_unread_article_in_section(SECTION_LIST *p_section, const ARTICLE *p_article_cur, const ARTICLE **pp_article_unread);

extern int scan_article_in_section_by_uid(SECTION_LIST *p_section, const ARTICLE *p_article_cur,
										  int direction, int32_t uid, const ARTICLE **pp_article);
extern int scan_article_in_section_by_username(SECTION_LIST *p_section, const ARTICLE *p_article_cur,
										  int direction, const char *username, const ARTICLE **pp_article);
extern int scan_article_in_section_by_title(SECTION_LIST *p_section, const ARTICLE *p_article_cur,
										  int direction, const char *title, const ARTICLE **pp_article);

extern int get_section_ex_menu_set(SECTION_LIST *p_section, MENU_SET *p_ex_menu_set);

#endif //_SECTION_LIST_LOADER_H_
