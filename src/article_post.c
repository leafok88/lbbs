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
	ARTICLE_CACHE cache;
	EDITOR_DATA *p_editor_data;

	log_common("Debug: sid=%d aid=%d type=%d\n",
			   p_section->sid, (p_article == NULL ? 0 : p_article->aid), type);

	switch (type)
	{
	case ARTICLE_POST_NEW:
		p_editor_data = editor_data_load("");
		if (p_editor_data == NULL)
		{
			log_error("editor_data_load() error\n");
			return -2;
		}
		break;
	case ARTICLE_POST_EDIT:
	case ARTICLE_POST_REPLY:
		if (article_cache_load(&cache, VAR_ARTICLE_CACHE_DIR, p_article) < 0)
		{
			log_error("article_cache_load(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
			return -2;
		}
		p_editor_data = editor_data_load(cache.p_data);
		if (p_editor_data == NULL)
		{
			log_error("editor_data_load(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
			return -2;
		}
		if (article_cache_unload(&cache) < 0)
		{
			log_error("article_cache_unload(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
			return -2;
		}
		break;
	default:
		log_error("Unsupported type=%d\n", type);
		return -1;
	}

	editor_display(p_editor_data);

	// editor_data_save(p_editor_data, p_data_new, data_new_len);

	editor_data_cleanup(p_editor_data);

	return 0;
}
