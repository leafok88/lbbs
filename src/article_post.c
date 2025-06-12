/***************************************************************************
						article_post.c  -  description
							 -------------------
	copyright            : (C) 2004-2025 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "article_post.h"
#include "article_cache.h"
#include "editor.h"
#include "screen.h"
#include "log.h"

int article_post(SECTION_LIST *p_section, ARTICLE *p_article, ARTICLE_POST_TYPE type)
{
	log_common("Debug: sid=%d aid=%d type=%d\n",
			   p_section->sid, (p_article == NULL ? 0 : p_article->aid), type);

	return 0;
}
