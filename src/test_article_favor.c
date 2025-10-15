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
	ARTICLE_FAVOR favor;
	int32_t aid;
	int i;
	int ret = 0;

	article_favor_load(0, &favor, 0);

	printf("Test #1\n");

	for (i = MAX_FAVOR_AID_INC_CNT * 3; i > 0; i--)
	{
		if (i % MAX_FAVOR_AID_INC_CNT == 0 || i % MAX_FAVOR_AID_INC_CNT == MAX_FAVOR_AID_INC_CNT / 2)
		{
			printf("Base cnt = %d, Inc cnt = %d\n", favor.aid_base_cnt, favor.aid_inc_cnt);
		}

		aid = i * 5 + 7;
		if (article_favor_check(aid, &favor) != 0)
		{
			printf("article_favor_check(%d) != 0\n", aid);
			break;
		}
		if (article_favor_set(aid, &favor, 1) != 1)
		{
			printf("article_favor_set(%d, 1) != 1\n", aid);
			break;
		}
		if (article_favor_check(aid, &favor) != 1)
		{
			printf("article_favor_check(%d) != 1\n", aid);
			break;
		}
		if ((ret = article_favor_set(aid, &favor, 0)) != 2)
		{
			printf("article_favor_set(%d, 0) != 2, ret = %d\n", aid, ret);
			break;
		}

		if (article_favor_check(aid, &favor) != 0)
		{
			printf("article_favor_check(%d) != 0\n", aid);
			break;
		}
	}

	printf("Base cnt = %d, Inc cnt = %d\n", favor.aid_base_cnt, favor.aid_inc_cnt);

	printf("Test #2\n");

	article_favor_merge_inc(&favor);

	printf("Base cnt = %d, Inc cnt = %d\n", favor.aid_base_cnt, favor.aid_inc_cnt);

	for (i = MAX_FAVOR_AID_INC_CNT * 3; i > 0; i--)
	{
		if (i % MAX_FAVOR_AID_INC_CNT == 0 || i % MAX_FAVOR_AID_INC_CNT == MAX_FAVOR_AID_INC_CNT / 2)
		{
			printf("Base cnt = %d, Inc cnt = %d\n", favor.aid_base_cnt, favor.aid_inc_cnt);
		}

		aid = i * 5 + 7;
		if (article_favor_check(aid, &favor) != 0)
		{
			printf("article_favor_check(%d) != 0\n", aid);
			break;
		}

		if (article_favor_set(aid, &favor, 1) != 1)
		{
			printf("article_favor_set(%d, 1) != 1\n", aid);
			break;
		}
		if (article_favor_check(aid, &favor) != 1)
		{
			printf("article_favor_check(%d) != 1\n", aid);
			break;
		}
	}

	printf("Base cnt = %d, Inc cnt = %d\n", favor.aid_base_cnt, favor.aid_inc_cnt);

	printf("Test #3\n");

	article_favor_merge_inc(&favor);

	printf("Base cnt = %d, Inc cnt = %d\n", favor.aid_base_cnt, favor.aid_inc_cnt);

	for (i = MAX_FAVOR_AID_INC_CNT * 3; i > 0; i--)
	{
		if (i % MAX_FAVOR_AID_INC_CNT == 0 || i % MAX_FAVOR_AID_INC_CNT == MAX_FAVOR_AID_INC_CNT / 2)
		{
			printf("Base cnt = %d, Inc cnt = %d\n", favor.aid_base_cnt, favor.aid_inc_cnt);
		}

		aid = i * 5 + 7;
		if (article_favor_set(aid, &favor, 1) != 0)
		{
			printf("article_favor_set(%d, 1) != 0\n", aid);
			break;
		}

		if (article_favor_check(aid, &favor) != 1)
		{
			printf("article_favor_check(%d) != 1\n", aid);
			break;
		}

		if (article_favor_set(aid, &favor, 0) != 1)
		{
			printf("article_favor_set(%d, 0) != 1\n", aid);
			break;
		}

		if (article_favor_check(aid, &favor) != 0)
		{
			printf("article_favor_check(%d) != 0\n", aid);
			break;
		}
	}

	printf("Base cnt = %d, Inc cnt = %d\n", favor.aid_base_cnt, favor.aid_inc_cnt);

	printf("Test #4\n");

	article_favor_merge_inc(&favor);

	printf("Base cnt = %d, Inc cnt = %d\n", favor.aid_base_cnt, favor.aid_inc_cnt);

	article_favor_unload(&favor);

	return 0;
}
