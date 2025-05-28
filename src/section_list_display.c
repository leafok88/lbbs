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

#include "section_list_display.h"
#include "section_list_loader.h"
#include "common.h"
#include "io.h"
#include "screen.h"
#include "log.h"
#include "str_process.h"
#include <time.h>
#define _POSIX_C_SOURCE 200809L
#include <string.h>

enum select_cmd_t
{
	EXIT_SECTION = 0,
	VIEW_ARTICLE = 1,
	CHANGE_PAGE = 2,
	REFRESH_SCREEN = 3,
};

static int section_list_draw_items(SECTION_LIST *p_section, int page_id, ARTICLE *p_articles[], int article_count)
{
	char str_time[LINE_BUFFER_LEN];
	struct tm tm_sub;
	char title_f[BBS_article_title_max_len + 1];
	int title_f_len;
	int eol;
	int len;
	int i;

	clrline(4, 23);

	for (i = 0; i < article_count; i++)
	{
		localtime_r(&p_articles[i]->sub_dt, &tm_sub);
		strftime(str_time, sizeof(str_time), "%b %e", &tm_sub);
		strncpy(title_f, p_articles[i]->title, sizeof(title_f) - 1);
		title_f[sizeof(title_f) - 1] = '\0';
		len = split_line(title_f, (p_articles[i]->tid == 0 ? 40 : 37), &eol, &title_f_len);
		if (title_f[len] != '\0')
		{
			title_f[len] = '\0';
		}

		moveto(4 + i, 1);
		prints("  %7d %s%*s %s  %s%s",
			   p_articles[i]->aid,
			   p_articles[i]->username,
			   BBS_username_max_len - (int)strnlen(p_articles[i]->username, sizeof(p_articles[i]->username)),
			   "",
			   str_time,
			   (p_articles[i]->tid == 0 ? "● " : ""),
			   title_f);
	}

	return 0;
}

static int section_list_draw_screen(SECTION_LIST *p_section)
{
	char str_section_master[LINE_BUFFER_LEN] = "诚征版主中";
	char str_section_name[LINE_BUFFER_LEN];

	if (p_section->master_list[0] != '\0')
	{
		snprintf(str_section_master, sizeof(str_section_master), "版主：%s", p_section->master_list);
	}
	snprintf(str_section_name, sizeof(str_section_name), "讨论区 [%s]", p_section->sname);

	clearscr();
	show_top(str_section_master, p_section->stitle, str_section_name);
	moveto(2, 0);
	prints("离开[\033[1;32m←\033[0;37m,\033[1;32mESC\033[0;37m] 选择[\033[1;32m↑\033[0;37m,\033[1;32m↓\033[0;37m] 阅读[\033[1;32m→\033[0;37m,\033[1;32mENTER\033[0;37m]\033[m");
	moveto(3, 0);
	prints("\033[44;37m  \033[1;37m 编 号  发 布 者     日  期  文 章 标 题                                      \033[m");
	show_bottom("");

	return 0;
}

static enum select_cmd_t section_list_select(int total_page, int item_count, int *p_page_id, int *p_selected_index)
{
	int old_page_id = *p_page_id;
	int old_selected_index = *p_selected_index;
	int ch;

	BBS_last_access_tm = time(0);

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
		case KEY_NULL: // broken pipe
		case KEY_ESC:
		case KEY_LEFT:
			return EXIT_SECTION; // exit section
		case KEY_TIMEOUT:
			if (time(0) - BBS_last_access_tm >= MAX_DELAY_TIME)
			{
				return EXIT_SECTION; // exit section
			}
			continue;
		case CR:
			igetch_reset();
		case KEY_RIGHT:
			if (item_count > 0)
			{
				return VIEW_ARTICLE;
			}
			break;
		case KEY_HOME:
			*p_page_id = 0;
		case KEY_PGUP:
			*p_selected_index = 0;
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
		case KEY_END:
			if (total_page > 0)
			{
				*p_page_id = total_page - 1;
			}
		case KEY_PGDN:
			if (item_count > 0)
			{
				*p_selected_index = item_count - 1;
			}
		case KEY_DOWN:
			if (*p_selected_index + 1 >= item_count) // next page
			{
				if (*p_page_id + 1 < total_page)
				{
					(*p_page_id)++;
					*p_selected_index = 0;
				}
			}
			else
			{
				(*p_selected_index)++;
			}
			break;
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

		BBS_last_access_tm = time(0);
	}

	return EXIT_SECTION;
}

int section_list_display(const char *sname)
{
	SECTION_LIST *p_section;
	ARTICLE *p_articles[BBS_article_limit_per_page];
	int article_count;
	int page_id = 0;
	int selected_index = 0;
	int ret;

	p_section = section_list_find_by_name(sname);
	if (p_section == NULL)
	{
		log_error("Section %s not found\n", sname);
		return -1;
	}

	if (section_list_draw_screen(p_section) < 0)
	{
		log_error("section_list_draw_screen() error\n");
		return -2;
	}

	if (p_section->visible_article_count > 0)
	{
		ret = query_section_articles(p_section, page_id, p_articles, &article_count);
		if (ret < 0)
		{
			log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
			return -3;
		}
	}
	else // empty section
	{
		article_count = 0;
		selected_index = 0;
	}

	while (!SYS_server_exit)
	{
		ret = section_list_draw_items(p_section, page_id, p_articles, article_count);
		if (ret < 0)
		{
			log_error("section_list_draw_items(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
			return -4;
		}
		iflush();

		ret = section_list_select(p_section->page_count, article_count, &page_id, &selected_index);
		switch (ret)
		{
		case EXIT_SECTION:
			return 0;
		case CHANGE_PAGE:
			if (p_section->visible_article_count > 0)
			{
				ret = query_section_articles(p_section, page_id, p_articles, &article_count);
				if (ret < 0)
				{
					log_error("query_section_articles(sid=%d, page_id=%d) error\n", p_section->sid, page_id);
					return -3;
				}
			}
			else // empty section
			{
				article_count = 0;
				selected_index = 0;
			}
			if (selected_index >= article_count)
			{
				selected_index = article_count - 1;
			}
			break;
		case VIEW_ARTICLE:
			log_std("Debug: article %d selected\n", p_articles[selected_index]->aid);
		case REFRESH_SCREEN:
			if (section_list_draw_screen(p_section) < 0)
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
