/***************************************************************************
						article_del.c  -  description
							 -------------------
	copyright            : (C) 2004-2025 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "article_del.h"
#include "database.h"
#include "log.h"
#include "io.h"
#include "screen.h"
#include "user_priv.h"
#include <ctype.h>
#include <stdlib.h>

int article_del(const SECTION_LIST *p_section, const ARTICLE *p_article)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int ret = 0;
	int ch;
	int article_visible = 0;
	int article_excerption = 0;
	int exp_change = 0;

	if (p_section == NULL || p_article == NULL)
	{
		log_error("NULL pointer error\n");
	}

	if (p_article->excerption) // Delete is not allowed
	{
		clearscr();
		moveto(1, 1);
		prints("该文章无法被删除，请联系版主。");
		press_any_key();

		return 0;
	}

	clearscr();
	moveto(1, 1);
	prints("真的要删除文章？(Y)是, (N)否 [N]: ");
	iflush();

	for (ch = 0; !SYS_server_exit; ch = igetch_t(MAX_DELAY_TIME))
	{
		switch (toupper(ch))
		{
		case KEY_NULL:
		case KEY_TIMEOUT:
			goto cleanup;
		case CR:
			igetch_reset();
		case KEY_ESC:
		case 'N':
			return 0;
		case 'Y':
			break;
		default: // Invalid selection
			continue;
		}

		break;
	}

	if (SYS_server_exit) // Do not save data on shutdown
	{
		goto cleanup;
	}

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
			 "SELECT visible, excerption FROM bbs WHERE AID = %d FOR UPDATE",
			 p_article->aid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article status error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	if ((rs = mysql_use_result(db)) == NULL)
	{
		log_error("Get article status data failed\n");
		ret = -1;
		goto cleanup;
	}

	if ((row = mysql_fetch_row(rs)))
	{
		article_visible = atoi(row[0]);
		article_excerption = atoi(row[1]);
	}
	mysql_free_result(rs);
	rs = NULL;

	if (!article_visible) // Already deleted
	{
		mysql_close(db);
		db = NULL;

		clearscr();
		moveto(1, 1);
		prints("该文章已被删除，请稍后刷新列表。");
		press_any_key();

		goto cleanup;
	}

	if (article_excerption) // Delete is not allowed
	{
		clearscr();
		moveto(1, 1);
		prints("该文章无法被删除，请联系版主。");
		press_any_key();

		goto cleanup;
	}

	// Update article(s)
	snprintf(sql, sizeof(sql),
			 "UPDATE bbs SET visible = 0, reply_count = 0, m_del = %d "
			 "WHERE (AID = %d OR TID = %d) AND visible",
			 (p_article->uid == BBS_priv.uid ? 0 : 1),
			 p_article->aid, p_article->aid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Update article status error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Update exp
	if (p_article->uid == BBS_priv.uid)
	{
		exp_change = (p_article->tid == 0 ? -20 : -5);
	}
	else
	{
		exp_change = (p_article->tid == 0 ? -50 : -15);
	}

	snprintf(sql, sizeof(sql),
			 "UPDATE user_pubinfo SET exp = exp + %d WHERE UID = %d",
			 exp_change, BBS_priv.uid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Update exp error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Add log
	snprintf(sql, sizeof(sql),
			 "INSERT INTO bbs_article_op(AID, UID, type, op_dt, op_ip)"
			 "VALUES(%d, %d, '%c', NOW(), '%s')",
			 p_article->aid, BBS_priv.uid,
			 (p_article->uid == BBS_priv.uid ? 'D' : 'X'),
			 hostaddr_client);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Add log error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Set reply count
	if (p_article->tid != 0)
	{
		snprintf(sql, sizeof(sql),
				 "UPDATE bbs SET reply_count = reply_count - 1 WHERE AID = %d",
				 p_article->tid);

		if (mysql_query(db, sql) != 0)
		{
			log_error("Update article error: %s\n", mysql_error(db));
			ret = -1;
			goto cleanup;
		}
	}

	// Commit transaction
	if (mysql_query(db, "COMMIT") != 0)
	{
		log_error("Commit transaction error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	mysql_close(db);
	db = NULL;

	clearscr();
	moveto(1, 1);
	prints("删除成功，请在%d秒后刷新列表。", BBS_section_list_load_interval);
	press_any_key();
	ret = 1; // Success

cleanup:
	mysql_free_result(rs);
	mysql_close(db);

	return ret;
}
