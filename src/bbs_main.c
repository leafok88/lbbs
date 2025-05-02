/***************************************************************************
						  bbs_main.c  -  description
							 -------------------
	begin                : Mon Oct 18 2004
	copyright            : (C) 2004 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "bbs_main.h"
#include "bbs.h"
#include "welcome.h"
#include "login.h"
#include "user_priv.h"
#include "common.h"
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
	char temp[256];

	strcpy(temp, app_home_dir);
	strcat(temp, "data/goodbye.txt");
	display_file_ex(temp, 1, 0);

	sleep(1);

	return 0;
}

int bbs_center()
{
	int ch, result, redraw;
	char temp[256];
	time_t t_last_action;

	BBS_last_access_tm = t_last_action = time(0);

	clearscr();

	show_top("");
	show_active_board();
	show_bottom("");
	display_menu(get_menu(&bbs_menu, "TOPMENU"));

	while (!SYS_exit)
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
			return 0;
		case KEY_TIMEOUT:
			if (time(0) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				return -1;
			}
			continue;
		default:
			redraw = 1;
			switch (menu_control(&bbs_menu, ch))
			{
			case EXITBBS:
				return 0;
			case REDRAW:
				break;
			case NOREDRAW:
			case UNKNOWN_CMD:
			default:
				redraw = 0;
				break;
			}
			if (redraw)
			{
				clearscr();
				show_top("");
				show_active_board();
				show_bottom("");
				display_current_menu(&bbs_menu);
			}
		}
		BBS_last_access_tm = time(0);
	}

	return 0;
}

int bbs_main()
{
	char temp[256];
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
	strcpy(temp, app_home_dir);
	strcat(temp, "data/bbs_top.txt");
	display_file_ex(temp, 1, 1);

	// Main
	bbs_center();

	// Logout
	bbs_exit();
	log_std("User logout\n");

	return 0;
}
