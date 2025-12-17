/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * menu_proc
 *   - handler functions of menu commands
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "article_cache.h"
#include "article_favor_display.h"
#include "article_view_log.h"
#include "bbs.h"
#include "bbs_cmd.h"
#include "bbs_net.h"
#include "chicken.h"
#include "common.h"
#include "io.h"
#include "log.h"
#include "login.h"
#include "menu.h"
#include "section_list_display.h"
#include "screen.h"
#include "user_info_update.h"
#include "user_list_display.h"
#include "user_priv.h"
#include <ctype.h>
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

typedef union exec_handler_t
{
	void *p;
	int (*handler)();
} exec_handler;

int exec_mbem(void *param)
{
	void *hdll;
	exec_handler func;
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

#ifdef _ENABLE_SHARED
		hdll = dlopen(s + 5, RTLD_LAZY);

		if (hdll)
		{
			char *error;

			if ((func.p = dlsym(hdll, c ? c : "mod_main")) != NULL)
			{
				func.handler();
			}
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
#else
		(void)hdll;
		(void)func;

		if (strcasecmp(c, "bbs_net") == 0)
		{
			bbs_net();
		}
		else if (strcasecmp(c, "chicken_main") == 0)
		{
			chicken_main();
		}
		else
		{
			clearscr();
			prints("未知入口 [%s] !!\r\n", c);
			press_any_key();
		}
#endif
	}

	return REDRAW;
}

int exit_bbs(void *param)
{
	return EXITBBS;
}

int eula(void *param)
{
	display_file(DATA_EULA, 0);

	return REDRAW;
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

int version(void *param)
{
	display_file(DATA_VERSION, 1);

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
	char buf[2] = "N";

	clearscr();
	get_data(1, 1, "真的要关闭系统吗[y/N]? ", buf, sizeof(buf), 1);

	if (toupper(buf[0]) == 'Y')
	{
		log_common("Notify main process to exit by [%s]\n", BBS_username);

		if (kill(getppid(), SIGTERM) < 0)
		{
			log_error("Send SIGTERM signal failed (%d)\n", errno);
		}
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
	int loop;

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
		for (loop = 1; !SYS_server_exit && loop;)
		{
			iflush();
			ch = igetch(100);

			if (ch != KEY_NULL && ch != KEY_TIMEOUT)
			{
				BBS_last_access_tm = time(NULL);
			}

			if (user_online_update("TOP10_MENU") < 0)
			{
				log_error("user_online_update(TOP10_MENU) error\n");
			}

			switch (ch)
			{
			case KEY_NULL: // broken pipe
#ifdef _DEBUG
				log_error("KEY_NULL\n");
#endif
				loop = 0;
				break;
			case KEY_TIMEOUT:
				if (time(NULL) - BBS_last_access_tm >= BBS_max_user_idle_time)
				{
					log_error("User input timeout\n");
					loop = 0;
					break;
				}
				continue;
			case CR:
			default:
				switch (menu_control(&top10_menu, ch))
				{
				case EXITMENU:
					loop = 0;
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

int list_user(void *param)
{
	if (user_list_display(0) < 0)
	{
		log_error("user_list_display(all_user) error\n");
	}

	return REDRAW;
}

int list_online_user(void *param)
{
	if (user_list_display(1) < 0)
	{
		log_error("user_list_display(online_user) error\n");
	}

	return REDRAW;
}

int edit_intro(void *param)
{
	if (user_intro_edit(BBS_priv.uid) < 0)
	{
		log_error("user_intro_edit(%d) error\n", BBS_priv.uid);
	}

	return REDRAW;
}

int edit_sign(void *param)
{
	if (user_sign_edit(BBS_priv.uid) < 0)
	{
		log_error("user_sign_edit(%d) error\n", BBS_priv.uid);
	}

	return REDRAW;
}
