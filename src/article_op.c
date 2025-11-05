/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * article_op
 *   - basic operations of articles
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#include "article_op.h"
#include "bbs.h"
#include "io.h"
#include "screen.h"

int display_article_meta(int32_t aid)
{
	clearscr();
	moveto(3, 1);
	prints("Web版 文章链接：");
	moveto(4, 1);
	prints("http://%s/bbs/view_article.php?id=%d#%d", BBS_server, aid, aid);
	press_any_key();

	return 0;
}
