/***************************************************************************
				  test_article_favor.c  -  description
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

#include "article_favor.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
	ARTICLE_FAVOR view_log;
	int32_t aid;
	int i;

	article_favor_load(0, &view_log, 0);

	for (i = MAX_FAVOR_AID_INC_CNT * 3; i > 0; i--)
	{
		if (i % MAX_FAVOR_AID_INC_CNT == 0 || i % MAX_FAVOR_AID_INC_CNT == MAX_FAVOR_AID_INC_CNT / 2)
		{
			printf("Base cnt = %d, Inc cnt = %d\n", view_log.aid_base_cnt, view_log.aid_inc_cnt);
		}

		aid = i * 5 + 7;
		if (article_favor_check(aid, &view_log) != 0)
		{
			printf("article_favor_check(%d) != 0\n", aid);
			break;
		}
		if (article_favor_set(aid, &view_log) != 1)
		{
			printf("article_favor_set(%d) != 1\n", aid);
			break;
		}
	}

	printf("Base cnt = %d, Inc cnt = %d\n", view_log.aid_base_cnt, view_log.aid_inc_cnt);

	article_favor_unload(&view_log);
	
	return 0;
}
