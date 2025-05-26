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

#include "bbs_main.h"
#include "bbs.h"
#include "login.h"
#include "user_priv.h"
#include "common.h"
#include "database.h"
#include "log.h"
#include "io.h"
#include "screen.h"
#include "menu.h"
#include "bbs_cmd.h"
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

int bbs_info()
{
	prints("欢迎光临 \033[1;33m%s \033[32m[%s]  \033[37m( %s )\r\n",
		   BBS_name, BBS_server, app_version);

	return iflush();
}

int bbs_welcome(MYSQL *db)
{
	char sql[SQL_BUFFER_LEN];

	u_int32_t u_online = 0;
	u_int32_t u_anonymous = 0;
	u_int32_t u_total = 0;
	u_int32_t max_u_online = 0;
	u_int32_t u_login_count = 0;

	MYSQL_RES *rs;
	MYSQL_ROW row;

	snprintf(sql, sizeof(sql),
			 "SELECT COUNT(*) AS cc FROM "
			 "(SELECT DISTINCT SID FROM user_online "
			 "WHERE last_tm >= SUBDATE(NOW(), INTERVAL %d SECOND)) AS t1",
			 BBS_user_off_line);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_online error: %s\n", mysql_error(db));
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_online data failed\n");
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
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_online data failed\n");
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
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_list data failed\n");
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
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_login_log data failed\n");
		return -2;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		u_login_count = (u_int32_t)atoi(row[0]);
	}
	mysql_free_result(rs);

	// Log max user_online
	FILE *fin, *fout;
	if ((fin = fopen(VAR_MAX_USER_ONLINE, "r")) != NULL)
	{
		fscanf(fin, "%d", &max_u_online);
		fclose(fin);
	}
	if (u_online > max_u_online)
	{
		max_u_online = u_online;
		if ((fout = fopen(VAR_MAX_USER_ONLINE, "w")) == NULL)
		{
			log_error("Open max_user_online.dat failed\n");
			return -3;
		}
		fprintf(fout, "%d\n", max_u_online);
		fclose(fout);
	}

	// Count current user before login
	u_online++;
	u_anonymous++;

	// Display logo
	display_file_ex(DATA_WELCOME, 1, 0);

	// Display welcome message
	prints("\r\033[1;35m欢迎光临\033[33m 【 %s 】 \033[35mBBS\r\n"
		   "\033[32m目前上站人数 [\033[36m%d/%d\033[32m] "
		   "匿名游客[\033[36m%d\033[32m] "
		   "注册用户数[\033[36m%d/%d\033[32m]\r\n"
		   "从 [\033[36m%s\033[32m] 起，最高人数记录："
		   "[\033[36m%d\033[32m]，累计访问人次：[\033[36m%d\033[32m]\r\n",
		   BBS_name, u_online, BBS_max_client, u_anonymous, u_total,
		   BBS_max_user, BBS_start_dt, max_u_online, u_login_count);

	return 0;
}

int bbs_logout(MYSQL *db)
{
	if (user_online_del(db) < 0)
	{
		return -1;
	}

	display_file_ex(DATA_GOODBYE, 1, 1);

	log_std("User logout\n");

	return 0;
}

int bbs_center()
{
	int ch;
	time_t t_last_action;

	BBS_last_access_tm = t_last_action = time(0);

	clearscr();

	show_top("");
	show_active_board();
	show_bottom("");
	display_menu(p_bbs_menu);
	iflush();

	while (!SYS_server_exit)
	{
		ch = igetch(100);

		if (p_bbs_menu->choose_step == 0 && time(0) - t_last_action >= 10)
		{
			t_last_action = time(0);

			show_active_board();
			show_bottom("");
			iflush();
		}

		switch (ch)
		{
		case KEY_NULL: // broken pipe
			return 0;
		case KEY_TIMEOUT:
			if (time(0) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				return 0;
			}
			continue;
		case CR:
			igetch_reset();
		default:
			switch (menu_control(p_bbs_menu, ch))
			{
			case EXITBBS:
				return 0;
			case REDRAW:
				t_last_action = time(0);
				clearscr();
				show_top("");
				show_active_board();
				show_bottom("");
				display_menu(p_bbs_menu);
				break;
			case NOREDRAW:
			case UNKNOWN_CMD:
			default:
				break;
			}
			iflush();
		}

		BBS_last_access_tm = time(0);
	}

	return 0;
}

int bbs_main()
{
	MYSQL *db;

	set_input_echo(0);

	// System info
	if (bbs_info() < 0)
	{
		return -1;
	}

	db = db_open();
	if (db == NULL)
	{
		prints("无法连接数据库\n");
		return -2;
	}

	// Welcome
	if (bbs_welcome(db) < 0)
	{
		mysql_close(db);
		return -3;
	}

	// User login
	if (bbs_login(db) < 0)
	{
		mysql_close(db);
		return -4;
	}
	clearscr();

	// BBS Top 10
	display_file_ex(VAR_BBS_TOP, 1, 1);

	// Load menu in shared memory
	if (set_menu_shm_readonly(p_bbs_menu) < 0)
	{
		return -5;
	}

	// Main
	bbs_center();

	// Unload menu in shared memory
	detach_menu_shm(p_bbs_menu);
	free(p_bbs_menu);
	p_bbs_menu = NULL;

	// Logout
	bbs_logout(db);

	// Unload file_loader and trie_dict
	// Do nothing explictly - SHM detached automatically on process exit

	mysql_close(db);

	return 0;
}
