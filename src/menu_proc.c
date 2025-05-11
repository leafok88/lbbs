/***************************************************************************
						  menu_proc.c  -  description
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

#include "bbs.h"
#include "bbs_cmd.h"
#include "common.h"
#include "log.h"
#include "io.h"
#include "screen.h"
#include "menu.h"
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int exec_mbem(const char *str)
{
	void *hdll;
	int (*func)();
	char *c, *s;
	char buf[FILE_PATH_LEN];

	strncpy(buf, str, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	s = strstr(buf, "@mod:");
	if (s)
	{
		c = strstr(s + 5, "#");
		if (c)
		{
			*c = 0;
			c++;
		}

		hdll = dlopen(s + 5, RTLD_LAZY);

		if (hdll)
		{
			char *error;

			if ((func = dlsym(hdll, c ? c : "mod_main")) != NULL)
				func();
			else if ((error = dlerror()) != NULL)
			{
				clearscr();
				prints("%s\r\n", error);
				press_any_key();
			}
			dlclose(hdll);
		}
		else
		{
			clearscr();
			prints("加载库文件 [%s] 失败!!\r\n", s + 5);
			prints("失败原因:%s\r\n", dlerror());
			press_any_key();
		}
	}

	return REDRAW;
}

int exitbbs(const char *s)
{
	return EXITBBS;
}

int license(const char *s)
{
	display_file_ex(DATA_LICENSE, 1, 1);

	return REDRAW;
}

int copyright(const char *s)
{
	display_file_ex(DATA_COPYRIGHT, 1, 1);

	return REDRAW;
}

int reloadbbsmenu(const char *s)
{
	MENU_SET new_menu;

	clearscr();

	if (load_menu(&new_menu, CONF_MENU) < 0)
	{
		log_error("Reload menu failed\n");

		unload_menu(&new_menu);

		prints("菜单配置校验失败\r\n");
	}
	else
	{
		unload_menu(&new_menu);

		if (kill(getppid(), SIGHUP) < 0)
		{
			log_error("Send SIGHUP signal failed (%d)\n", errno);
	
			prints("发送指令失败\r\n");
		}
		else
		{
			prints("已发送指令\r\n");
		}
	}

	press_any_key();

	return REDRAW;
}

int shutdownbbs(const char *s)
{
	log_std("Notify main process to exit\n");

	if (kill(getppid(), SIGTERM) < 0)
	{
		log_error("Send SIGTERM signal failed (%d)\n", errno);
	}

	return REDRAW;
}
