/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * article_op
 *   - basic operations of articles
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _ARTICLE_OP_H_
#define _ARTICLE_OP_H_

#include <stdint.h>

enum bbs_article_op_type_t
{
    BBS_article_set_excerption = 'E',
    BBS_article_unset_excerption = 'O',
};
extern int display_article_meta(int32_t aid);
extern int article_exc_set(int32_t aid, int8_t is_exc);

#endif //_ARTICLE_OP_H_
