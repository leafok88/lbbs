/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * article_op
 *   - basic operations of articles
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifndef _ARTICLE_OP_H_
#define _ARTICLE_OP_H_

#include "section_list.h"
#include <stdint.h>

extern int display_article_meta(int32_t aid);
extern int article_excerption_set(SECTION_LIST *p_section, int32_t aid, int8_t set);

#endif //_ARTICLE_OP_H_
