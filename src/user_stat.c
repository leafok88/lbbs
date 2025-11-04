/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * user_stat
 *   - data model and basic operations of user related statistics
 *
 * Copyright (C) 2004-2025 by Leaflet <leaflet@leafok.com>
 */

#include "log.h"
#include "section_list.h"
#include "user_list.h"
#include "user_stat.h"

int user_stat_map_init(USER_STAT_MAP *p_map)
{
	if (p_map == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	p_map->user_count = 0;
	p_map->last_uid = -1;
	p_map->last_article_index = -1;

	return 0;
}

int user_stat_map_update(USER_STAT_MAP *p_map)
{
	int32_t uid_list[BBS_max_user_count];
	int user_cnt = BBS_max_user_count;
	int article_cnt;
	int i;
	ARTICLE *p_article;

	if (p_map == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	// Load uid_list
	if (get_user_id_list(uid_list, &user_cnt, p_map->last_uid + 1) < 0)
	{
		log_error("get_user_id_list(%d, 0) error\n", user_cnt);
		return -2;
	}

	for (i = 0; i < user_cnt && p_map->user_count + i < BBS_max_user_count; i++)
	{
		p_map->stat_list[p_map->user_count + i].uid = uid_list[i];
		p_map->stat_list[p_map->user_count + i].article_count = 0;
	}

	p_map->user_count += i;
	p_map->last_uid = uid_list[i - 1];

	// Scan articles
	article_cnt = article_block_article_count();
	for (i = p_map->last_article_index + 1; i < article_cnt; i++)
	{
		p_article = article_block_find_by_index(i);
		if (p_article == NULL)
		{
			log_error("article_block_find_by_index(index=%d) error\n", i);
			break;
		}

		if (p_article->visible == 0)
		{
			continue;
		}

		if (p_article->uid > p_map->last_uid)
		{
#ifdef _DEBUG
			log_error("uid=%d of article(aid=%d) is greater than last_uid=%d\n",
					  p_article->uid, p_article->aid, p_map->last_uid);
#endif
			continue;
		}

		if (user_stat_article_cnt_inc(p_map, p_article->uid, 1) < 0)
		{
			log_error("user_stat_article_cnt_inc(uid=%d, 1) error\n", p_article->uid);
			break;
		}
	}

	p_map->last_article_index = i - 1;

	return 0;
}

int user_stat_article_cnt_inc(USER_STAT_MAP *p_map, int32_t uid, int n)
{
	int left;
	int right;
	int mid;

	if (p_map == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	left = 0;
	right = p_map->user_count - 1;

	while (left < right)
	{
		mid = (left + right) / 2;
		if (uid < p_map->stat_list[mid].uid)
		{
			right = mid - 1;
		}
		else if (uid > p_map->stat_list[mid].uid)
		{
			left = mid + 1;
		}
		else // if (uid == p_map->stat_list[mid].uid)
		{
			left = mid;
			break;
		}
	}

	if (uid == p_map->stat_list[left].uid) // found
	{
		p_map->stat_list[left].article_count += n;
		return 1;
	}

	return 0; // not found
}

int user_stat_get(USER_STAT_MAP *p_map, int32_t uid, const USER_STAT **pp_stat)
{
	int left;
	int right;
	int mid;

	if (p_map == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	left = 0;
	right = p_map->user_count - 1;

	while (left < right)
	{
		mid = (left + right) / 2;
		if (uid < p_map->stat_list[mid].uid)
		{
			right = mid - 1;
		}
		else if (uid > p_map->stat_list[mid].uid)
		{
			left = mid + 1;
		}
		else // if (uid == p_map->stat_list[mid].uid)
		{
			left = mid;
			break;
		}
	}

	if (uid == p_map->stat_list[left].uid) // found
	{
		*pp_stat = &(p_map->stat_list[left]);
		return 1;
	}

	// not found
	*pp_stat = NULL;
	return 0;
}
