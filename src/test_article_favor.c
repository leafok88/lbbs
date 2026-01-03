/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * test_article_favor
 *   - tester for data model and basic operations of user favorite articles
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
		if (article_favor_check(aid, &favor) != 1)
		{
			printf("article_favor_check(%d) != 1\n", aid);
			break;
		}

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
