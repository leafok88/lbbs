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

#include "section_list.h"
#include <mysql.h>

#define ERR_UNKNOWN_SECTION -101
#define LOAD_ARTICLE_COUNT_LIMIT 1000

extern int section_list_loader_pid;
extern int last_article_op_log_mid;

extern int load_section_config_from_db(void);

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

extern int section_list_loader_reload(void);

// Return on success : real page_id (>= 0)
//           failure : error number (< 0)
extern int query_section_articles(SECTION_LIST *p_section, int32_t page_id, ARTICLE *p_articles[], int32_t *p_article_count);

#endif //_SECTION_LIST_LOADER_H_
