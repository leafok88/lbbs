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

#include "section_list.h"
#include <mysql.h>

extern int load_section_config_from_db(MYSQL *db);

// Input global_lock = 0 : lock / unlock corresponding section per article
//                     1 : lock / unlock all sections per invocation
extern int append_articles_from_db(MYSQL *db, int32_t start_aid, int global_lock);

// Return on success : real page_id (>= 0)
//           failure : error number (< 0)
extern int query_section_articles(SECTION_LIST *p_section, int32_t page_id, ARTICLE *p_articles[], int32_t *p_article_count);
