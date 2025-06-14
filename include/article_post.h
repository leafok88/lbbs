/***************************************************************************
						article_post.h  -  description
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

#ifndef _ARTICLE_POST_H_
#define _ARTICLE_POST_H_

#include "section_list.h"

extern int article_post(const SECTION_LIST *p_section, ARTICLE *p_article_new);
extern int article_modify(const SECTION_LIST *p_section, const ARTICLE *p_article, ARTICLE *p_article_new);
extern int article_reply(const SECTION_LIST *p_section, const ARTICLE *p_article, ARTICLE *p_article_new);

#endif //_ARTICLE_POST_H_
