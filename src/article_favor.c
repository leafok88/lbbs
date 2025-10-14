/***************************************************************************
					 article_favor.c  -  description
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
#include "common.h"
#include "database.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

ARTICLE_FAVOR BBS_article_favor;

int article_favor_load(int uid, ARTICLE_FAVOR *p_favor, int keep_inc)
{
	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];

	if (p_favor == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	p_favor->uid = uid;

	if (uid == 0)
	{
		p_favor->aid_base_cnt = 0;
		p_favor->aid_base = NULL;

		if (!keep_inc)
		{
			p_favor->aid_inc_cnt = 0;
		}

		return 0;
	}

	if ((db = db_open()) == NULL)
	{
		log_error("article_favor_load() error: Unable to open DB\n");
		return -2;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT AID FROM article_favorite WHERE UID = %d "
			 "ORDER BY AID",
			 uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article_favorite error: %s\n", mysql_error(db));
		return -3;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get article_favorite data failed\n");
		return -3;
	}

	p_favor->aid_base_cnt = 0;
	p_favor->aid_base = malloc(sizeof(int32_t) * mysql_num_rows(rs));
	if (p_favor->aid_base == NULL)
	{
		log_error("malloc(INT32 * %d) error: OOM\n", mysql_num_rows(rs));
		mysql_free_result(rs);
		mysql_close(db);
		return -4;
	}

	while ((row = mysql_fetch_row(rs)))
	{
		p_favor->aid_base[(p_favor->aid_base_cnt)++] = atoi(row[0]);
	}
	mysql_free_result(rs);

	mysql_close(db);

	log_common("Loaded %d article_favorite records for uid=%d\n", p_favor->aid_base_cnt, uid);

	if (!keep_inc)
	{
		p_favor->aid_inc_cnt = 0;
	}

	return 0;
}

int article_favor_unload(ARTICLE_FAVOR *p_favor)
{
	if (p_favor == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_favor->aid_base != NULL)
	{
		free(p_favor->aid_base);
		p_favor->aid_base = NULL;
		p_favor->aid_base_cnt = 0;
	}

	return 0;
}

int article_favor_save_inc(const ARTICLE_FAVOR *p_favor)
{
	MYSQL *db = NULL;
	char sql[SQL_BUFFER_LEN];
	char tuple_tmp[LINE_BUFFER_LEN];
	int i;
	int affected_record = 0;

	if (p_favor == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_favor->uid <= 0 || p_favor->aid_inc_cnt == 0)
	{
		return 0;
	}

	if ((db = db_open()) == NULL)
	{
		log_error("article_favor_load() error: Unable to open DB\n");
		return -2;
	}

	snprintf(sql, sizeof(sql),
			 "INSERT IGNORE INTO article_favorite(AID, UID) VALUES ");

	for (i = 0; i < p_favor->aid_inc_cnt; i++)
	{
		snprintf(tuple_tmp, sizeof(tuple_tmp),
				 "(%d, %d)",
				 p_favor->aid_inc[i], p_favor->uid);
		strncat(sql, tuple_tmp, sizeof(sql) - 1 - strnlen(sql, sizeof(sql)));

		if ((i + 1) % 100 == 0 || (i + 1) == p_favor->aid_inc_cnt) // Insert 100 records per query
		{
			if (mysql_query(db, sql) != 0)
			{
				log_error("Add article_favorite error: %s\n", mysql_error(db));
				mysql_close(db);
				return -3;
			}

			affected_record += (int)mysql_affected_rows(db);

			snprintf(sql, sizeof(sql),
					 "INSERT IGNORE INTO article_favorite(AID, UID) VALUES ");
		}
		else
		{
			strncat(sql, ", ", sizeof(sql) - 1 - strnlen(sql, sizeof(sql)));
		}
	}

	log_common("Saved %d article_favorite records for uid=%d\n", affected_record, p_favor->uid);

	mysql_close(db);

	return 0;
}

int article_favor_merge_inc(ARTICLE_FAVOR *p_favor)
{
	int32_t *aid_new;
	int aid_new_cnt;
	int i, j, k;

	if (p_favor == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_favor->aid_inc_cnt == 0) // Nothing to be merged
	{
		return 0;
	}

	aid_new_cnt = p_favor->aid_base_cnt + p_favor->aid_inc_cnt;

	aid_new = malloc(sizeof(int32_t) * (size_t)aid_new_cnt);
	if (aid_new == NULL)
	{
		log_error("malloc(INT32 * %d) error: OOM\n", aid_new_cnt);
		return -2;
	}

	for (i = 0, j = 0, k = 0; i < p_favor->aid_base_cnt && j < p_favor->aid_inc_cnt;)
	{
		if (p_favor->aid_base[i] <= p_favor->aid_inc[j])
		{
			if (p_favor->aid_base[i] == p_favor->aid_inc[j])
			{
				log_error("Duplicate aid = %d found in both Base (offset = %d) and Inc (offset = %d)\n",
						  p_favor->aid_base[i], i, j);
				j++; // Skip duplicate one in Inc
			}

			aid_new[k++] = p_favor->aid_base[i++];
		}
		else if (p_favor->aid_base[i] > p_favor->aid_inc[j])
		{
			aid_new[k++] = p_favor->aid_inc[j++];
		}
	}

	memcpy(aid_new + k, p_favor->aid_base + i, sizeof(int32_t) * (size_t)(p_favor->aid_base_cnt - i));
	k += (p_favor->aid_base_cnt - i);
	memcpy(aid_new + k, p_favor->aid_inc + j, sizeof(int32_t) * (size_t)(p_favor->aid_inc_cnt - j));
	k += (p_favor->aid_inc_cnt - j);

	free(p_favor->aid_base);
	p_favor->aid_base = aid_new;
	p_favor->aid_base_cnt = k;

	p_favor->aid_inc_cnt = 0;

	return 0;
}

int article_favor_check(int32_t aid, const ARTICLE_FAVOR *p_favor)
{
	int left;
	int right;
	int mid;
	int i;

	if (p_favor == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	for (i = 0; i < 2; i++)
	{
		left = 0;
		right = (i == 0 ? p_favor->aid_base_cnt : p_favor->aid_inc_cnt) - 1;

		if (right < 0)
		{
			continue;
		}

		while (left < right)
		{
			mid = (left + right) / 2;
			if (aid < (i == 0 ? p_favor->aid_base[mid] : p_favor->aid_inc[mid]))
			{
				right = mid;
			}
			else if (aid > (i == 0 ? p_favor->aid_base[mid] : p_favor->aid_inc[mid]))
			{
				left = mid + 1;
			}
			else // if (aid == p_favor->aid_base[mid])
			{
				return 1;
			}
		}

		if (aid == (i == 0 ? p_favor->aid_base[left] : p_favor->aid_inc[left])) // Found
		{
			return 1;
		}
	}

	return 0;
}

int article_favor_set(int32_t aid, ARTICLE_FAVOR *p_favor)
{
	int left;
	int right;
	int mid;
	int i;

	if (p_favor == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	for (i = 0; i < 2; i++)
	{
		left = 0;
		right = (i == 0 ? p_favor->aid_base_cnt : p_favor->aid_inc_cnt) - 1;

		if (right < 0)
		{
			continue;
		}

		while (left < right)
		{
			mid = (left + right) / 2;
			if (aid < (i == 0 ? p_favor->aid_base[mid] : p_favor->aid_inc[mid]))
			{
				right = mid;
			}
			else if (aid > (i == 0 ? p_favor->aid_base[mid] : p_favor->aid_inc[mid]))
			{
				left = mid + 1;
			}
			else // if (aid == p_favor->aid_base[mid])
			{
				return 0; // Already set
			}
		}

		if (aid == (i == 0 ? p_favor->aid_base[left] : p_favor->aid_inc[left])) // Found
		{
			return 0; // Already set
		}
	}

	// Merge if Inc is full
	if (p_favor->aid_inc_cnt >= MAX_FAVOR_AID_INC_CNT)
	{
		// Save incremental article favorite
		if (article_favor_save_inc(p_favor) < 0)
		{
			log_error("article_favor_save_inc() error\n");
			return -2;
		}

		article_favor_merge_inc(p_favor);

		p_favor->aid_inc[(p_favor->aid_inc_cnt)++] = aid;

		return 1; // Set complete
	}

	if (right < 0)
	{
		right = 0;
	}
	else if (aid > p_favor->aid_inc[left])
	{
		right = left + 1;
	}

	for (i = p_favor->aid_inc_cnt - 1; i >= right; i--)
	{
		p_favor->aid_inc[i + 1] = p_favor->aid_inc[i];
	}

	p_favor->aid_inc[right] = aid;
	(p_favor->aid_inc_cnt)++;

	return 1; // Set complete
}

int query_favor_articles(ARTICLE_FAVOR *p_favor, int page_id, ARTICLE **p_articles,
						 char p_snames[][BBS_section_name_max_len + 1], int *p_article_count, int *p_page_count)
{
	SECTION_LIST *p_section;
	int32_t aid;
	int i;

	if (p_favor == NULL || p_articles == NULL || p_article_count == NULL || p_page_count == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (article_favor_save_inc(p_favor) < 0)
	{
		log_error("article_favor_save_inc() error\n");
		return -2;
	}
	if (article_favor_merge_inc(p_favor) < 0)
	{
		log_error("article_favor_merge_inc() error\n");
		return -2;
	}

	*p_page_count = p_favor->aid_base_cnt / BBS_article_limit_per_page + (p_favor->aid_base_cnt % BBS_article_limit_per_page == 0 ? 0 : 1);
	*p_article_count = 0;

	if (p_favor->aid_base_cnt == 0)
	{
		// empty list
		return 0;
	}

	if (page_id < 0 || page_id >= *p_page_count)
	{
		log_error("Invalid page_id = %d, not in range [0, %d)\n", page_id, *p_page_count);
		return -1;
	}

	for (i = 0;
		 i < BBS_article_limit_per_page &&
		 page_id * BBS_article_limit_per_page + i < p_favor->aid_base_cnt;
		 i++)
	{
		aid = p_favor->aid_base[page_id * BBS_article_limit_per_page + i];
		p_articles[i] = article_block_find_by_aid(aid);
		if (p_articles[i] == NULL)
		{
			log_error("article_block_find_by_aid(aid=%d) error: page_id=%d, i=%d\n", aid, page_id, i);
			return -3;
		}

		p_section = section_list_find_by_sid(p_articles[i]->sid);
		if (p_section == NULL)
		{
			log_error("section_list_find_by_sid(%d) error\n", p_articles[i]->sid);
			return -3;
		}

		// acquire lock of section
		if (section_list_rd_lock(p_section) < 0)
		{
			log_error("section_list_rd_lock(sid = %d) error\n", p_section->sid);
			return -4;
		}

		memcpy(p_snames[i], p_section->sname, sizeof(p_snames[i]));

		// release lock of section
		if (section_list_rd_unlock(p_section) < 0)
		{
			log_error("section_list_rd_unlock(sid = %d) error\n", p_section->sid);
			return -4;
		}
	}
	*p_article_count = i;

	return 0;
}
