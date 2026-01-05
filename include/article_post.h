/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * article_post
 *   - user interactive feature to post / modify / reply article
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifndef _ARTICLE_POST_H_
#define _ARTICLE_POST_H_

#include "section_list.h"

extern int article_post(const SECTION_LIST *p_section, ARTICLE *p_article_new);
extern int article_modify(const SECTION_LIST *p_section, const ARTICLE *p_article, ARTICLE *p_article_new);
extern int article_reply(const SECTION_LIST *p_section, const ARTICLE *p_article, ARTICLE *p_article_new);

#endif //_ARTICLE_POST_H_
