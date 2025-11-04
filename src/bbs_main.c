/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bbs_main
 *   - entry and major procedures of user interactive access
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

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
#include "section_list_display.h"
#include "trie_dict.h"
#include "user_list.h"
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
	int u_online = 0;
	int u_anonymous = 0;
	int u_total = 0;
	int u_login_count = 0;

	if (get_user_online_list_count(&u_online, &u_anonymous) < 0)
	{
		log_error("get_user_online_list_count() error\n");
		u_online = 0;
	}
	u_online += u_anonymous;
	u_online++; // current user
	if (BBS_priv.uid == 0)
	{
		u_anonymous++;
	}

	if (get_user_list_count(&u_total) < 0)
	{
		log_error("get_user_list_count() error\n");
		u_total = 0;
	}

	if (get_user_login_count(&u_login_count) < 0)
	{
		log_error("get_user_login_count() error\n");
		u_login_count = 0;
	}

	// Display logo
	display_file(DATA_WELCOME, 2);

	// Display welcome message
	prints("\r\033[1;35m欢迎光临\033[33m 【 %s 】 \033[35mBBS\r\n"
		   "\033[32m目前上站人数 [\033[36m%d/%d\033[32m] "
		   "匿名游客[\033[36m%d\033[32m] "
		   "注册用户数[\033[36m%d/%d\033[32m]\r\n"
		   "从 [\033[36m%s\033[32m] 起，累计访问人次：[\033[36m%d\033[32m]\033[m\r\n",
		   BBS_name, u_online, BBS_max_client, u_anonymous, u_total,
		   BBS_max_user_count, BBS_start_dt, u_login_count);

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

	log_common("User [%s] logout, idle for %ld seconds since last input\n", BBS_username, time(NULL) - BBS_last_access_tm);

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

int bbs_charset_select()
{
	char msg[LINE_BUFFER_LEN];
	int ch;

	snprintf(msg, sizeof(msg),
			 "\rChoose character set in 5 seconds [UTF-8, GBK]: [U/g]");

	while (!SYS_server_exit)
	{
		ch = press_any_key_ex(msg, 5);
		switch (ch)
		{
		case KEY_NULL:
			return -1;
		case KEY_TIMEOUT:
		case CR:
		case 'u':
		case 'U':
			return 0;
		case 'g':
		case 'G':
			if (io_conv_init("GBK") < 0)
			{
				log_error("io_conv_init(%s) error\n", "GBK");
				return -1;
			}
			return 0;
		default:
			continue;
		}
	}

	return 0;
}

int bbs_main()
{
	struct sigaction act = {0};
	char msg[LINE_BUFFER_LEN];

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
	if (set_user_list_pool_shm_readonly() < 0)
	{
		goto cleanup;
	}

	// Load menu in shared memory
	if (set_menu_shm_readonly(&bbs_menu) < 0)
	{
		goto cleanup;
	}

	// Set default charset
	if (io_conv_init(BBS_DEFAULT_CHARSET) < 0)
	{
		log_error("io_conv_init(%s) error\n", BBS_DEFAULT_CHARSET);
		goto cleanup;
	}

	set_input_echo(0);

	// Set user charset
	bbs_charset_select();

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
		snprintf(msg, sizeof(msg), "\033[1m%s 欢迎使用ssh方式访问 \033[1;33m按任意键继续...\033[m", BBS_username);
		press_any_key_ex(msg, 60);
	}
	else if (bbs_login() < 0)
	{
		goto cleanup;
	}
	log_common("User [%s] login\n", BBS_username);

	// Load section aid locations
	if (section_aid_locations_load(BBS_priv.uid) < 0)
	{
		log_error("article_view_log_load() error\n");
		goto cleanup;
	}

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

	// Save section aid locations
	if (section_aid_locations_save(BBS_priv.uid) < 0)
	{
		log_error("article_view_log_save() error\n");
	}

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
	// Cleanup iconv
	io_conv_cleanup();

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
	detach_user_list_pool_shm();
	detach_section_list_shm();
	detach_article_block_shm();
	detach_trie_dict_shm();

	return 0;
}
