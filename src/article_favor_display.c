/***************************************************************************
                    article_favor_display.c  -  description
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

#include "article_favor_display.h"
#include "io.h"
#include "log.h"
#include "login.h"
#include "screen.h"
#include "str_process.h"
#include "section_list.h"
#include "section_list_display.h"
#include <string.h>
#include <time.h>
#include <sys/param.h>

enum select_cmd_t
{
    EXIT_LIST = 0,
    LOCATE_ARTICLE,
    CHANGE_PAGE,
    SHOW_HELP,
    SWITCH_NAME,
};

static int article_favor_draw_screen(int display_sname)
{
    clearscr();
    show_top("[主题收藏]", BBS_name, "");
    moveto(2, 0);
    prints("返回[\033[1;32m←\033[0;37m,\033[1;32mESC\033[0;37m] 选择[\033[1;32m↑\033[0;37m,\033[1;32m↓\033[0;37m] "
           "阅读[\033[1;32m→\033[0;37m,\033[1;32mENTER\033[0;37m] %s[\033[1;32mn\033[0;37m] "
           "帮助[\033[1;32mh\033[0;37m]\033[m",
           (display_sname ? "用户名" : "版块名称"));
    moveto(3, 0);
    if (display_sname)
    {
        prints("\033[44;37m  \033[1;37m 编  号   版块名称             日  期  文章标题                               \033[m");
    }
    else
    {
        prints("\033[44;37m  \033[1;37m 编  号   发 布 者     日  期  文章标题                                       \033[m");
    }

    return 0;
}

static int article_favor_draw_items(int page_id, ARTICLE *p_articles[], char p_snames[][BBS_section_name_max_len + 1],
                                    int article_count, int display_sname)
{
    char str_time[LINE_BUFFER_LEN];
    struct tm tm_sub;
    char title_f[BBS_article_title_max_len + 1];
    int title_f_len;
    int eol;
    int len;
    int i;
    time_t tm_now;

    time(&tm_now);

    clrline(4, 23);

    for (i = 0; i < article_count; i++)
    {
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

        len = split_line(title_f, 59 - (display_sname ? BBS_section_name_max_len : BBS_username_max_len), &eol, &title_f_len, 1);
        if (title_f[len] != '\0')
        {
            title_f[len] = '\0';
        }

        moveto(4 + i, 1);

        prints("  %7d   %s%*s %s",
               p_articles[i]->aid,
               (display_sname ? p_snames[i] : p_articles[i]->username),
               (display_sname
                    ? BBS_section_name_max_len - str_length(p_snames[i], 1)
                    : BBS_username_max_len - str_length(p_articles[i]->username, 1)),
               "",
               str_time,
               title_f);
    }

    return 0;
}

static enum select_cmd_t article_favor_select(int total_page, int item_count, int *p_page_id, int *p_selected_index)
{
    int old_page_id = *p_page_id;
    int old_selected_index = *p_selected_index;
    int ch;

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
        case KEY_NULL:        // broken pipe
            return EXIT_LIST; // exit list
        case KEY_TIMEOUT:
            if (time(NULL) - BBS_last_access_tm >= MAX_DELAY_TIME)
            {
                return EXIT_LIST; // exit list
            }
            continue;
        case 'n':
            BBS_last_access_tm = time(NULL);
            return SWITCH_NAME;
        case CR:
            igetch_reset();
        case KEY_RIGHT:
            if (item_count > 0)
            {
                BBS_last_access_tm = time(NULL);
                return LOCATE_ARTICLE;
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
                else if (ch == KEY_UP || ch == 'k') // Rotate to the tail of list
                {
                    if (total_page > 0)
                    {
                        *p_page_id = total_page - 1;
                    }
                    if (item_count > 0)
                    {
                        *p_selected_index = item_count - 1;
                    }
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
                else if (ch == KEY_DOWN || ch == 'j') // Rotate to the head of list
                {
                    *p_page_id = 0;
                    *p_selected_index = 0;
                }
            }
            else
            {
                (*p_selected_index)++;
            }
            break;
        case 'h':
            return SHOW_HELP;
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
    }

    return EXIT_LIST;
}

int article_favor_display(ARTICLE_FAVOR *p_favor)
{
    static int display_sname = 0;

    char page_info_str[LINE_BUFFER_LEN];
    char snames[BBS_article_limit_per_page][BBS_section_name_max_len + 1];
    ARTICLE *p_articles[BBS_article_limit_per_page];
    int article_count;
    int page_count;
    int page_id = 0;
    int selected_index = 0;
    int ret;

    if (p_favor == NULL)
    {
        log_error("NULL pointer error\n");
        return -1;
    }

    article_favor_draw_screen(display_sname);

    ret = query_favor_articles(p_favor, page_id, p_articles, snames, &article_count, &page_count);
    if (ret < 0)
    {
        log_error("query_favor_articles(page_id=%d) error\n", page_id);
        return -2;
    }

    if (article_count == 0) // empty list
    {
        selected_index = 0;
    }

    while (!SYS_server_exit)
    {
        ret = article_favor_draw_items(page_id, p_articles, snames, article_count, display_sname);
        if (ret < 0)
        {
            log_error("article_favor_draw_items(page_id=%d) error\n", page_id);
            return -3;
        }

        snprintf(page_info_str, sizeof(page_info_str),
                 "\033[33m[第\033[36m%d\033[33m/\033[36m%d\033[33m页]",
                 page_id + 1, MAX(page_count, 1));

        show_bottom(page_info_str);
        iflush();

        if (user_online_update("ARTICLE_FAVOR") < 0)
        {
            log_error("user_online_update(ARTICLE_FAVOR) error\n");
        }

        ret = article_favor_select(page_count, article_count, &page_id, &selected_index);
        switch (ret)
        {
        case EXIT_LIST:
            return 0;
        case CHANGE_PAGE:
            ret = query_favor_articles(p_favor, page_id, p_articles, snames, &article_count, &page_count);
            if (ret < 0)
            {
                log_error("query_favor_articles(page_id=%d) error\n", page_id);
                return -2;
            }

            if (article_count == 0) // empty list
            {
                selected_index = 0;
            }
            else if (selected_index >= article_count)
            {
                selected_index = article_count - 1;
            }
            break;
        case LOCATE_ARTICLE:
            if (section_list_display(snames[selected_index], p_articles[selected_index]->aid) < 0)
            {
                log_error("section_list_display(sname=%s, aid=%d) error\n",
                          snames[selected_index], p_articles[selected_index]->aid);
            }
            break;
        case SWITCH_NAME:
            display_sname = !display_sname;
            article_favor_draw_screen(display_sname);
            break;
        case SHOW_HELP:
            // Display help information
            display_file(DATA_READ_HELP, 1);
            article_favor_draw_screen(display_sname);
            break;
        default:
            log_error("Unknown command %d\n", ret);
        }
    }

    return 0;
}
