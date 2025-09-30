/***************************************************************************
					   article_op.c  -  description
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
