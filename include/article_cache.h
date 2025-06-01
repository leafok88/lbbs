/***************************************************************************
					   article_cache.h  -  description
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

#ifndef _ARTICLE_CACHE_H_
#define _ARTICLE_CACHE_H_

#include "common.h"
#include "str_process.h"
#include "section_list.h"

struct article_cache_t
{
	void *p_mmap;
	size_t mmap_len;
	char *p_data;
	size_t data_len;
	long line_total;
	long line_offsets[MAX_SPLIT_FILE_LINES];
};
typedef struct article_cache_t ARTICLE_CACHE;

extern int article_cache_generate(const char *cache_dir, const ARTICLE *p_article, const SECTION_LIST *p_section,
								  const char *content, const char *sub_ip, int overwrite);

extern int article_cache_load(ARTICLE_CACHE *p_cache, const char *cache_dir, const ARTICLE *p_article);
extern int article_cache_unload(ARTICLE_CACHE *p_cache);

#endif //_ARTICLE_CACHE_H_
