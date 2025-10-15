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

#include "article_cache.h"
#include "article_favor_display.h"
#include "article_view_log.h"
#include "bbs.h"
#include "bbs_cmd.h"
#include "common.h"
#include "io.h"
#include "log.h"
#include "login.h"
#include "menu.h"
#include "section_list_display.h"
#include "screen.h"
#include "user_priv.h"
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

int list_section(void *param)
{
	section_list_display(param, 0);

	return REDRAW;
}

int exec_mbem(void *param)
{
	void *hdll;
	int (*func)();
	char *c, *s;
	char buf[FILE_PATH_LEN];

	strncpy(buf, (const char *)param, sizeof(buf) - 1);
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

int exit_bbs(void *param)
{
	return EXITBBS;
}

int license(void *param)
{
	display_file(DATA_LICENSE, 0);

	return REDRAW;
}

int copyright(void *param)
{
	display_file(DATA_COPYRIGHT, 1);

	return REDRAW;
}

int reload_bbs_conf(void *param)
{
	clearscr();

	if (kill(getppid(), SIGHUP) < 0)
	{
		log_error("Send SIGHUP signal failed (%d)\n", errno);

		prints("发送指令失败\r\n");
	}
	else
	{
		prints("已发送指令\r\n");
	}

	press_any_key();

	return REDRAW;
}

int shutdown_bbs(void *param)
{
	log_common("Notify main process to exit\n");

	if (kill(getppid(), SIGTERM) < 0)
	{
		log_error("Send SIGTERM signal failed (%d)\n", errno);
	}

	return REDRAW;
}

int favor_section_filter(void *param)
{
	MENU_ITEM *p_menu_item = param;

	return (is_favor(&BBS_priv, p_menu_item->priv) && checklevel2(&BBS_priv, p_menu_item->level));
}

static int display_ex_article_key_handler(int *p_key, DISPLAY_CTX *p_ctx)
{
	switch (*p_key)
	{
	case 0: // Set msg
		snprintf(p_ctx->msg, sizeof(p_ctx->msg),
				 "| 返回[\033[32m←\033[33m,\033[32mESC\033[33m] | "
				 "移动[\033[32m↑\033[33m/\033[32m↓\033[33m/\033[32mPgUp\033[33m/\033[32mPgDn\033[33m] | "
				 "帮助[\033[32mh\033[33m] |");
		break;
	}

	return 0;
}

int view_ex_article(void *param)
{
	ARTICLE_CACHE cache;
	ARTICLE *p_article;
	int32_t aid = atoi(param);
	int ret;

	(void)ret;

	p_article = article_block_find_by_aid(aid);
	if (p_article == NULL)
	{
		log_error("article_block_find_by_aid(%d) error\n", aid);
		return NOREDRAW;
	}

	if (article_cache_load(&cache, VAR_ARTICLE_CACHE_DIR, p_article) < 0)
	{
		log_error("article_cache_load(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
		return NOREDRAW;
	}

	if (user_online_update("VIEW_ARTICLE") < 0)
	{
		log_error("user_online_update(VIEW_ARTICLE) error\n");
	}

	ret = display_data(cache.p_data, cache.line_total, cache.line_offsets, 0,
					   display_ex_article_key_handler, DATA_READ_HELP);

	if (article_cache_unload(&cache) < 0)
	{
		log_error("article_cache_unload(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
	}

	// Update article_view_log
	if (article_view_log_set_viewed(p_article->aid, &BBS_article_view_log) < 0)
	{
		log_error("article_view_log_set_viewed(aid=%d) error\n", p_article->aid);
	}

	return REDRAW;
}

int list_ex_section(void *param)
{
	SECTION_LIST *p_section;

	p_section = section_list_find_by_name(param);
	if (p_section == NULL)
	{
		log_error("Section %s not found\n", (const char *)param);
		return -1;
	}

	if (section_list_ex_dir_display(p_section) < 0)
	{
		log_error("section_list_ex_dir_display(sid=%d) error\n", p_section->sid);
	}

	return REDRAW;
}

int show_top10_menu(void *param)
{
	static int show_top10 = 0;
	int ch = 0;

	if (show_top10)
	{
		return NOREDRAW;
	}
	show_top10 = 1;
	
	clearscr();
	show_top("", BBS_name, "");
	show_bottom("");

	if (display_menu(&top10_menu) == 0)
	{
		while (!SYS_server_exit)
		{
			iflush();
			ch = igetch(100);
			switch (ch)
			{
			case KEY_NULL: // broken pipe
				show_top10 = 0;
				return 0;
			case KEY_TIMEOUT:
				if (time(NULL) - BBS_last_access_tm >= MAX_DELAY_TIME)
				{
					show_top10 = 0;
					return 0;
				}
				continue;
			case CR:
				igetch_reset();
			default:
				switch (menu_control(&top10_menu, ch))
				{
				case EXITMENU:
					ch = EXITMENU;
					break;
				case REDRAW:
					clearscr();
					show_bottom("");
					display_menu(&top10_menu);
					break;
				case NOREDRAW:
				case UNKNOWN_CMD:
				default:
					break;
				}
			}

			BBS_last_access_tm = time(NULL);

			if (ch == EXITMENU)
			{
				break;
			}
		}
	}
	
	show_top10 = 0;
	return REDRAW;
}

int locate_article(void *param)
{
	char buf[MAX_MENUITEM_NAME_LENGTH];
	char *sname, *aid, *saveptr;

	strncpy(buf, param, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	sname = strtok_r(buf, "|", &saveptr);
	aid = strtok_r(NULL, "|", &saveptr);

	if (sname == NULL || aid == NULL)
	{
		log_error("top10_locate(%s) error: invalid parameter\n", param);
		return NOREDRAW;
	}

	section_list_display(sname, atoi(aid));

	return REDRAW;
}

int favor_topic(void *param)
{
	if (article_favor_display(&BBS_article_favor) < 0)
	{
		log_error("article_favor_display() error\n");
	}

	return REDRAW;
}
