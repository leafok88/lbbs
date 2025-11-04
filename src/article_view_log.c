/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * article_view_log
 *   - data persistence and query of article view log
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#include "article_view_log.h"
#include "common.h"
#include "database.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

ARTICLE_VIEW_LOG BBS_article_view_log;

int article_view_log_load(int uid, ARTICLE_VIEW_LOG *p_view_log, int keep_inc)
{
	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];

	if (p_view_log == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	p_view_log->uid = uid;

	if (uid == 0)
	{
		p_view_log->aid_base_cnt = 0;
		p_view_log->aid_base = NULL;

		if (!keep_inc)
		{
			p_view_log->aid_inc_cnt = 0;
		}

		return 0;
	}

	if ((db = db_open()) == NULL)
	{
		log_error("article_view_log_load() error: Unable to open DB\n");
		return -2;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT AID FROM view_article_log WHERE UID = %d "
			 "ORDER BY AID",
			 uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query view_article_log error: %s\n", mysql_error(db));
		return -3;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get view_article_log data failed\n");
		return -3;
	}

	p_view_log->aid_base_cnt = 0;
	p_view_log->aid_base = malloc(sizeof(int32_t) * mysql_num_rows(rs));
	if (p_view_log->aid_base == NULL)
	{
		log_error("malloc(INT32 * %d) error: OOM\n", mysql_num_rows(rs));
		mysql_free_result(rs);
		mysql_close(db);
		return -4;
	}

	while ((row = mysql_fetch_row(rs)))
	{
		p_view_log->aid_base[(p_view_log->aid_base_cnt)++] = atoi(row[0]);
	}
	mysql_free_result(rs);

	mysql_close(db);

	log_common("Loaded %d view_article_log records for uid=%d\n", p_view_log->aid_base_cnt, uid);

	if (!keep_inc)
	{
		p_view_log->aid_inc_cnt = 0;
	}

	return 0;
}

int article_view_log_unload(ARTICLE_VIEW_LOG *p_view_log)
{
	if (p_view_log == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_view_log->aid_base != NULL)
	{
		free(p_view_log->aid_base);
		p_view_log->aid_base = NULL;
		p_view_log->aid_base_cnt = 0;
	}

	return 0;
}

int article_view_log_save_inc(const ARTICLE_VIEW_LOG *p_view_log)
{
	MYSQL *db = NULL;
	char sql[SQL_BUFFER_LEN];
	char tuple_tmp[LINE_BUFFER_LEN];
	int i;
	int affected_record = 0;

	if (p_view_log == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_view_log->uid <= 0 || p_view_log->aid_inc_cnt == 0)
	{
		return 0;
	}

	if ((db = db_open()) == NULL)
	{
		log_error("article_view_log_load() error: Unable to open DB\n");
		return -2;
	}

	snprintf(sql, sizeof(sql),
			 "INSERT IGNORE INTO view_article_log(AID, UID, dt) VALUES ");

	for (i = 0; i < p_view_log->aid_inc_cnt; i++)
	{
		snprintf(tuple_tmp, sizeof(tuple_tmp),
				 "(%d, %d, NOW())",
				 p_view_log->aid_inc[i], p_view_log->uid);
		strncat(sql, tuple_tmp, sizeof(sql) - 1 - strnlen(sql, sizeof(sql)));

		if ((i + 1) % 100 == 0 || (i + 1) == p_view_log->aid_inc_cnt) // Insert 100 records per query
		{
			if (mysql_query(db, sql) != 0)
			{
				log_error("Add view_article_log error: %s\n", mysql_error(db));
				mysql_close(db);
				return -3;
			}

			affected_record += (int)mysql_affected_rows(db);

			snprintf(sql, sizeof(sql),
					 "INSERT IGNORE INTO view_article_log(AID, UID, dt) VALUES ");
		}
		else
		{
			strncat(sql, ", ", sizeof(sql) - 1 - strnlen(sql, sizeof(sql)));
		}
	}

	log_common("Saved %d view_article_log records for uid=%d\n", affected_record, p_view_log->uid);

	mysql_close(db);

	return 0;
}

int article_view_log_merge_inc(ARTICLE_VIEW_LOG *p_view_log)
{
	int32_t *aid_new;
	int aid_new_cnt;
	int i, j, k;
	int len;

	if (p_view_log == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_view_log->aid_inc_cnt == 0) // Nothing to be merged
	{
		return 0;
	}

	aid_new_cnt = p_view_log->aid_base_cnt + p_view_log->aid_inc_cnt;

	aid_new = malloc(sizeof(int32_t) * (size_t)aid_new_cnt);
	if (aid_new == NULL)
	{
		log_error("malloc(INT32 * %d) error: OOM\n", aid_new_cnt);
		return -2;
	}

	for (i = 0, j = 0, k = 0; i < p_view_log->aid_base_cnt && j < p_view_log->aid_inc_cnt;)
	{
		if (p_view_log->aid_base[i] <= p_view_log->aid_inc[j])
		{
			if (p_view_log->aid_base[i] == p_view_log->aid_inc[j])
			{
				log_error("Duplicate aid = %d found in both Base (offset = %d) and Inc (offset = %d)\n",
						  p_view_log->aid_base[i], i, j);
				j++; // Skip duplicate one in Inc
			}

			aid_new[k++] = p_view_log->aid_base[i++];
		}
		else if (p_view_log->aid_base[i] > p_view_log->aid_inc[j])
		{
			aid_new[k++] = p_view_log->aid_inc[j++];
		}
	}

	len = p_view_log->aid_base_cnt - i;
	if (len > 0)
	{
		memcpy(aid_new + k, p_view_log->aid_base + i,
			   sizeof(int32_t) * (size_t)len);
		k += len;
	}
	len = p_view_log->aid_inc_cnt - j;
	if (len > 0)
	{
		memcpy(aid_new + k, p_view_log->aid_inc + j,
			   sizeof(int32_t) * (size_t)len);
		k += len;
	}

	free(p_view_log->aid_base);
	p_view_log->aid_base = aid_new;
	p_view_log->aid_base_cnt = k;

	p_view_log->aid_inc_cnt = 0;

	return 0;
}

int article_view_log_is_viewed(int32_t aid, const ARTICLE_VIEW_LOG *p_view_log)
{
	int left;
	int right;
	int mid;
	int i;

	if (p_view_log == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	for (i = 0; i < 2; i++)
	{
		left = 0;
		right = (i == 0 ? p_view_log->aid_base_cnt : p_view_log->aid_inc_cnt) - 1;

		if (right < 0)
		{
			continue;
		}

		while (left < right)
		{
			mid = (left + right) / 2;
			if (aid < (i == 0 ? p_view_log->aid_base[mid] : p_view_log->aid_inc[mid]))
			{
				right = mid - 1;
			}
			else if (aid > (i == 0 ? p_view_log->aid_base[mid] : p_view_log->aid_inc[mid]))
			{
				left = mid + 1;
			}
			else // if (aid == p_view_log->aid_base[mid])
			{
				return 1;
			}
		}

		if (aid == (i == 0 ? p_view_log->aid_base[left] : p_view_log->aid_inc[left])) // Found
		{
			return 1;
		}
	}

	return 0;
}

int article_view_log_set_viewed(int32_t aid, ARTICLE_VIEW_LOG *p_view_log)
{
	int left;
	int right;
	int mid;
	int i;

	if (p_view_log == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	for (i = 0; i < 2; i++)
	{
		left = 0;
		right = (i == 0 ? p_view_log->aid_base_cnt : p_view_log->aid_inc_cnt) - 1;

		if (right < 0)
		{
			continue;
		}

		while (left < right)
		{
			mid = (left + right) / 2;
			if (aid < (i == 0 ? p_view_log->aid_base[mid] : p_view_log->aid_inc[mid]))
			{
				right = mid - 1;
			}
			else if (aid > (i == 0 ? p_view_log->aid_base[mid] : p_view_log->aid_inc[mid]))
			{
				left = mid + 1;
			}
			else // if (aid == p_view_log->aid_base[mid])
			{
				return 0; // Already set
			}
		}

		if (aid == (i == 0 ? p_view_log->aid_base[left] : p_view_log->aid_inc[left])) // Found
		{
			return 0; // Already set
		}
	}

	// Merge if Inc is full
	if (p_view_log->aid_inc_cnt >= MAX_VIEWED_AID_INC_CNT)
	{
		// Save incremental article view log
		if (article_view_log_save_inc(p_view_log) < 0)
		{
			log_error("article_view_log_save_inc() error\n");
			return -2;
		}

		article_view_log_merge_inc(p_view_log);

		p_view_log->aid_inc[(p_view_log->aid_inc_cnt)++] = aid;

		return 1; // Set complete
	}

	if (right < 0)
	{
		right = 0;
	}
	else if (aid > p_view_log->aid_inc[left])
	{
		right = left + 1;
	}

	if (p_view_log->aid_inc_cnt > right)
	{
		memmove(p_view_log->aid_inc + right + 1,
				p_view_log->aid_inc + right,
				sizeof(int32_t) * (size_t)(p_view_log->aid_inc_cnt - right));
	}

	p_view_log->aid_inc[right] = aid;
	(p_view_log->aid_inc_cnt)++;

	return 1; // Set complete
}
