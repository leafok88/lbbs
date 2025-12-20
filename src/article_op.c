/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * article_op
 *   - basic operations of articles
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "article_op.h"
#include "bbs.h"
#include "database.h"
#include "io.h"
#include "log.h"
#include "screen.h"
#include "user_priv.h"

enum ret_msg_len_t
{
	ret_msg_len = 20,
};

int display_article_meta(int32_t aid)
{
	clearscr();
	moveto(3, 1);
	prints("Web版 文章链接: ");
	moveto(4, 1);
	prints("https://%s/bbs/view_article.php?id=%d#%d", BBS_server, aid, aid);
	press_any_key();

	return 0;
}

int article_excerption_set(SECTION_LIST *p_section, int32_t aid, int8_t set)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int ret = 0;
	int uid = 0;
	int tid = 0;
	int sid = 0;
	int8_t transship = 0;
	int8_t excerption = 0;

	if (p_section == NULL)
	{
		log_error("NULL pointer error");
		ret = -1;
		goto cleanup;
	}

	if (set)
	{
		set = 1;
	}

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Begin transaction
	if (mysql_query(db, "SET autocommit=0") != 0)
	{
		log_error("SET autocommit=0 error: %s", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	if (mysql_query(db, "BEGIN") != 0)
	{
		log_error("Begin transaction error: %s", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT UID, TID, SID, transship, excerption FROM bbs "
			 "WHERE AID = %d AND visible FOR UPDATE",
			 aid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article error: %s", mysql_error(db));
		ret = -2;
		goto cleanup;
	}

	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get article data failed");
		ret = -2;
		goto cleanup;
	}

	if ((row = mysql_fetch_row(rs)))
	{
		uid = atoi(row[0]);
		tid = atoi(row[1]);
		sid = atoi(row[2]);
		transship = (int8_t)atoi(row[3]);
		excerption = (int8_t)atoi(row[4]);
	}
	else
	{
		log_error("Article not found: aid=%d", aid);
		ret = -2;
		goto cleanup;
	}
	mysql_free_result(rs);
	rs = NULL;

	if (p_section->sid != sid)
	{
		log_error("Inconsistent SID of article (aid=%d): %d!=%d", aid, p_section->sid, sid);
		ret = -2;
		goto cleanup;
	}

	// Check if already set
	if (excerption != set)
	{
		snprintf(sql, sizeof(sql),
				 "UPDATE bbs SET excerption = %d WHERE AID = %d",
				 set, aid);
		if (mysql_query(db, sql) != 0)
		{
			log_error("Set excerption error: %s", mysql_error(db));
			ret = -2;
			goto cleanup;
		}

		// Clear gen_ex if unset excerption
		if (set == 0)
		{
			snprintf(sql, sizeof(sql),
					 "UPDATE bbs SET gen_ex = 0, static = 0 WHERE AID = %d OR TID = %d",
					 aid, aid);

			if (mysql_query(db, sql) != 0)
			{
				log_error("Set gen_ex error: %s", mysql_error(db));
				ret = -2;
				goto cleanup;
			}

			// Delete ex_dir path if head of thread
			if (tid == 0)
			{
				snprintf(sql, sizeof(sql),
						 "DELETE FROM ex_file WHERE AID = %d",
						 aid);

				if (mysql_query(db, sql) != 0)
				{
					log_error("Delete ex_file error: %s", mysql_error(db));
					ret = -2;
					goto cleanup;
				}
			}
		}

		// Change UID of attachments
		snprintf(sql, sizeof(sql),
				 "UPDATE upload_file SET UID = %d "
				 "WHERE ref_AID = %d AND deleted = 0",
				 (set ? 0 : uid), aid);
		if (mysql_query(db, sql) != 0)
		{
			log_error("Set attachment status error: %s", mysql_error(db));
			ret = -2;
			goto cleanup;
		}

		// Modify exp
		if (checkpriv(&BBS_priv, sid, S_GETEXP)) // Except in test section
		{
			snprintf(sql, sizeof(sql),
					 "UPDATE user_pubinfo SET exp = exp + %d WHERE UID = %d",
					 (set ? 1 : -1) * (tid == 0 ? (transship ? 20 : 50) : 10), uid);

			if (mysql_query(db, sql) != 0)
			{
				log_error("Change exp error: %s", mysql_error(db));
				ret = -2;
				goto cleanup;
			}
		}

		// Add log
		snprintf(sql, sizeof(sql),
				 "INSERT INTO bbs_article_op(AID, UID, type, op_dt, op_ip) "
				 "VALUES(%d, %d, '%c', NOW(), '%s')",
				 aid, BBS_priv.uid, (set ? 'E' : 'O'), hostaddr_client);

		if (mysql_query(db, sql) != 0)
		{
			log_error("Add log error: %s", mysql_error(db));
			ret = -2;
			goto cleanup;
		}
	}

	if (section_list_rw_lock(p_section) < 0)
	{
		log_error("section_list_rw_lock(sid=%d) error", sid);
		ret = -3;
		goto cleanup;
	}

	ret = section_list_set_article_excerption(p_section, aid, set);
	if (ret < 0)
	{
		log_error("section_list_set_article_excerption(sid=%d, aid=%d, set=%d) error", sid, aid, set);
		ret = -3;
		goto cleanup;
	}

	if (section_list_rw_unlock(p_section) < 0)
	{
		log_error("section_list_rw_unlock(sid=%d) error", sid);
		ret = -3;
		goto cleanup;
	}

	// Commit transaction
	if (mysql_query(db, "COMMIT") != 0)
	{
		log_error("Mysqli error: %s\n", mysql_error(db));

		// Rollback change to avoid data inconsistency
		if (section_list_rw_lock(p_section) < 0)
		{
			log_error("section_list_rw_lock(sid=%d) error", sid);
			ret = -3;
			goto cleanup;
		}

		ret = section_list_set_article_excerption(p_section, aid, excerption);
		if (ret < 0)
		{
			log_error("section_list_set_article_excerption(sid=%d, aid=%d, set=%d) error", sid, aid, excerption);
			ret = -3;
			goto cleanup;
		}

		if (section_list_rw_unlock(p_section) < 0)
		{
			log_error("section_list_rw_unlock(sid=%d) error", sid);
			ret = -3;
			goto cleanup;
		}

		ret = 0;
	}
	else
	{
		ret = 1; // Success
	}

cleanup:
	mysql_free_result(rs);
	mysql_close(db);

	return ret;
}
