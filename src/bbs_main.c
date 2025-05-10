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
#include "welcome.h"
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

int bbs_info()
{
	prints("»¶Ó­¹âÁÙ \033[1;33m%s \033[32m[%s]  \033[37m( %s )\r\n",
		   BBS_name, BBS_server, app_version);

	iflush();

	return 0;
}

int bbs_exit()
{
	display_file_ex(DATA_GOODBYE, 1, 0);

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
	display_menu(get_menu(&bbs_menu, "TOPMENU"));

	while (!SYS_server_exit)
	{
		ch = igetch(0);

		if (time(0) - t_last_action >= 10)
		{
			t_last_action = time(0);
			show_active_board();
			show_bottom("");
		}

		switch (ch)
		{
		case KEY_NULL:
		case KEY_TIMEOUT:
			if (time(0) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				return 0;
			}
			continue;
		default:
			switch (menu_control(&bbs_menu, ch))
			{
			case EXITBBS:
				return 0;
			case REDRAW:
				clearscr();
				show_top("");
				show_active_board();
				show_bottom("");
				display_current_menu(&bbs_menu);
				break;
			case NOREDRAW:
			case UNKNOWN_CMD:
			default:
				break;
			}
		}
		BBS_last_access_tm = time(0);
	}

	return 0;
}

int bbs_main()
{
	int ret;

	set_input_echo(0);

	bbs_info();

	// Welcome
	bbs_welcome();

	// Login
	ret = bbs_login();
	if (ret < 0)
		return -1;
	log_std("User \"%s\"(%ld) login from %s:%d\n",
			BBS_username, BBS_priv.uid, hostaddr_client, port_client);
	clearscr();

	// BBS Top 10
	display_file_ex("./var/bbs_top.txt", 1, 1);

	// Main
	bbs_center();

	// Logout
	bbs_exit();
	log_std("User logout\n");

	MYSQL *db = db_open();
	if (db == NULL)
	{
		return -1;
	}

	user_online_del(db);

	mysql_close(db);

	return 0;
}
