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
#include "common.h"
#include "database.h"
#include "io.h"
#include "log.h"
#include "screen.h"
#include "user_priv.h"
#include <stdlib.h>
#include <string.h>

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

int article_exc_set(SECTION_LIST *p_section, int32_t aid, int8_t is_exc)
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
	int8_t set_exc = !is_exc;
	char ret_msg[ret_msg_len] = "";
	int section_set_exc_ret = 0;

	if (p_section == NULL)
	{
		log_error("NULL pointer error\n");
		ret = -1;
		goto cleanup;
	}

	sid = p_section->sid;

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	// Begin transaction
	if (mysql_query(db, "SET autocommit=0") != 0)
	{
		log_error("SET autocommit=0 error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	if (mysql_query(db, "BEGIN") != 0)
	{
		log_error("Begin transaction error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT UID, TID, SID, transship, excerption FROM bbs WHERE visible and AID=%d FOR UPDATE",
			 aid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query bbs error: %s\n", mysql_error(db));
		ret = -2;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get article data failed\n");
		ret = -2;
		goto cleanup;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		if (sid != atoi(row[2]))
		{
			log_error("sid error: sid = %d, row[2]= %s, aid = %d\n", sid, row[2], aid);
			ret = -2;
			goto cleanup;
		}
		uid = atoi(row[0]);
		tid = atoi(row[1]);
		transship = (int8_t)atoi(row[3]);
		excerption = (int8_t)atoi(row[4]);
	}
	else
	{
		log_error("mysql_fetch_row() error: %s\n", mysql_error(db));
		ret = -2;
		goto cleanup;
	}
	mysql_free_result(rs);
	rs = NULL;

	// Check if already set
	if (set_exc == excerption)
	{
		snprintf(ret_msg, sizeof(ret_msg), "已%s文摘区", set_exc ? "收录" : "移出");
		ret = 1;
		goto cleanup;
	}

	// Update article(s)
	snprintf(sql, sizeof(sql),
			 "UPDATE bbs SET excerption = %d WHERE AID=%d",
			 set_exc, aid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Update article set error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Clear gen_ex if unset excerption
	if (set_exc == 0)
	{
		snprintf(sql, sizeof(sql),
				 "UPDATE bbs SET gen_ex = 0, static = 0 WHERE AID=%d OR TID =%d",
				 aid, aid);

		if (mysql_query(db, sql) != 0)
		{
			log_error("Clear gen_ex error: %s\n", mysql_error(db));
			ret = -1;
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
				log_error("Delete ex_dir error: %s\n", mysql_error(db));
				ret = -1;
				goto cleanup;
			}
		}
	}

	// Change UID of attachments
	snprintf(sql, sizeof(sql),
			 "UPDATE upload_file SET UID = %d WHERE ref_AID = %d AND deleted = 0",
			 set_exc ? 0 : uid, aid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Change UID error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Modify exp
	if (checkpriv(&BBS_priv, sid, S_GETEXP)) // Except in test section
	{
		snprintf(sql, sizeof(sql),
				 "UPDATE user_pubinfo SET exp = exp + %d WHERE UID = %d",
				 (set_exc ? 1 : -1) * (tid == 0 ? (transship ? 20 : 50) : 10), uid);

		if (mysql_query(db, sql) != 0)
		{
			log_error("Update exp error: %s\n", mysql_error(db));
			ret = -1;
			goto cleanup;
		}
	}

	// Add log
	snprintf(sql, sizeof(sql),
			 "INSERT INTO bbs_article_op(AID, UID, type, op_dt, op_ip)"
			 "VALUES(%d, %d, '%c', NOW(), '%s')",
			 aid, BBS_priv.uid, set_exc ? BBS_article_set_excerption : BBS_article_unset_excerption, hostaddr_client);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Add log error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	if (section_list_rw_lock(p_section) != 0)
	{
		log_error("section_list_rw_lock error on section %d\n", sid);
		ret = -1;
		goto cleanup;
	}
	section_set_exc_ret = section_list_set_article_excerption(p_section, aid, set_exc);
	if (section_set_exc_ret == 0)
	{
		ret = 1;
		goto cleanup;
	}
	else if (section_set_exc_ret < 0)
	{
		log_error("section_list_set_article_excerption error sid = %d, aid= %d, set_exc= %d\n", sid, aid, set_exc);
		ret = -1;
		goto cleanup;
	}
	if (section_list_rw_unlock(p_section) != 0)
	{
		log_error("section_list_rw_unlock error on section %d\n", sid);
		ret = -1;
		goto cleanup;
	}

	// Commit transaction
	if (mysql_query(db, "COMMIT") != 0)
	{
		if (section_list_rw_lock(p_section) != 0)
		{
			log_error("section_list_rw_lock error on section %d\n", sid);
			ret = -1;
			goto cleanup;
		}
		section_set_exc_ret = section_list_set_article_excerption(p_section, aid, !set_exc);
		if (section_set_exc_ret == 0)
		{
			ret = -1;
			goto cleanup;
		}
		else if (section_set_exc_ret < 0)
		{
			log_error("section_list_set_article_excerption error sid = %d, aid= %d, set_exc= %d\n", sid, aid, !set_exc);
			ret = -1;
			goto cleanup;
		}
		if (section_list_rw_unlock(p_section) != 0)
		{
			log_error("section_list_rw_unlock error on section %d\n", sid);
			ret = -1;
			goto cleanup;
		}

		log_error("Commit transaction error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	mysql_close(db);
	db = NULL;

	snprintf(ret_msg, sizeof(ret_msg), "已%s文摘区", set_exc ? "收录" : "移出");
	ret = 1; // Success

cleanup:
	mysql_free_result(rs);
	mysql_close(db);
	// press_any_key_ex(ret_msg, 3);
	return ret;
}
