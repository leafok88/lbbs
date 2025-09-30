/***************************************************************************
					section_list_display.c  -  description
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
#include "article_op.h"
#include "article_post.h"
#include "article_view_log.h"
#include "article_del.h"
#include "common.h"
#include "io.h"
#include "log.h"
#include "login.h"
#include "menu.h"
#include "section_list_display.h"
#include "section_list_loader.h"
#include "screen.h"
#include "str_process.h"
#include "user_priv.h"
#include <string.h>
#include <time.h>
#include <sys/param.h>

static int section_topic_view_mode = 0;
static int section_topic_view_tid = -1;

enum select_cmd_t
{
	EXIT_SECTION = 0,
	VIEW_ARTICLE,
	CHANGE_PAGE,
	SHOW_HELP,
	CHANGE_NAME_DISPLAY,
	POST_ARTICLE,
	EDIT_ARTICLE,
	DELETE_ARTICLE,
	QUERY_ARTICLE,
	FIRST_TOPIC_ARTICLE,
	LAST_TOPIC_ARTICLE,
	VIEW_EX_DIR,
};

static int section_list_draw_items(int page_id, ARTICLE *p_articles[], int article_count, int display_nickname, int ontop_start_offset)
{
	char str_time[LINE_BUFFER_LEN];
	struct tm tm_sub;
	char title_f[BBS_article_title_max_len + 1];
	int title_f_len;
	int eol;
	int len;
	int i;
	char article_flag;
	int is_viewed;
	time_t tm_now;

	time(&tm_now);

	clrline(4, 23);

	for (i = 0; i < article_count; i++)
	{
		if (p_articles[i]->uid == BBS_priv.uid)
		{
			is_viewed = 1;
		}
		else
		{
			is_viewed = article_view_log_is_viewed(p_articles[i]->aid, &BBS_article_view_log);
			if (is_viewed < 0)
			{
				log_error("article_view_log_is_viewed(aid=%d) error\n", p_articles[i]->aid);
				is_viewed = 0;
			}
		}

		if (p_articles[i]->excerption)
		{
			article_flag = (is_viewed ? 'm' : 'M');
		}
		else if (p_articles[i]->lock && is_viewed)
		{
			article_flag = 'x';
		}
		else
		{
			article_flag = (is_viewed ? ' ' : 'N');
		}

		localtime_r(&p_articles[i]->sub_dt, &tm_sub);
		if (tm_now - p_articles[i]->sub_dt < 3600 * 24 * 365)
		{
			strftime(str_time, sizeof(str_time), "%b %e ", &tm_sub);
		}
		else
		{
			strftime(str_time, sizeof(str_time), "%m/%Y", &tm_sub);
		}

		strncpy(title_f, (p_articles[i]->tid == 0 ? "● " : ""), sizeof(title_f) - 1);
		title_f[sizeof(title_f) - 1] = '\0';
		strncat(title_f, (p_articles[i]->transship ? "[转载]" : ""), sizeof(title_f) - 1 - strnlen(title_f, sizeof(title_f)));
		strncat(title_f, p_articles[i]->title, sizeof(title_f) - 1 - strnlen(title_f, sizeof(title_f)));
		len = split_line(title_f, 47 - (display_nickname ? 8 : 0), &eol, &title_f_len, 1);
		if (title_f[len] != '\0')
		{
			title_f[len] = '\0';
		}

		moveto(4 + i, 1);
		if (i >= ontop_start_offset)
		{
			prints("   \033[1;33m[提示]\033[m %c %s%*s %s %s%s\033[m",
				   article_flag,
				   (display_nickname ? p_articles[i]->nickname : p_articles[i]->username),
				   (display_nickname ? BBS_nickname_max_len - (int)strnlen(p_articles[i]->nickname, sizeof(p_articles[i]->nickname))
									 : BBS_username_max_len - (int)strnlen(p_articles[i]->username, sizeof(p_articles[i]->username))),
				   "",
				   str_time,
				   (p_articles[i]->aid == section_topic_view_tid
						? "\033[1;33m"
						: (p_articles[i]->tid == section_topic_view_tid
							   ? "\033[1;36m"
							   : "")),
				   title_f);
		}
		else
		{
			prints("  %s%7d\033[m %c %s%*s %s %s%s\033[m",
				   (p_articles[i]->aid == section_topic_view_tid
						? "\033[1;33m"
						: (p_articles[i]->tid == section_topic_view_tid
							   ? "\033[1;36m"
							   : "")),
				   p_articles[i]->aid,
				   article_flag,
				   (display_nickname ? p_articles[i]->nickname : p_articles[i]->username),
				   (display_nickname ? BBS_nickname_max_len - (int)strnlen(p_articles[i]->nickname, sizeof(p_articles[i]->nickname))
									 : BBS_username_max_len - (int)strnlen(p_articles[i]->username, sizeof(p_articles[i]->username))),
				   "",
				   str_time,
				   (p_articles[i]->aid == section_topic_view_tid
						? "\033[1;33m"
						: (p_articles[i]->tid == section_topic_view_tid
							   ? "\033[1;36m"
							   : "")),
				   title_f);
		}
	}

	return 0;
}

static int section_list_draw_screen(const char *sname, const char *stitle, const char *master_list, int display_nickname)
{
	char str_section_master[LINE_BUFFER_LEN] = "诚征版主中";
	char str_section_name[LINE_BUFFER_LEN];

	if (master_list[0] != '\0')
	{
		snprintf(str_section_master, sizeof(str_section_master), "版主：%s", master_list);
	}
	snprintf(str_section_name, sizeof(str_section_name), "讨论区 [%s]", sname);

	clearscr();
	show_top(str_section_master, stitle, str_section_name);
	moveto(2, 0);
	prints("返回[\033[1;32m←\033[0;37m,\033[1;32mESC\033[0;37m] 选择[\033[1;32m↑\033[0;37m,\033[1;32m↓\033[0;37m] "
		   "阅读[\033[1;32m→\033[0;37m,\033[1;32mENTER\033[0;37m] 发表[\033[1;32mCtrl-P\033[0;37m] "
		   "%s[\033[1;32mn\033[0;37m] 精华区[\033[1;32mx\033[0;37m] 帮助[\033[1;32mh\033[0;37m]\033[m",
		   (display_nickname ? "用户名" : "昵称"));
	moveto(3, 0);
	if (display_nickname)
	{
		prints("\033[44;37m  \033[1;37m 编  号   发布者昵称           日  期  文章标题                               \033[m");
	}
	else
	{
		prints("\033[44;37m  \033[1;37m 编  号   发 布 者     日  期  文章标题                                       \033[m");
	}

	return 0;
}

static enum select_cmd_t section_list_select(int total_page, int item_count, int *p_page_id, int *p_selected_index)
{
	int old_page_id = *p_page_id;
	int old_selected_index = *p_selected_index;
	int ch;
	time_t last_refresh_tm = time(NULL);

	if (item_count > 0 && *p_selected_index >= 0)
	{
		moveto(4 + *p_selected_index, 1);
		outc('>');
		iflush();
	}

	while (!SYS_server_exit)
	{
		ch = igetch(100);

		switch (ch)
		{
		case KEY_ESC:
		case KEY_LEFT:
			BBS_last_access_tm = time(NULL);
		case KEY_NULL:			 // broken pipe
			return EXIT_SECTION; // exit section
		case KEY_TIMEOUT:
			if (time(NULL) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				return EXIT_SECTION; // exit section
			}
			continue;
		case 'n':
			BBS_last_access_tm = time(NULL);
			return CHANGE_NAME_DISPLAY;
		case CR:
			igetch_reset();
		case 'r':
		case KEY_RIGHT:
			if (item_count > 0)
			{
				BBS_last_access_tm = time(NULL);
				return VIEW_ARTICLE;
			}
			break;
		case Ctrl('P'):
			return POST_ARTICLE;
		case 'E':
			if (item_count > 0)
			{
				return EDIT_ARTICLE;
			}
			break;
		case 'd':
			if (item_count > 0)
			{
				return DELETE_ARTICLE;
			}
			break;
		case Ctrl('Q'):
			if (item_count > 0)
			{
				return QUERY_ARTICLE;
			}
			break;
		case KEY_HOME:
			*p_page_id = 0;
		case 'P':
		case KEY_PGUP:
			*p_selected_index = 0;
		case 'k':
		case KEY_UP:
			if (*p_selected_index <= 0)
			{
				if (*p_page_id > 0)
				{
					(*p_page_id)--;
					*p_selected_index = BBS_article_limit_per_page - 1;
				}
			}
			else
			{
				(*p_selected_index)--;
			}
			break;
		case '$':
		case KEY_END:
			if (total_page > 0)
			{
				*p_page_id = total_page - 1;
			}
		case 'N':
		case KEY_PGDN:
			if (item_count > 0)
			{
				*p_selected_index = item_count - 1;
			}
		case 'j':
		case KEY_DOWN:
			if (*p_selected_index + 1 >= item_count) // next page
			{
				if (*p_page_id + 1 < total_page)
				{
					(*p_page_id)++;
					*p_selected_index = 0;
				}
				else // end of last page
				{
					return CHANGE_PAGE; // force refresh pages
				}
			}
			else
			{
				(*p_selected_index)++;
			}
			break;
		case '=':
			if (item_count > 0)
			{
				return FIRST_TOPIC_ARTICLE;
			}
			break;
		case '\\':
			if (item_count > 0)
			{
				return LAST_TOPIC_ARTICLE;
			}
			break;
		case 'h':
			return SHOW_HELP;
		case 'x':
			return VIEW_EX_DIR;
		default:
		}

		if (old_page_id != *p_page_id)
		{
			return CHANGE_PAGE;
		}

		if (item_count > 0 && old_selected_index != *p_selected_index)
		{
			if (old_selected_index >= 0)
			{
				moveto(4 + old_selected_index, 1);
				outc(' ');
			}
			if (*p_selected_index >= 0)
			{
				moveto(4 + *p_selected_index, 1);
				outc('>');
			}
			iflush();

			old_selected_index = *p_selected_index;
		}

		BBS_last_access_tm = time(NULL);
		if (BBS_last_access_tm - last_refresh_tm >= BBS_section_list_load_interval)
		{
			return CHANGE_PAGE; // force section list refresh
		}
	}

	return EXIT_SECTION;
}

static int display_article_key_handler(int *p_key, DISPLAY_CTX *p_ctx)
{
	switch (*p_key)
	{
	case 'p':
	case Ctrl('X'):
		section_topic_view_mode = !section_topic_view_mode;
	case 0: // Set msg
		if (section_topic_view_mode)
		{
			snprintf(p_ctx->msg, sizeof(p_ctx->msg),
					 "| 返回[\033[32m←\033[33m,\033[32mESC\033[33m] "
					 "同主题阅读[\033[32m↑\033[33m/\033[32m↓\033[33m] "
					 "切换[\033[32mp\033[33m] 回复[\033[32mr\033[33m] 帮助[\033[32mh\033[33m] |");
		}
		else
		{
			snprintf(p_ctx->msg, sizeof(p_ctx->msg),
					 "| 返回[\033[32m←\033[33m,\033[32mESC\033[33m] "
					 "移动[\033[32m↑\033[33m/\033[32m↓\033[33m/\033[32mPgUp\033[33m/\033[32mPgDn\033[33m] "
					 "切换[\033[32mp\033[33m] 回复[\033[32mr\033[33m] 帮助[\033[32mh\033[33m] |");
		}
		*p_key = 0;
		break;
	case 'r': // Reply article
		return 1;
	case '=': // First topic article
		return 1;
	case '\\': // Last topic article
		return 1;
	case KEY_UP:
	case KEY_PGUP:
	case KEY_HOME:
		if (p_ctx->reach_begin)
		{
			if (section_topic_view_mode)
			{
				*p_key = KEY_PGUP;
			}
			else
			{
				*p_key = KEY_UP;
			}
			return 1;
		}
		break;
	case 'k':
		if (section_topic_view_mode)
		{
			*p_key = KEY_PGUP;
		}
		else
		{
			*p_key = KEY_UP;
		}
		return 1;
	case KEY_DOWN:
	case KEY_PGDN:
	case KEY_END:
		if (p_ctx->reach_end)
		{
			if (section_topic_view_mode)
			{
				*p_key = KEY_PGDN;
			}
			else
			{
				*p_key = KEY_DOWN;
			}
			return 1;
		}
		break;
	case 'j':
		if (section_topic_view_mode)
		{
			*p_key = KEY_PGDN;
		}
		else
		{
			*p_key = KEY_DOWN;
		}
		return 1;
	}

	return 0;
}

int section_list_display(const char *sname)
{
	static int display_nickname = 0;

	SECTION_LIST *p_section;
	char stitle[BBS_section_title_max_len + 1];
	char master_list[(BBS_username_max_len + 1) * 3 + 1];
	char page_info_str[LINE_BUFFER_LEN];
	ARTICLE *p_articles[BBS_article_limit_per_page];
	int article_count;
	int page_count;
	int ontop_start_offset;
	int page_id = 0;
	int selected_index = 0;
	ARTICLE_CACHE cache;
	int ret;
	int loop;
	int direction;
	ARTICLE article_new;
	int page_id_cur;

	p_section = section_list_find_by_name(sname);
	if (p_section == NULL)
	{
		log_error("Section %s not found\n", sname);
		return -1;
	}

	if ((ret = section_list_rd_lock(p_section)) < 0)
	{
		log_error("section_list_rd_lock(sid = 0) error\n");
		return -2;
	}

	strncpy(stitle, p_section->stitle, sizeof(stitle) - 1);
	stitle[sizeof(stitle) - 1] = '\0';
	strncpy(master_list, p_section->master_list, sizeof(master_list) - 1);
	master_list[sizeof(master_list) - 1] = '\0';

	if ((ret = section_list_rd_unlock(p_section)) < 0)
	{
		log_error("section_list_rd_unlock(sid = 0) error\n");
		return -2;
	}

	if (section_list_draw_screen(sname, stitle, master_list, display_nickname) < 0)
	{
		log_error("section_list_draw_screen() error\n");
		return -2;
	}

	ret = query_section_articles(p_section, page_id, p_articles, &article_count, &page_count, &ontop_start_offset);
	if (ret < 0)
	{
		log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
		return -3;
	}

	if (article_count == 0) // empty section
	{
		selected_index = 0;
	}

	while (!SYS_server_exit)
	{
		ret = section_list_draw_items(page_id, p_articles, article_count, display_nickname, ontop_start_offset);
		if (ret < 0)
		{
			log_error("section_list_draw_items(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
			return -4;
		}

		snprintf(page_info_str, sizeof(page_info_str),
				 "\033[33m[第\033[36m%d\033[33m/\033[36m%d\033[33m页]",
				 page_id + 1, MAX(page_count, 1));

		show_bottom(page_info_str);
		iflush();

		if (user_online_update(sname) < 0)
		{
			log_error("user_online_update(%s) error\n", sname);
		}

		ret = section_list_select(page_count, article_count, &page_id, &selected_index);
		switch (ret)
		{
		case EXIT_SECTION:
			return 0;
		case CHANGE_PAGE:
			ret = query_section_articles(p_section, page_id, p_articles, &article_count, &page_count, &ontop_start_offset);
			if (ret < 0)
			{
				log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
				return -3;
			}
			if (article_count == 0) // empty section
			{
				selected_index = 0;
			}
			else if (selected_index >= article_count)
			{
				selected_index = article_count - 1;
			}
			break;
		case VIEW_ARTICLE:
			do
			{
				loop = 0;

				if (article_cache_load(&cache, VAR_ARTICLE_CACHE_DIR, p_articles[selected_index]) < 0)
				{
					log_error("article_cache_load(aid=%d, cid=%d) error\n", p_articles[selected_index]->aid, p_articles[selected_index]->cid);
					break;
				}

				if (user_online_update("VIEW_ARTICLE") < 0)
				{
					log_error("user_online_update(VIEW_ARTICLE) error\n");
				}

				ret = display_data(cache.p_data, cache.line_total, cache.line_offsets, 0,
								   display_article_key_handler, DATA_READ_HELP);

				if (article_cache_unload(&cache) < 0)
				{
					log_error("article_cache_unload(aid=%d, cid=%d) error\n", p_articles[selected_index]->aid, p_articles[selected_index]->cid);
					break;
				}

				// Update article_view_log
				if (article_view_log_set_viewed(p_articles[selected_index]->aid, &BBS_article_view_log) < 0)
				{
					log_error("article_view_log_set_viewed(aid=%d) error\n", p_articles[selected_index]->aid);
				}

				switch (ret)
				{
				case KEY_UP:
					if (selected_index <= 0)
					{
						if (page_id > 0)
						{
							page_id--;
							selected_index = BBS_article_limit_per_page - 1;

							ret = query_section_articles(p_section, page_id, p_articles, &article_count, &page_count, &ontop_start_offset);
							if (ret < 0)
							{
								log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
								return -3;
							}

							if (article_count != BBS_article_limit_per_page) // page is not full
							{
								selected_index = MAX(0, article_count - 1);
							}
							else
							{
								loop = 1;
							}
						}
					}
					else
					{
						selected_index--;
						loop = 1;
					}
					break;
				case KEY_DOWN:
					if (selected_index + 1 >= article_count) // next page
					{
						if (page_id + 1 < page_count)
						{
							page_id++;
							selected_index = 0;

							ret = query_section_articles(p_section, page_id, p_articles, &article_count, &page_count, &ontop_start_offset);
							if (ret < 0)
							{
								log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
								return -3;
							}

							if (article_count == 0) // empty page
							{
								selected_index = 0;
							}
							else
							{
								loop = 1;
							}
						}
					}
					else
					{
						selected_index++;
						loop = 1;
					}
					break;
				case KEY_PGUP:
				case KEY_PGDN:
					direction = (ret == KEY_PGUP ? -1 : 1);
					ret = locate_article_in_section(p_section, p_articles[selected_index], direction, 1,
													&page_id, &selected_index, &article_count);
					if (ret < 0)
					{
						log_error("locate_article_in_section(sid=%d, aid=%d, direction=%d, step=1) error\n",
								  p_section->sid, p_articles[selected_index]->aid, direction);
						return -3;
					}
					else if (ret > 0) // found
					{
						ret = query_section_articles(p_section, page_id, p_articles, &article_count, &page_count, &ontop_start_offset);
						if (ret < 0)
						{
							log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
							return -3;
						}
						loop = 1;
					}
					break;
				case 'r': // Reply article
					if (user_online_update("REPLY_ARTICLE") < 0)
					{
						log_error("user_online_update(REPLY_ARTICLE) error\n");
					}

					if (article_reply(p_section, p_articles[selected_index], &article_new) < 0)
					{
						log_error("article_reply(aid=%d) error\n", p_articles[selected_index]->aid);
					}
					loop = 1;
					break;
				case '=':  // First topic article
				case '\\': // Last topic article
					page_id_cur = page_id;
					direction = (ret == '=' ? -1 : 1);
					ret = locate_article_in_section(p_section, p_articles[selected_index], direction, BBS_article_limit_per_section,
													&page_id, &selected_index, &article_count);
					if (ret < 0)
					{
						log_error("locate_article_in_section(sid=%d, aid=%d, direction=%d, step=%d) error\n",
								  p_section->sid, p_articles[selected_index]->aid, direction, BBS_article_limit_per_section);
						return -3;
					}
					else if (ret > 0) // found
					{
						if (page_id != page_id_cur) // page changed
						{
							ret = query_section_articles(p_section, page_id, p_articles, &article_count, &page_count, &ontop_start_offset);
							if (ret < 0)
							{
								log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
								return -3;
							}
						}
						loop = 1;
					}
					break;
				}
			} while (loop);

			// Update current topic
			section_topic_view_tid = (p_articles[selected_index]->tid == 0 ? p_articles[selected_index]->aid : p_articles[selected_index]->tid);

			if (section_list_draw_screen(sname, stitle, master_list, display_nickname) < 0)
			{
				log_error("section_list_draw_screen() error\n");
				return -2;
			}
			break;
		case CHANGE_NAME_DISPLAY:
			display_nickname = !display_nickname;
			if (section_list_draw_screen(sname, stitle, master_list, display_nickname) < 0)
			{
				log_error("section_list_draw_screen() error\n");
				return -2;
			}
			break;
		case POST_ARTICLE:
			if (user_online_update("POST_ARTICLE") < 0)
			{
				log_error("user_online_update(POST_ARTICLE) error\n");
			}

			if ((ret = article_post(p_section, &article_new)) < 0)
			{
				log_error("article_post(sid=%d) error\n", p_section->sid);
			}
			else if (ret > 0) // New article posted
			{
				ret = query_section_articles(p_section, page_id, p_articles, &article_count, &page_count, &ontop_start_offset);
				if (ret < 0)
				{
					log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
					return -3;
				}
			}
			if (section_list_draw_screen(sname, stitle, master_list, display_nickname) < 0)
			{
				log_error("section_list_draw_screen() error\n");
				return -2;
			}
			break;
		case EDIT_ARTICLE:
			if (!checkpriv(&BBS_priv, p_section->sid, S_POST) ||
				p_articles[selected_index]->uid != BBS_priv.uid)
			{
				break; // No permission
			}

			if (user_online_update("EDIT_ARTICLE") < 0)
			{
				log_error("user_online_update() error\n");
			}

			if (article_modify(p_section, p_articles[selected_index], &article_new) < 0)
			{
				log_error("article_modify(aid=%d) error\n", p_articles[selected_index]->aid);
			}
			if (section_list_draw_screen(sname, stitle, master_list, display_nickname) < 0)
			{
				log_error("section_list_draw_screen() error\n");
				return -2;
			}
			break;
		case DELETE_ARTICLE:
			if (!checkpriv(&BBS_priv, p_section->sid, S_POST) ||
				(!checkpriv(&BBS_priv, p_section->sid, S_MAN_S) && p_articles[selected_index]->uid != BBS_priv.uid))
			{
				break; // No permission
			}
			if ((ret = article_del(p_section, p_articles[selected_index])) < 0)
			{
				log_error("article_del(aid=%d) error\n", p_articles[selected_index]->aid);
			}
			else if (ret > 0) // Article deleted
			{
				ret = query_section_articles(p_section, page_id, p_articles, &article_count, &page_count, &ontop_start_offset);
				if (ret < 0)
				{
					log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
					return -3;
				}
			}
			if (section_list_draw_screen(sname, stitle, master_list, display_nickname) < 0)
			{
				log_error("section_list_draw_screen() error\n");
				return -2;
			}
			break;
		case QUERY_ARTICLE:
			if ((ret = display_article_meta(p_articles[selected_index]->aid)) < 0)
			{
				log_error("display_article_meta(aid=%d) error\n", p_articles[selected_index]->aid);
			}
			if (section_list_draw_screen(sname, stitle, master_list, display_nickname) < 0)
			{
				log_error("section_list_draw_screen() error\n");
				return -2;
			}
			break;
		case FIRST_TOPIC_ARTICLE:
		case LAST_TOPIC_ARTICLE:
			page_id_cur = page_id;
			direction = (ret == FIRST_TOPIC_ARTICLE ? -1 : 1);
			ret = locate_article_in_section(p_section, p_articles[selected_index], direction, BBS_article_limit_per_section,
											&page_id, &selected_index, &article_count);
			if (ret < 0)
			{
				log_error("locate_article_in_section(sid=%d, aid=%d, direction=%d, step=%d) error\n",
						  p_section->sid, p_articles[selected_index]->aid, direction, BBS_article_limit_per_section);
				return -3;
			}
			else if (ret > 0 && page_id != page_id_cur) // found and page changed
			{
				ret = query_section_articles(p_section, page_id, p_articles, &article_count, &page_count, &ontop_start_offset);
				if (ret < 0)
				{
					log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
					return -3;
				}
			}
			break;
		case SHOW_HELP:
			// Display help information
			display_file(DATA_READ_HELP, 1);
			if (section_list_draw_screen(sname, stitle, master_list, display_nickname) < 0)
			{
				log_error("section_list_draw_screen() error\n");
				return -2;
			}
			break;
		case VIEW_EX_DIR:
			if (section_list_ex_dir_display(p_section) < 0)
			{
				log_error("section_list_ex_dir_display(sid=%d) error\n", p_section->sid);
			}
			if (section_list_draw_screen(sname, stitle, master_list, display_nickname) < 0)
			{
				log_error("section_list_draw_screen() error\n");
				return -2;
			}
			break;
		default:
			log_error("Unknown command %d\n", ret);
		}
	}

	return 0;
}

int section_list_ex_dir_display(SECTION_LIST *p_section)
{
	MENU_SET ex_menu_set;
	int ch = 0;

	if (p_section == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_section->ex_menu_tm == 0) // N/A
	{
		moveto(2, 1);
		clrtoeol();
		prints("该版块精华区未开放");
		press_any_key();
		return 0;
	}

	if (get_section_ex_menu_set(p_section, &ex_menu_set) < 0)
	{
		log_error("get_section_ex_menu_set(sid=%d) error\n", p_section->sid);
		return -3;
	}
	if (get_menu_shm_readonly(&ex_menu_set) < 0)
	{
		log_error("get_menu_shm_readonly(sid=%d) error\n", p_section->sid);
		return -3;
	}

	clearscr();
	show_bottom("");

	if (display_menu(&ex_menu_set) == 0)
	{
		while (!SYS_server_exit)
		{
			iflush();
			ch = igetch(100);
			switch (ch)
			{
			case KEY_NULL: // broken pipe
				return 0;
			case KEY_TIMEOUT:
				if (time(NULL) - BBS_last_access_tm >= MAX_DELAY_TIME)
				{
					return 0;
				}
				continue;
			case CR:
				igetch_reset();
			default:
				switch (menu_control(&ex_menu_set, ch))
				{
				case EXITMENU:
					ch = EXITMENU;
					break;
				case REDRAW:
					clearscr();
					show_bottom("");
					display_menu(&ex_menu_set);
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

	detach_menu_shm(&ex_menu_set);

	return 0;
}
