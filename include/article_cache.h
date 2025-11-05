/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * article_cache
 *   - convert article content from DB to local cache with LML conversion and line offset index
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _ARTICLE_CACHE_H_
#define _ARTICLE_CACHE_H_

#include "common.h"
#include "section_list.h"
#include "str_process.h"

enum article_cache_constant_t
{
	ARTICLE_CONTENT_MAX_LEN = 1024 * 1024 * 4, // 4MB
};

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
extern void article_cache_cleanup(void);

extern int article_cache_load(ARTICLE_CACHE *p_cache, const char *cache_dir, const ARTICLE *p_article);
extern int article_cache_unload(ARTICLE_CACHE *p_cache);

#endif //_ARTICLE_CACHE_H_
