/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * user_info_update
 *   - update user information
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 *
 * Co-author:               老黑噜 <2391669999@qq.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bbs.h"
#include "bwf.h"
#include "database.h"
#include "editor.h"
#include "io.h"
#include "lml.h"
#include "log.h"
#include "screen.h"
#include "str_process.h"
#include "user_info_update.h"
#include "user_list.h"
#include "user_priv.h"
#include <ctype.h>
#include <stdlib.h>
#include <sys/param.h>

enum bbs_user_sign_const_t
{
	BBS_user_sign_max_len = 4096,
	BBS_user_sign_max_line = 10,
	BBS_user_sign_cnt = 3,
};

int user_intro_edit(int uid)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql_intro[SQL_BUFFER_LEN + 2 * BBS_user_intro_max_len + 1];
	EDITOR_DATA *p_editor_data = NULL;
	int ret = 0;
	int ch = 0;
	char intro[BBS_user_intro_max_len + 1];
	char intro_f[BBS_user_intro_max_len * 2 + 1];
	long len_intro = 0L;
	long line_offsets[BBS_user_intro_max_line + 1];
	long lines = 0L;

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	snprintf(sql_intro, sizeof(sql_intro), "SELECT introduction FROM user_pubinfo WHERE UID=%d", uid);
	if (mysql_query(db, sql_intro) != 0)
	{
		log_error("Query user_pubinfo error: %s", mysql_error(db));
		ret = -2;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_intro data failed");
		ret = -2;
		goto cleanup;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		p_editor_data = editor_data_load(row[0] ? row[0] : "");
	}
	else
	{
		log_error("mysql_fetch_row() failed");
		ret = -2;
		goto cleanup;
	}
	mysql_free_result(rs);
	rs = NULL;
	mysql_close(db);
	db = NULL;

	for (ch = 'E'; !SYS_server_exit;)
	{
		if (ch == 'E')
		{
			editor_display(p_editor_data);
		}

		clearscr();
		moveto(1, 1);
		prints("(S)保存, (C)取消 or (E)再编辑? [S]: ");
		iflush();

		ch = igetch_t(BBS_max_user_idle_time);
		switch (toupper(ch))
		{
		case KEY_NULL:
		case KEY_TIMEOUT:
			goto cleanup;
		case CR:
		case 'S':
			len_intro = editor_data_save(p_editor_data, intro, BBS_user_intro_max_len);
			if (len_intro < 0)
			{
				log_error("editor_data_save() error");
				ret = -3;
				goto cleanup;
			}

			if (check_badwords(intro, '*') < 0)
			{
				log_error("check_badwords(introduction) error");
				ret = -3;
				goto cleanup;
			}

			lml_render(intro, intro_f, sizeof(intro_f), SCREEN_COLS, 0);

			lines = split_data_lines(intro_f, SCREEN_COLS + 1, line_offsets, BBS_user_intro_max_line + 2, 1, NULL);
			if (lines > BBS_user_intro_max_line)
			{
				clearscr();
				moveto(1, 1);
				prints("说明档长度超过限制 (%d行)，请返回修改", BBS_user_intro_max_line);
				press_any_key();

				ch = 'E';
				continue;
			}
			break;
		case 'C':
			clearscr();
			moveto(1, 1);
			prints("取消更改...");
			press_any_key();
			goto cleanup;
		case 'E':
			ch = 'E';
			continue;
		default: // Invalid selection
			continue;
		}

		break;
	}

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Secure SQL parameters
	mysql_real_escape_string(db, intro_f, intro, (unsigned long)len_intro);

	// Update user intro
	snprintf(sql_intro, sizeof(sql_intro),
			 "UPDATE user_pubinfo SET introduction = '%s' WHERE UID=%d",
			 intro_f, uid);

	if (mysql_query(db, sql_intro) != 0)
	{
		log_error("Update user_pubinfo error: %s", mysql_error(db));
		ret = -2;
		goto cleanup;
	}

	mysql_close(db);
	db = NULL;

	clearscr();
	moveto(1, 1);
	prints("说明档修改完成，会在%d秒内生效", BBS_user_list_load_interval);
	press_any_key();
	ret = 1; // Success

cleanup:
	mysql_free_result(rs);
	mysql_close(db);

	// Cleanup buffers
	editor_data_cleanup(p_editor_data);

	return ret;
}

int user_sign_edit(int uid)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql_sign[SQL_BUFFER_LEN + 2 * BBS_user_sign_max_len + 1];
	EDITOR_DATA *p_editor_data = NULL;
	int ret = 0;
	int ch = 0;
	char sign[BBS_user_sign_max_len + 1];
	char sign_f[BBS_user_sign_max_len * 2 + 1];
	long len_sign = 0L;
	long line_offsets[BBS_user_sign_max_line + 1];
	long lines = 0L;
	char buf[2] = "";
	int sign_id = 0;

	clearscr();
	get_data(1, 1, "请输入需要编辑的签名档编号（1-3）: ", buf, sizeof(buf), 1);
	sign_id = atoi(buf);
	if (sign_id < 1 || sign_id > BBS_user_sign_cnt)
	{
		ret = -1;
		goto cleanup;
	}

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	snprintf(sql_sign, sizeof(sql_sign), "SELECT sign_%d FROM user_pubinfo WHERE UID=%d", sign_id, uid);
	if (mysql_query(db, sql_sign) != 0)
	{
		log_error("Query user_pubinfo error: %s", mysql_error(db));
		ret = -2;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_sign data failed");
		ret = -2;
		goto cleanup;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		p_editor_data = editor_data_load(row[0] ? row[0] : "");
	}
	else
	{
		log_error("mysql_fetch_row() failed");
		ret = -2;
		goto cleanup;
	}
	mysql_free_result(rs);
	rs = NULL;

	mysql_close(db);
	db = NULL;

	for (ch = 'E'; !SYS_server_exit;)
	{
		if (ch == 'E')
		{
			editor_display(p_editor_data);
		}

		clearscr();
		moveto(1, 1);
		prints("(S)保存, (C)取消 or (E)再编辑? [S]: ");
		iflush();

		ch = igetch_t(BBS_max_user_idle_time);
		switch (toupper(ch))
		{
		case KEY_NULL:
		case KEY_TIMEOUT:
			goto cleanup;
		case CR:
		case 'S':
			len_sign = editor_data_save(p_editor_data, sign, BBS_user_sign_max_len);
			if (len_sign < 0)
			{
				log_error("editor_data_save() error");
				ret = -3;
				goto cleanup;
			}

			if (check_badwords(sign, '*') < 0)
			{
				log_error("check_badwords(sign) error");
				ret = -3;
				goto cleanup;
			}

			lml_render(sign, sign_f, sizeof(sign_f), SCREEN_COLS, 0);

			lines = split_data_lines(sign_f, SCREEN_COLS + 1, line_offsets, BBS_user_sign_max_line + 2, 1, NULL);
			if (lines > BBS_user_sign_max_line)
			{
				clearscr();
				moveto(1, 1);
				prints("签名档长度超过限制 (%d行)，请返回修改", BBS_user_sign_max_line);
				press_any_key();

				ch = 'E';
				continue;
			}
			break;
		case 'C':
			clearscr();
			moveto(1, 1);
			prints("取消更改...");
			press_any_key();
			goto cleanup;
		case 'E':
			ch = 'E';
			continue;
		default: // Invalid selection
			continue;
		}

		break;
	}

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Secure SQL parameters
	mysql_real_escape_string(db, sign_f, sign, (unsigned long)len_sign);

	// Update user sign
	snprintf(sql_sign, sizeof(sql_sign),
			 "UPDATE user_pubinfo SET sign_%d = '%s' WHERE UID=%d",
			 sign_id, sign_f, uid);

	if (mysql_query(db, sql_sign) != 0)
	{
		log_error("Update user_pubinfo error: %s", mysql_error(db));
		ret = -2;
		goto cleanup;
	}

	mysql_close(db);
	db = NULL;

	clearscr();
	moveto(1, 1);
	prints("签名档修改完成");
	press_any_key();
	ret = 1; // Success

cleanup:
	mysql_free_result(rs);
	mysql_close(db);

	// Cleanup buffers
	editor_data_cleanup(p_editor_data);

	return ret;
}
