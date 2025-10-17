/***************************************************************************
						article_post.c  -  description
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

#include "article_cache.h"
#include "article_post.h"
#include "bbs.h"
#include "database.h"
#include "editor.h"
#include "io.h"
#include "log.h"
#include "lml.h"
#include "screen.h"
#include "user_priv.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define TITLE_INPUT_MAX_LEN 72
#define ARTICLE_CONTENT_MAX_LEN 1024 * 1024 * 4 // 4MB
#define ARTICLE_QUOTE_MAX_LINES 20
#define ARTICLE_QUOTE_LINE_MAX_LEN 76

#define MODIFY_DT_MAX_LEN 50

int article_post(const SECTION_LIST *p_section, ARTICLE *p_article_new)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	char *sql_content = NULL;
	EDITOR_DATA *p_editor_data = NULL;
	char title_input[BBS_article_title_max_len + 1];
	char title_f[BBS_article_title_max_len * 2 + 1];
	char *content = NULL;
	char *content_f = NULL;
	long len_content;
	int content_display_length;
	char nickname_f[BBS_nickname_max_len * 2 + 1];
	int sign_id = 0;
	int reply_note = 1;
	long len;
	int ch;
	char *p, *q;
	long ret = 0;

	if (p_section == NULL || p_article_new == NULL)
	{
		log_error("NULL pointer error\n");
	}

	if (!checkpriv(&BBS_priv, p_section->sid, S_POST))
	{
		clearscr();
		moveto(1, 1);
		prints("您没有权限在本版块发表文章\n");
		press_any_key();

		return 0;
	}

	p_article_new->title[0] = '\0';
	title_input[0] = '\0';
	p_article_new->transship = 0;

	p_editor_data = editor_data_load("");
	if (p_editor_data == NULL)
	{
		log_error("editor_data_load() error\n");
		ret = -1;
		goto cleanup;
	}

	// Set title and sign
	for (ch = 'T'; !SYS_server_exit;)
	{
		clearscr();
		moveto(21, 1);
		prints("发表文章于 %s[%s] 讨论区，类型: %s，回复通知：%s",
			   p_section->stitle, p_section->sname,
			   (p_article_new->transship ? "转载" : "原创"),
			   (reply_note ? "开启" : "关闭"));
		moveto(22, 1);
		prints("标题: %s", (p_article_new->title[0] == '\0' ? "[无]" : p_article_new->title));
		moveto(23, 1);
		prints("使用第 [1;32m%d[m 个签名", sign_id);

		if (toupper(ch) != 'T')
		{
			prints("    按[1;32m0[m~[1;32m3[m选签名档(0表示不使用)");

			moveto(24, 1);
			prints("[1;32mT[m改标题, [1;32mC[m取消, [1;32mZ[m设为%s, [1;32mN[m%s, [1;32mEnter[m继续: ",
				   (p_article_new->transship ? "原创" : "转载"),
				   (reply_note ? "关闭回复通知" : "开启回复通知"));
			iflush();
			ch = 0;
		}

		for (; !SYS_server_exit; ch = igetch_t(MAX_DELAY_TIME))
		{
			switch (toupper(ch))
			{
			case KEY_NULL:
			case KEY_TIMEOUT:
				goto cleanup;
			case CR:
				break;
			case 'T':
				len = get_data(24, 1, "标题: ", title_input, sizeof(title_input), TITLE_INPUT_MAX_LEN);
				for (p = title_input; *p == ' '; p++)
					;
				for (q = title_input + len; q > p && *(q - 1) == ' '; q--)
					;
				*q = '\0';
				len = q - p;
				if (*p != '\0')
				{
					memcpy(p_article_new->title, p, (size_t)len + 1);
					memcpy(title_input, p_article_new->title, (size_t)len + 1);
				}
				ch = 0;
				break;
			case 'C':
				clearscr();
				moveto(1, 1);
				prints("取消...");
				press_any_key();
				goto cleanup;
			case 'Z':
				p_article_new->transship = (p_article_new->transship ? 0 : 1);
				break;
			case 'N':
				reply_note = (reply_note ? 0 : 1);
				break;
			case '0':
			case '1':
			case '2':
			case '3':
				sign_id = ch - '0';
				break;
			default: // Invalid selection
				continue;
			}

			break;
		}

		if (ch != CR || p_article_new->title[0] == '\0')
		{
			continue;
		}

		for (ch = 'E'; !SYS_server_exit && toupper(ch) == 'E';)
		{
			editor_display(p_editor_data);

			clearscr();
			moveto(1, 1);
			prints("(S)发送, (C)取消, (T)更改标题 or (E)再编辑? [S]: ");
			iflush();

			for (ch = 0; !SYS_server_exit; ch = igetch_t(MAX_DELAY_TIME))
			{
				switch (toupper(ch))
				{
				case KEY_NULL:
				case KEY_TIMEOUT:
					goto cleanup;
				case CR:
				case 'S':
					break;
				case 'C':
					clearscr();
					moveto(1, 1);
					prints("取消...");
					press_any_key();
					goto cleanup;
				case 'T':
					break;
				case 'E':
					break;
				default: // Invalid selection
					continue;
				}

				break;
			}
		}

		if (toupper(ch) != 'T')
		{
			break;
		}
	}

	if (SYS_server_exit) // Do not save data on shutdown
	{
		goto cleanup;
	}

	content = malloc(ARTICLE_CONTENT_MAX_LEN);
	if (content == NULL)
	{
		log_error("malloc(content) error: OOM\n");
		ret = -1;
		goto cleanup;
	}

	len_content = editor_data_save(p_editor_data, content, ARTICLE_CONTENT_MAX_LEN);
	if (len_content < 0)
	{
		log_error("editor_data_save() error\n");
		ret = -1;
		goto cleanup;
	}

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	if (sign_id > 0)
	{
		snprintf(sql, sizeof(sql),
				 "SELECT sign_%d AS sign FROM user_pubinfo WHERE UID = %d",
				 sign_id, BBS_priv.uid);

		if (mysql_query(db, sql) != 0)
		{
			log_error("Query sign error: %s\n", mysql_error(db));
			ret = -1;
			goto cleanup;
		}
		if ((rs = mysql_use_result(db)) == NULL)
		{
			log_error("Get sign data failed\n");
			ret = -1;
			goto cleanup;
		}

		if ((row = mysql_fetch_row(rs)))
		{
			len_content += snprintf(content + len_content,
									ARTICLE_CONTENT_MAX_LEN - (size_t)len_content,
									"\n\n--\n%s\n", row[0]);
		}
		mysql_free_result(rs);
		rs = NULL;
	}

	// Calculate display length of content
	content_display_length = str_length(content, 1);

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

	// Secure SQL parameters
	content_f = malloc((size_t)len_content * 2 + 1);
	if (content_f == NULL)
	{
		log_error("malloc(content_f) error: OOM\n");
		ret = -1;
		goto cleanup;
	}

	mysql_real_escape_string(db, nickname_f, BBS_nickname, (unsigned long)strnlen(BBS_nickname, sizeof(BBS_nickname)));
	mysql_real_escape_string(db, title_f, p_article_new->title, strnlen(p_article_new->title, sizeof(p_article_new->title)));
	mysql_real_escape_string(db, content_f, content, (unsigned long)len_content);

	free(content);
	content = NULL;

	// Add content
	sql_content = malloc(SQL_BUFFER_LEN + (size_t)len_content * 2 + 1);
	if (sql_content == NULL)
	{
		log_error("malloc(sql_content) error: OOM\n");
		ret = -1;
		goto cleanup;
	}

	snprintf(sql_content, SQL_BUFFER_LEN + (size_t)len_content * 2 + 1,
			 "INSERT INTO bbs_content(AID, content) values(0, '%s')",
			 content_f);

	free(content_f);
	content_f = NULL;

	if (mysql_query(db, sql_content) != 0)
	{
		log_error("Add article content error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	p_article_new->cid = (int32_t)mysql_insert_id(db);

	free(sql_content);
	sql_content = NULL;

	// Add article
	snprintf(sql, sizeof(sql),
			 "INSERT INTO bbs(SID, TID, UID, username, nickname, title, CID, transship, "
			 "sub_dt, sub_ip, reply_note, exp, last_reply_dt, icon, length) "
			 "VALUES(%d, 0, %d, '%s', '%s', '%s', %d, %d, NOW(), '%s', %d, %d, NOW(), 1, %d)",
			 p_section->sid, BBS_priv.uid, BBS_username, nickname_f, title_f,
			 p_article_new->cid, p_article_new->transship, hostaddr_client,
			 reply_note, BBS_user_exp, content_display_length);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Add article error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	p_article_new->aid = (int32_t)mysql_insert_id(db);

	// Link content to article
	snprintf(sql, sizeof(sql),
			 "UPDATE bbs_content SET AID = %d WHERE CID = %d",
			 p_article_new->aid, p_article_new->cid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Update content error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Add exp
	if (checkpriv(&BBS_priv, p_section->sid, S_GETEXP)) // Except in test section
	{
		snprintf(sql, sizeof(sql),
				 "UPDATE user_pubinfo SET exp = exp + %d WHERE UID = %d",
				 (p_article_new->transship ? 5 : 15), BBS_priv.uid);

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
			 "VALUES(%d, %d, 'A', NOW(), '%s')",
			 p_article_new->aid, BBS_priv.uid, hostaddr_client);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Add log error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
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
	prints("发送完成，新文章通常会在%d秒后可见", BBS_section_list_load_interval);
	press_any_key();
	ret = 1; // Success

cleanup:
	mysql_close(db);

	// Cleanup buffers
	editor_data_cleanup(p_editor_data);

	free(sql_content);
	free(content);
	free(content_f);

	return (int)ret;
}

int article_modify(const SECTION_LIST *p_section, const ARTICLE *p_article, ARTICLE *p_article_new)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	char *sql_content = NULL;
	char *content = NULL;
	char *content_f = NULL;
	long len_content;
	int content_display_length;
	int reply_note = 1;
	int ch;
	long ret = 0;
	time_t now;
	struct tm tm_modify_dt;
	char str_modify_dt[MODIFY_DT_MAX_LEN + 1];

	EDITOR_DATA *p_editor_data = NULL;

	if (p_section == NULL || p_article == NULL)
	{
		log_error("NULL pointer error\n");
	}

	if (p_article->excerption) // Modify is not allowed
	{
		clearscr();
		moveto(1, 1);
		prints("该文章无法被编辑，请联系版主。");
		press_any_key();

		return 0;
	}

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT bbs_content.CID, bbs_content.content, reply_note "
			 "FROM bbs INNER JOIN bbs_content ON bbs.CID = bbs_content.CID "
			 "WHERE bbs.AID = %d",
			 p_article->aid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article content error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	if ((rs = mysql_use_result(db)) == NULL)
	{
		log_error("Get article content data failed\n");
		ret = -1;
		goto cleanup;
	}

	if ((row = mysql_fetch_row(rs)))
	{
		content = malloc(ARTICLE_CONTENT_MAX_LEN);
		if (content == NULL)
		{
			log_error("malloc(content) error: OOM\n");
			ret = -1;
			goto cleanup;
		}

		strncpy(content, row[1], ARTICLE_CONTENT_MAX_LEN - 1);
		content[ARTICLE_CONTENT_MAX_LEN - 1] = '\0';

		// Remove control sequence
		len_content = str_filter(content, 0);

		p_editor_data = editor_data_load(content);
		if (p_editor_data == NULL)
		{
			log_error("editor_data_load(aid=%d, cid=%d) error\n", p_article->aid, atoi(row[0]));
			ret = -1;
			goto cleanup;
		}

		free(content);
		content = NULL;

		reply_note = atoi(row[2]);
	}
	mysql_free_result(rs);
	rs = NULL;

	mysql_close(db);
	db = NULL;

	for (ch = 'E'; !SYS_server_exit && toupper(ch) == 'E';)
	{
		editor_display(p_editor_data);

		while (!SYS_server_exit)
		{
			clearscr();
			moveto(1, 1);
			prints("(S)保存, (C)取消, (N)%s回复通知 or (E)再编辑? [S]: ",
				   (reply_note ? "关闭" : "开启"));
			iflush();

			ch = igetch_t(MAX_DELAY_TIME);
			switch (toupper(ch))
			{
			case KEY_NULL:
			case KEY_TIMEOUT:
				goto cleanup;
			case CR:
			case 'S':
				break;
			case 'C':
				clearscr();
				moveto(1, 1);
				prints("取消...");
				press_any_key();
				goto cleanup;
			case 'N':
				reply_note = (reply_note ? 0 : 1);
				continue;
			case 'E':
				break;
			default: // Invalid selection
				continue;
			}

			break;
		}
	}

	if (SYS_server_exit) // Do not save data on shutdown
	{
		goto cleanup;
	}

	// Allocate buffers in big size
	content = malloc(ARTICLE_CONTENT_MAX_LEN);
	if (content == NULL)
	{
		log_error("malloc(content) error: OOM\n");
		ret = -1;
		goto cleanup;
	}

	len_content = editor_data_save(p_editor_data, content, ARTICLE_CONTENT_MAX_LEN - LINE_BUFFER_LEN);
	if (len_content < 0)
	{
		log_error("editor_data_save() error\n");
		ret = -1;
		goto cleanup;
	}

	time(&now);
	localtime_r(&now, &tm_modify_dt);
	strftime(str_modify_dt, sizeof(str_modify_dt), "%Y-%m-%d %H:%M:%S (UTC %z)", &tm_modify_dt);

	len_content += snprintf(content + len_content, LINE_BUFFER_LEN,
							"\n--\n※ 作者已于 %s 修改本文※\n",
							str_modify_dt);

	// Calculate display length of content
	content_display_length = str_length(content, 1);

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

	// Secure SQL parameters
	content_f = malloc((size_t)len_content * 2 + 1);
	if (content_f == NULL)
	{
		log_error("malloc(content_f) error: OOM\n");
		ret = -1;
		goto cleanup;
	}

	mysql_real_escape_string(db, content_f, content, (unsigned long)len_content);

	free(content);
	content = NULL;

	// Add content
	sql_content = malloc(SQL_BUFFER_LEN + (size_t)len_content * 2 + 1);
	if (sql_content == NULL)
	{
		log_error("malloc(sql_content) error: OOM\n");
		ret = -1;
		goto cleanup;
	}

	snprintf(sql_content, SQL_BUFFER_LEN + (size_t)len_content * 2 + 1,
			 "INSERT INTO bbs_content(AID, content) values(%d, '%s')",
			 p_article->aid, content_f);

	free(content_f);
	content_f = NULL;

	if (mysql_query(db, sql_content) != 0)
	{
		log_error("Add article content error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	p_article_new->cid = (int32_t)mysql_insert_id(db);

	free(sql_content);
	sql_content = NULL;

	// Update article
	snprintf(sql, sizeof(sql),
			 "UPDATE bbs SET CID = %d, length = %d, reply_note = %d, excerption = 0 WHERE AID = %d", // Set excerption = 0 explictly in case of rare condition
			 p_article_new->cid, content_display_length, reply_note, p_article->aid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Add article error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	if (mysql_query(db, sql) != 0)
	{
		log_error("Update content error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Add log
	snprintf(sql, sizeof(sql),
			 "INSERT INTO bbs_article_op(AID, UID, type, op_dt, op_ip)"
			 "VALUES(%d, %d, 'M', NOW(), '%s')",
			 p_article->aid, BBS_priv.uid, hostaddr_client);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Add log error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
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
	prints("修改完成，新内容通常会在%d秒后可见", BBS_section_list_load_interval);
	press_any_key();
	ret = 1; // Success

cleanup:
	mysql_free_result(rs);
	mysql_close(db);

	// Cleanup buffers
	editor_data_cleanup(p_editor_data);

	free(sql_content);
	free(content);
	free(content_f);

	return (int)ret;
}

int article_reply(const SECTION_LIST *p_section, const ARTICLE *p_article, ARTICLE *p_article_new)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	long line_offsets[ARTICLE_QUOTE_MAX_LINES + 1];
	char sql[SQL_BUFFER_LEN];
	char *sql_content = NULL;
	EDITOR_DATA *p_editor_data = NULL;
	char title_input[BBS_article_title_max_len + sizeof("Re: ")];
	char title_f[BBS_article_title_max_len * 2 + 1];
	char *content = NULL;
	char *content_f = NULL;
	long len_content;
	int content_display_length;
	char nickname_f[BBS_nickname_max_len * 2 + 1];
	int sign_id = 0;
	int reply_note = 0;
	long len;
	int ch;
	char *p, *q;
	int eol;
	int display_len;
	long quote_content_lines;
	long i;
	long ret = 0;
	int topic_locked = 0;
	char msg[BBS_msg_max_len];
	char msg_f[BBS_msg_max_len * 2 + 1];
	int len_msg;

	if (p_section == NULL || p_article == NULL)
	{
		log_error("NULL pointer error\n");
	}

	if (!checkpriv(&BBS_priv, p_section->sid, S_POST))
	{
		clearscr();
		moveto(1, 1);
		prints("您没有权限在本版块发表文章\n");
		press_any_key();

		return 0;
	}

	p_article_new->title[0] = '\0';
	snprintf(title_input, sizeof(title_input), "Re: %s", p_article->title);
	len = split_line(title_input, TITLE_INPUT_MAX_LEN, &eol, &display_len, 0);
	title_input[len] = '\0';

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT `lock` FROM bbs WHERE AID = %d",
			 (p_article->tid == 0 ? p_article->aid : p_article->tid));

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article status error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get article status data failed\n");
		ret = -1;
		goto cleanup;
	}

	if ((row = mysql_fetch_row(rs)))
	{
		if (atoi(row[0]) != 0)
		{
			topic_locked = 1;
		}
	}
	mysql_free_result(rs);
	rs = NULL;

	if (topic_locked) // Reply is not allowed
	{
		mysql_close(db);
		db = NULL;

		clearscr();
		moveto(1, 1);
		prints("该主题谢绝回复");
		press_any_key();

		goto cleanup;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT bbs_content.CID, bbs_content.content "
			 "FROM bbs INNER JOIN bbs_content ON bbs.CID = bbs_content.CID "
			 "WHERE bbs.AID = %d",
			 p_article->aid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article content error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	if ((rs = mysql_use_result(db)) == NULL)
	{
		log_error("Get article content data failed\n");
		ret = -1;
		goto cleanup;
	}

	if ((row = mysql_fetch_row(rs)))
	{
		content = malloc(ARTICLE_CONTENT_MAX_LEN);
		if (content == NULL)
		{
			log_error("malloc(content) error: OOM\n");
			ret = -1;
			goto cleanup;
		}

		content_f = malloc(ARTICLE_CONTENT_MAX_LEN);
		if (content_f == NULL)
		{
			log_error("malloc(content_f) error: OOM\n");
			ret = -1;
			goto cleanup;
		}

		// Apply LML render to content body
		len = lml_render(row[1], content_f, ARTICLE_CONTENT_MAX_LEN, 0);
		content_f[len] = '\0';

		// Remove control sequence
		len = str_filter(content_f, 0);

		len = snprintf(content, ARTICLE_CONTENT_MAX_LEN,
					   "\n\n【 在 %s (%s) 的大作中提到: 】\n",
					   p_article->username, p_article->nickname);

		quote_content_lines = split_data_lines(content_f, ARTICLE_QUOTE_LINE_MAX_LEN, line_offsets, ARTICLE_QUOTE_MAX_LINES + 1, 0, NULL);
		for (i = 0; i < quote_content_lines; i++)
		{
			memcpy(content + len, ": ", 2); // quote line prefix
			len += 2;
			memcpy(content + len, content_f + line_offsets[i], (size_t)(line_offsets[i + 1] - line_offsets[i]));
			len += (line_offsets[i + 1] - line_offsets[i]);
			if (content[len - 1] != '\n') // Appennd \n if not exist
			{
				content[len] = '\n';
				len++;
			}
		}
		if (content[len - 1] != '\n') // Appennd \n if not exist
		{
			content[len] = '\n';
			len++;
		}
		content[len] = '\0';

		free(content_f);
		content_f = NULL;

		p_editor_data = editor_data_load(content);
		if (p_editor_data == NULL)
		{
			log_error("editor_data_load(aid=%d, cid=%d) error\n", p_article->aid, atoi(row[0]));
			ret = -1;
			goto cleanup;
		}

		free(content);
		content = NULL;
	}
	mysql_free_result(rs);
	rs = NULL;

	mysql_close(db);
	db = NULL;

	// Set title and sign
	for (ch = 'T'; !SYS_server_exit;)
	{
		clearscr();
		moveto(21, 1);
		prints("回复文章于 %s[%s] 讨论区，回复通知：%s", p_section->stitle, p_section->sname, (reply_note ? "开启" : "关闭"));
		moveto(22, 1);
		prints("标题: %s", (p_article_new->title[0] == '\0' ? "[无]" : p_article_new->title));
		moveto(23, 1);
		prints("使用第 [1;32m%d[m 个签名", sign_id);

		if (toupper(ch) != 'T')
		{
			prints("    按[1;32m0[m~[1;32m3[m选签名档(0表示不使用)");

			moveto(24, 1);
			prints("[1;32mT[m改标题, [1;32mC[m取消, [1;32mN[m%s, [1;32mEnter[m继续: ",
				   (reply_note ? "关闭回复通知" : "开启回复通知"));
			iflush();
			ch = 0;
		}

		for (; !SYS_server_exit; ch = igetch_t(MAX_DELAY_TIME))
		{
			switch (toupper(ch))
			{
			case KEY_NULL:
			case KEY_TIMEOUT:
				goto cleanup;
			case CR:
				break;
			case 'T':
				len = get_data(24, 1, "标题: ", title_input, sizeof(title_input), TITLE_INPUT_MAX_LEN);
				for (p = title_input; *p == ' '; p++)
					;
				for (q = title_input + len; q > p && *(q - 1) == ' '; q--)
					;
				*q = '\0';
				len = q - p;
				if (*p != '\0')
				{
					memcpy(p_article_new->title, p, (size_t)len + 1);
					memcpy(title_input, p_article_new->title, (size_t)len + 1);
				}
				ch = 0;
				break;
			case 'C':
				clearscr();
				moveto(1, 1);
				prints("取消...");
				press_any_key();
				goto cleanup;
			case 'N':
				reply_note = (reply_note ? 0 : 1);
				break;
			case '0':
			case '1':
			case '2':
			case '3':
				sign_id = ch - '0';
				break;
			default: // Invalid selection
				continue;
			}

			break;
		}

		if (ch != CR || p_article_new->title[0] == '\0')
		{
			continue;
		}

		for (ch = 'E'; !SYS_server_exit && toupper(ch) == 'E';)
		{
			editor_display(p_editor_data);

			clearscr();
			moveto(1, 1);
			prints("(S)发送, (C)取消, (T)更改标题 or (E)再编辑? [S]: ");
			iflush();

			for (ch = 0; !SYS_server_exit; ch = igetch_t(MAX_DELAY_TIME))
			{
				switch (toupper(ch))
				{
				case KEY_NULL:
				case KEY_TIMEOUT:
					goto cleanup;
				case CR:
				case 'S':
					break;
				case 'C':
					clearscr();
					moveto(1, 1);
					prints("取消...");
					press_any_key();
					goto cleanup;
				case 'T':
					break;
				case 'E':
					break;
				default: // Invalid selection
					continue;
				}

				break;
			}
		}

		if (toupper(ch) != 'T')
		{
			break;
		}
	}

	if (SYS_server_exit) // Do not save data on shutdown
	{
		goto cleanup;
	}

	content = malloc(ARTICLE_CONTENT_MAX_LEN);
	if (content == NULL)
	{
		log_error("malloc(content) error: OOM\n");
		ret = -1;
		goto cleanup;
	}

	len_content = editor_data_save(p_editor_data, content, ARTICLE_CONTENT_MAX_LEN);
	if (len_content < 0)
	{
		log_error("editor_data_save() error\n");
		ret = -1;
		goto cleanup;
	}

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	if (sign_id > 0)
	{
		snprintf(sql, sizeof(sql),
				 "SELECT sign_%d AS sign FROM user_pubinfo WHERE UID = %d",
				 sign_id, BBS_priv.uid);

		if (mysql_query(db, sql) != 0)
		{
			log_error("Query sign error: %s\n", mysql_error(db));
			ret = -1;
			goto cleanup;
		}
		if ((rs = mysql_use_result(db)) == NULL)
		{
			log_error("Get sign data failed\n");
			ret = -1;
			goto cleanup;
		}

		if ((row = mysql_fetch_row(rs)))
		{
			len_content += snprintf(content + len_content,
									ARTICLE_CONTENT_MAX_LEN - (size_t)len_content,
									"\n\n--\n%s\n", row[0]);
		}
		mysql_free_result(rs);
		rs = NULL;
	}

	// Calculate display length of content
	content_display_length = str_length(content, 1);

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

	// Secure SQL parameters
	content_f = malloc((size_t)len_content * 2 + 1);
	if (content_f == NULL)
	{
		log_error("malloc(content_f) error: OOM\n");
		ret = -1;
		goto cleanup;
	}

	mysql_real_escape_string(db, nickname_f, BBS_nickname, (unsigned long)strnlen(BBS_nickname, sizeof(BBS_nickname)));
	mysql_real_escape_string(db, title_f, p_article_new->title, strnlen(p_article_new->title, sizeof(p_article_new->title)));
	mysql_real_escape_string(db, content_f, content, (unsigned long)len_content);

	free(content);
	content = NULL;

	// Add content
	sql_content = malloc(SQL_BUFFER_LEN + (size_t)len_content * 2 + 1);
	if (sql_content == NULL)
	{
		log_error("malloc(sql_content) error: OOM\n");
		ret = -1;
		goto cleanup;
	}

	snprintf(sql_content, SQL_BUFFER_LEN + (size_t)len_content * 2 + 1,
			 "INSERT INTO bbs_content(AID, content) values(0, '%s')",
			 content_f);

	free(content_f);
	content_f = NULL;

	if (mysql_query(db, sql_content) != 0)
	{
		log_error("Add article content error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	p_article_new->cid = (int32_t)mysql_insert_id(db);

	free(sql_content);
	sql_content = NULL;

	// Add article
	snprintf(sql, sizeof(sql),
			 "INSERT INTO bbs(SID, TID, UID, username, nickname, title, CID, transship, "
			 "sub_dt, sub_ip, reply_note, exp, last_reply_dt, icon, length) "
			 "VALUES(%d, %d, %d, '%s', '%s', '%s', %d, 0, NOW(), '%s', %d, %d, NOW(), 1, %d)",
			 p_section->sid, (p_article->tid == 0 ? p_article->aid : p_article->tid),
			 BBS_priv.uid, BBS_username, nickname_f, title_f,
			 p_article_new->cid, hostaddr_client,
			 reply_note, BBS_user_exp, content_display_length);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Add article error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	p_article_new->aid = (int32_t)mysql_insert_id(db);

	// Update topic article
	snprintf(sql, sizeof(sql),
			 "UPDATE bbs SET reply_count = reply_count + 1, "
			 "last_reply_dt = NOW(), last_reply_UID=%d, last_reply_username = '%s', "
			 "last_reply_nickname = '%s' WHERE AID = %d",
			 BBS_priv.uid, BBS_username, nickname_f,
			 (p_article->tid == 0 ? p_article->aid : p_article->tid));

	if (mysql_query(db, sql) != 0)
	{
		log_error("Update topic article error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Link content to article
	snprintf(sql, sizeof(sql),
			 "UPDATE bbs_content SET AID = %d WHERE CID = %d",
			 p_article_new->aid, p_article_new->cid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Update content error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Notify the authors of the topic / article which is replyed.
	snprintf(sql, sizeof(sql),
			 "SELECT DISTINCT UID FROM bbs WHERE (AID = %d OR AID = %d) "
			 "AND visible AND reply_note AND UID <> %d",
			 p_article->tid, p_article->aid, BBS_priv.uid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Read reply info error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get reply info failed\n");
		ret = -1;
		goto cleanup;
	}

	while ((row = mysql_fetch_row(rs)))
	{
		// Send notification message
		len_msg = snprintf(msg, BBS_msg_max_len,
						   "有人回复了您所发表/回复的文章，快来"
						   "[article %d]看看[/article]《%s》吧！\n",
						   p_article_new->aid, title_f);

		mysql_real_escape_string(db, msg_f, msg, (unsigned long)len_msg);

		snprintf(sql, sizeof(sql),
				 "INSERT INTO bbs_msg(fromUID, toUID, content, send_dt, send_ip) "
				 "VALUES(%d, %d, '%s', NOW(), '%s')",
				 BBS_sys_id, atoi(row[0]), msg_f, hostaddr_client);

		if (mysql_query(db, sql) != 0)
		{
			log_error("Insert msg error: %s\n", mysql_error(db));
			ret = -1;
			goto cleanup;
		}
	}
	mysql_free_result(rs);
	rs = NULL;

	// Add exp
	if (checkpriv(&BBS_priv, p_section->sid, S_GETEXP)) // Except in test section
	{
		snprintf(sql, sizeof(sql),
				 "UPDATE user_pubinfo SET exp = exp + %d WHERE UID = %d",
				 3, BBS_priv.uid);

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
			 "VALUES(%d, %d, 'A', NOW(), '%s')",
			 p_article_new->aid, BBS_priv.uid, hostaddr_client);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Add log error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
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
	prints("发送完成，新文章通常会在%d秒后可见", BBS_section_list_load_interval);
	press_any_key();
	ret = 1; // Success

cleanup:
	mysql_free_result(rs);
	mysql_close(db);

	// Cleanup buffers
	editor_data_cleanup(p_editor_data);

	free(sql_content);
	free(content);
	free(content_f);

	return (int)ret;
}
