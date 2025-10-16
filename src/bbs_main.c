/***************************************************************************
						  bbs_main.c  -  description
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
#include "article_view_log.h"
#include "bbs.h"
#include "bbs_cmd.h"
#include "bbs_main.h"
#include "common.h"
#include "database.h"
#include "editor.h"
#include "io.h"
#include "log.h"
#include "login.h"
#include "menu.h"
#include "screen.h"
#include "section_list.h"
#include "trie_dict.h"
#include "user_priv.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int bbs_info()
{
	prints("欢迎光临 \033[1;33m%s \033[32m[%s]  \033[37m( %s )\033[m\r\n",
		   BBS_name, BBS_server, APP_INFO);

	return iflush();
}

int bbs_welcome(void)
{
	char sql[SQL_BUFFER_LEN];

	u_int32_t u_online = 0;
	u_int32_t u_anonymous = 0;
	u_int32_t u_total = 0;
	u_int32_t u_login_count = 0;

	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;

	db = db_open();
	if (db == NULL)
	{
		return -1;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT COUNT(*) AS cc FROM "
			 "(SELECT DISTINCT SID FROM user_online "
			 "WHERE last_tm >= SUBDATE(NOW(), INTERVAL %d SECOND)) AS t1",
			 BBS_user_off_line);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_online error: %s\n", mysql_error(db));
		mysql_close(db);
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_online data failed\n");
		mysql_close(db);
		return -2;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		u_online = (u_int32_t)atoi(row[0]);
	}
	mysql_free_result(rs);

	snprintf(sql, sizeof(sql),
			 "SELECT COUNT(*) AS cc FROM "
			 "(SELECT DISTINCT SID FROM user_online "
			 "WHERE UID = 0 AND last_tm >= SUBDATE(NOW(), INTERVAL %d SECOND)) AS t1",
			 BBS_user_off_line);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_online error: %s\n", mysql_error(db));
		mysql_close(db);
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_online data failed\n");
		mysql_close(db);
		return -2;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		u_anonymous = (u_int32_t)atoi(row[0]);
	}
	mysql_free_result(rs);

	snprintf(sql, sizeof(sql), "SELECT COUNT(UID) AS cc FROM user_list WHERE enable");
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_list error: %s\n", mysql_error(db));
		mysql_close(db);
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_list data failed\n");
		mysql_close(db);
		return -2;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		u_total = (u_int32_t)atoi(row[0]);
	}
	mysql_free_result(rs);

	snprintf(sql, sizeof(sql), "SELECT ID FROM user_login_log ORDER BY ID LIMIT 1");
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_login_log error: %s\n", mysql_error(db));
		mysql_close(db);
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_login_log data failed\n");
		mysql_close(db);
		return -2;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		u_login_count = (u_int32_t)atoi(row[0]);
	}
	mysql_free_result(rs);

	mysql_close(db);

	// Count current user before login
	u_online++;
	u_anonymous++;

	// Display logo
	display_file(DATA_WELCOME, 2);

	// Display welcome message
	prints("\r\033[1;35m欢迎光临\033[33m 【 %s 】 \033[35mBBS\r\n"
		   "\033[32m目前上站人数 [\033[36m%d/%d\033[32m] "
		   "匿名游客[\033[36m%d\033[32m] "
		   "注册用户数[\033[36m%d/%d\033[32m]\r\n"
		   "从 [\033[36m%s\033[32m] 起，累计访问人次：[\033[36m%d\033[32m]\033[m\r\n",
		   BBS_name, u_online, BBS_max_client, u_anonymous, u_total,
		   BBS_max_user, BBS_start_dt, u_login_count);

	iflush();

	return 0;
}

int bbs_logout(void)
{
	MYSQL *db;

	db = db_open();
	if (db == NULL)
	{
		return -1;
	}

	if (user_online_exp(db) < 0)
	{
		return -2;
	}

	if (user_online_del(db) < 0)
	{
		return -3;
	}

	mysql_close(db);

	display_file(DATA_GOODBYE, 1);

	log_common("User [%s] logout\n", BBS_username);

	return 0;
}

int bbs_center()
{
	int ch;
	time_t t_last_action;

	BBS_last_access_tm = t_last_action = time(NULL);

	clearscr();

	show_top("", BBS_name, "");
	show_active_board();
	show_bottom("");
	display_menu(&bbs_menu);
	iflush();

	while (!SYS_server_exit)
	{
		ch = igetch(100);

        if (ch != KEY_NULL && ch != KEY_TIMEOUT)
        {
            BBS_last_access_tm = time(NULL);
        }

		if (bbs_menu.choose_step == 0 && time(NULL) - t_last_action >= 10)
		{
			t_last_action = time(NULL);

			show_active_board();
			show_bottom("");
			display_menu_cursor(&bbs_menu, 1);
			iflush();
		}

		if (user_online_update("MENU") < 0)
		{
			log_error("user_online_update(MENU) error\n");
		}

		switch (ch)
		{
		case KEY_NULL: // broken pipe
			log_error("KEY_NULL\n");
			return 0;
		case KEY_TIMEOUT:
			if (time(NULL) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				log_error("User input timeout\n");
				return 0;
			}
			continue;
		case CR:
			igetch_reset();
		default:
			switch (menu_control(&bbs_menu, ch))
			{
			case EXITBBS:
			case EXITMENU:
				return 0;
			case REDRAW:
				t_last_action = time(NULL);
				clearscr();
				show_top("", BBS_name, "");
				show_active_board();
				show_bottom("");
				display_menu(&bbs_menu);
				break;
			case NOREDRAW:
			case UNKNOWN_CMD:
			default:
				break;
			}
			iflush();
		}
	}

	return 0;
}

int bbs_main()
{
	struct sigaction act = {0};

	// Set signal handler
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, NULL) == -1)
	{
		log_error("set signal action of SIGHUP error: %d\n", errno);
		goto cleanup;
	}
	act.sa_handler = SIG_DFL;
	if (sigaction(SIGCHLD, &act, NULL) == -1)
	{
		log_error("set signal action of SIGCHLD error: %d\n", errno);
		goto cleanup;
	}

	// Set data pools in shared memory readonly
	if (set_trie_dict_shm_readonly() < 0)
	{
		goto cleanup;
	}
	if (set_article_block_shm_readonly() < 0)
	{
		goto cleanup;
	}
	if (set_section_list_shm_readonly() < 0)
	{
		goto cleanup;
	}

	// Load menu in shared memory
	if (set_menu_shm_readonly(&bbs_menu) < 0)
	{
		goto cleanup;
	}

	set_input_echo(0);

	// System info
	if (bbs_info() < 0)
	{
		goto cleanup;
	}

	// Welcome
	if (bbs_welcome() < 0)
	{
		goto cleanup;
	}

	// User login
	if (SSH_v2)
	{
		prints("\033[1m%s 欢迎使用ssh方式访问 \033[1;33m按任意键继续...\033[m", BBS_username);
		iflush();
		igetch_t(MAX_DELAY_TIME);
	}
	else if (bbs_login() < 0)
	{
		goto cleanup;
	}
	log_common("User [%s] login\n", BBS_username);

	// Load article view log
	if (article_view_log_load(BBS_priv.uid, &BBS_article_view_log, 0) < 0)
	{
		log_error("article_view_log_load() error\n");
		goto cleanup;
	}

	// Load article favorite
	if (article_favor_load(BBS_priv.uid, &BBS_article_favor, 0) < 0)
	{
		log_error("article_favor_load() error\n");
		goto cleanup;
	}

	// Init editor memory pool
	if (editor_memory_pool_init() < 0)
	{
		log_error("editor_memory_pool_init() error\n");
		goto cleanup;
	}

	clearscr();

	// BBS Top 10
	display_file(VAR_BBS_TOP, 1);

	// Main
	bbs_center();

	// Logout
	bbs_logout();

	// Save incremental article view log
	if (article_view_log_save_inc(&BBS_article_view_log) < 0)
	{
		log_error("article_view_log_save_inc() error\n");
	}

	// Save incremental article favorite
	if (article_favor_save_inc(&BBS_article_favor) < 0)
	{
		log_error("article_favor_save_inc() error\n");
	}

cleanup:
	// Cleanup editor memory pool
	editor_memory_pool_cleanup();

	// Unload article view log
	article_view_log_unload(&BBS_article_view_log);

	// Unload article favor
	article_favor_unload(&BBS_article_favor);

	// Detach menu in shared memory
	detach_menu_shm(&bbs_menu);
	detach_menu_shm(&top10_menu);

	// Detach data pools shm
	detach_section_list_shm();
	detach_article_block_shm();
	detach_trie_dict_shm();

	return 0;
}
