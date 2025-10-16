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
#include <sys/param.h>

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
	if ((rs = mysql_use_result(db)) == NULL)
	{
		log_error("Get article_favorite data failed\n");
		return -3;
	}

	p_favor->aid_base_cnt = 0;

	while ((row = mysql_fetch_row(rs)))
	{
		p_favor->aid_base[p_favor->aid_base_cnt] = atoi(row[0]);
		(p_favor->aid_base_cnt)++;
		if (p_favor->aid_base_cnt >= MAX_FAVOR_AID_BASE_CNT)
		{
			log_error("Too many article_favorite records for uid=%d\n",
					  uid);
			break;
		}
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

	p_favor->aid_base_cnt = 0;

	return 0;
}

int article_favor_save_inc(const ARTICLE_FAVOR *p_favor)
{
	MYSQL *db = NULL;
	char sql_add[SQL_BUFFER_LEN];
	char sql_del[SQL_BUFFER_LEN];
	char tuple_tmp[LINE_BUFFER_LEN];
	int i;
	int j;
	int cnt_pending_add = 0;
	int cnt_pending_del = 0;
	int cnt_total_add = 0;
	int cnt_total_del = 0;

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

	snprintf(sql_add, sizeof(sql_add),
			 "INSERT IGNORE INTO article_favorite(AID, UID) VALUES ");
	snprintf(sql_del, sizeof(sql_add),
			 "DELETE FROM article_favorite WHERE UID = %d AND AID IN (",
			 p_favor->uid);

	for (i = 0, j = 0; j < p_favor->aid_inc_cnt;)
	{
		if (i < p_favor->aid_base_cnt && p_favor->aid_base[i] == p_favor->aid_inc[j]) // XOR - delete record
		{
			snprintf(tuple_tmp, sizeof(tuple_tmp), "%d, ", p_favor->aid_inc[j]);
			strncat(sql_del, tuple_tmp, sizeof(sql_del) - 1 - strnlen(sql_del, sizeof(sql_del)));

			cnt_pending_del++;
			i++;
			j++;
		}
		else if (i < p_favor->aid_base_cnt && p_favor->aid_base[i] < p_favor->aid_inc[j]) // skip existing record
		{
			i++;
		}
		else // if (i >= p_favor->aid_base_cnt || p_favor->aid_base[i] > p_favor->aid_inc[j])
		{
			snprintf(tuple_tmp, sizeof(tuple_tmp),
					 "(%d, %d), ",
					 p_favor->aid_inc[j], p_favor->uid);
			strncat(sql_add, tuple_tmp, sizeof(sql_add) - 1 - strnlen(sql_add, sizeof(sql_add)));

			cnt_pending_add++;
			j++;
		}

		if ((cnt_pending_add >= 100 || (j + 1) >= p_favor->aid_inc_cnt) && cnt_pending_add > 0)
		{
			// Add
			sql_add[strnlen(sql_add, sizeof(sql_add)) - 2] = '\0'; // remove last ", "

			if (mysql_query(db, sql_add) != 0)
			{
				log_error("Add article_favorite error: %s\n", mysql_error(db));
				log_error("%s\n", sql_add);
				mysql_close(db);
				return -3;
			}

			cnt_total_add += (int)mysql_affected_rows(db);
			cnt_pending_add = 0;

			snprintf(sql_add, sizeof(sql_add),
					 "INSERT IGNORE INTO article_favorite(AID, UID) VALUES ");
		}

		if ((cnt_pending_del >= 100 || (j + 1) >= p_favor->aid_inc_cnt) && cnt_pending_del > 0)
		{
			// Delete
			sql_del[strnlen(sql_del, sizeof(sql_del)) - 2] = ')'; // replace last ", " with ") "

			if (mysql_query(db, sql_del) != 0)
			{
				log_error("Delete article_favorite error: %s\n", mysql_error(db));
				log_error("%s\n", sql_del);
				mysql_close(db);
				return -3;
			}

			cnt_total_del += (int)mysql_affected_rows(db);
			cnt_pending_del = 0;

			snprintf(sql_del, sizeof(sql_add),
					 "DELETE FROM article_favorite WHERE UID = %d AND AID IN (",
					 p_favor->uid);
		}
	}

	log_common("Saved %d and deleted %d article_favorite records for uid=%d\n",
			   cnt_total_add, cnt_total_del, p_favor->uid);

	mysql_close(db);

	return 0;
}

int article_favor_merge_inc(ARTICLE_FAVOR *p_favor)
{
	int32_t aid_new[MAX_FAVOR_AID_BASE_CNT];
	int i, j, k;
	int len;

	if (p_favor == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_favor->aid_inc_cnt == 0) // Nothing to be merged
	{
		return 0;
	}

	for (i = 0, j = 0, k = 0; i < p_favor->aid_base_cnt && j < p_favor->aid_inc_cnt && k < MAX_FAVOR_AID_BASE_CNT;)
	{
		if (p_favor->aid_base[i] == p_favor->aid_inc[j]) // XOR - discard duplicate pair
		{
			i++;
			j++;
		}
		else if (p_favor->aid_base[i] < p_favor->aid_inc[j])
		{
			aid_new[k++] = p_favor->aid_base[i++];
		}
		else // if (p_favor->aid_base[i] > p_favor->aid_inc[j])
		{
			aid_new[k++] = p_favor->aid_inc[j++];
		}
	}

	len = MIN(p_favor->aid_base_cnt - i, MAX_FAVOR_AID_BASE_CNT - k);
	if (len > 0)
	{
		memcpy(aid_new + k, p_favor->aid_base + i,
			   sizeof(int32_t) * (size_t)len);
		i += len;
		k += len;
	}
	if (i < p_favor->aid_base_cnt)
	{
		log_error("Too many base aids, %d will be discarded\n", p_favor->aid_base_cnt - i);
	}

	len = MIN(p_favor->aid_inc_cnt - j, MAX_FAVOR_AID_BASE_CNT - k);
	if (len > 0)
	{
		memcpy(aid_new + k, p_favor->aid_inc + j,
			   sizeof(int32_t) * (size_t)len);
		j += len;
		k += len;
	}
	if (j < p_favor->aid_inc_cnt)
	{
		log_error("Too many inc aids, %d will be discarded\n", p_favor->aid_inc_cnt - j);
	}

	memcpy(p_favor->aid_base, aid_new, sizeof(int32_t) * (size_t)k);

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
	int is_set = 0;

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
				left = mid;
				break;
			}
		}

		if (aid == (i == 0 ? p_favor->aid_base[left] : p_favor->aid_inc[left]))
		{
			is_set = (is_set ? 0 : 1);
		}
	}

	return is_set;
}

int article_favor_set(int32_t aid, ARTICLE_FAVOR *p_favor, int state)
{
	int left;
	int right;
	int mid;
	int i;
	int is_set = 0;

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
				left = mid;
				break;
			}
		}

		if (aid == (i == 0 ? p_favor->aid_base[left] : p_favor->aid_inc[left]))
		{
			is_set = (is_set ? 0 : 1);
		}
	}

	if ((is_set ^ (state ? 1 : 0)) == 0) // No change
	{
		return 0;
	}

	if (aid == p_favor->aid_inc[left] && p_favor->aid_inc_cnt > 0) // Unset
	{
		if (p_favor->aid_inc_cnt > left + 1)
		{
			memmove(p_favor->aid_inc + left,
					p_favor->aid_inc + left + 1,
					sizeof(int32_t) * (size_t)(p_favor->aid_inc_cnt - left - 1));
		}

		(p_favor->aid_inc_cnt)--;

		return 2; // Unset complete
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

	if (p_favor->aid_inc_cnt > right)
	{
		memmove(p_favor->aid_inc + right + 1,
				p_favor->aid_inc + right,
				sizeof(int32_t) * (size_t)(p_favor->aid_inc_cnt - right));
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
