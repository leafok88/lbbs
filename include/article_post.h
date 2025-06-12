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

enum article_post_type_t
{
	ARTICLE_POST_NEW = 0,
	ARTICLE_POST_EDIT = 1,
	ARTICLE_POST_REPLY = 2,
};
typedef enum article_post_type_t ARTICLE_POST_TYPE;

extern int article_post(SECTION_LIST *p_section, ARTICLE *p_article, ARTICLE_POST_TYPE type);

#endif //_ARTICLE_POST_H_
