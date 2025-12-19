/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * user_list_display
 *   - user interactive list of (online) users
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "io.h"
#include "log.h"
#include "login.h"
#include "screen.h"
#include "str_process.h"
#include "user_list.h"
#include "user_priv.h"
#include "user_info_display.h"
#include "user_list_display.h"
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/param.h>

enum select_cmd_t
{
	EXIT_LIST = 0,
	VIEW_USER,
	CHANGE_PAGE,
	REFRESH_LIST,
	SHOW_HELP,
	SEARCH_USER,
};

static int user_list_draw_screen(int online_user)
{
	clearscr();
	show_top((online_user ? "[线上使用者]" : "[已注册用户]"), BBS_name, "");
	moveto(2, 0);
	prints("返回[\033[1;32m←\033[0;37m,\033[1;32mESC\033[0;37m] 选择[\033[1;32m↑\033[0;37m,\033[1;32m↓\033[0;37m] "
		   "查看[\033[1;32m→\033[0;37m,\033[1;32mENTER\033[0;37m] 查找[\033[1;32ms\033[0;37m] "
		   "帮助[\033[1;32mh\033[0;37m]\033[m");
	moveto(3, 0);

	if (online_user)
	{
		prints("\033[44;37m \033[1;37m 编  号  用户名       昵称                 在线时长 空闲   最后活动            \033[m");
	}
	else
	{
		prints("\033[44;37m \033[1;37m 编  号  用户名       昵称                 上次登陆距今                        \033[m");
	}

	return 0;
}

static int user_list_draw_items(int page_id, USER_INFO *p_users, int user_count)
{
	char str_time_login[LINE_BUFFER_LEN];
	time_t tm_now;
	time_t tm_duration;
	struct tm *p_tm;
	int i;

	clrline(4, 23);

	time(&tm_now);

	for (i = 0; i < user_count; i++)
	{
		tm_duration = tm_now - p_users[i].last_login_dt;
		p_tm = gmtime(&tm_duration);
		if (p_tm == NULL)
		{
			log_error("Invalid time duration");
			str_time_login[0] = '\0';
		}
		else if (p_tm->tm_year > 70)
		{
			snprintf(str_time_login, sizeof(str_time_login),
					 "%d年%d天", p_tm->tm_year - 70, p_tm->tm_yday);
		}
		else if (p_tm->tm_yday > 0)
		{
			snprintf(str_time_login, sizeof(str_time_login),
					 "%d天", p_tm->tm_yday);
		}
		else if (p_tm->tm_hour > 0)
		{
			snprintf(str_time_login, sizeof(str_time_login),
					 "%d:%.2d", p_tm->tm_hour, p_tm->tm_min);
		}
		else
		{
			snprintf(str_time_login, sizeof(str_time_login),
					 "%d\'%.2d\"", p_tm->tm_min, p_tm->tm_sec);
		}

		moveto(4 + i, 1);

		prints("  %6d  %s%*s %s%*s %s",
			   p_users[i].id + 1,
			   p_users[i].username,
			   BBS_username_max_len - str_length(p_users[i].username, 1),
			   "",
			   p_users[i].nickname,
			   BBS_nickname_max_len / 2 - str_length(p_users[i].nickname, 1),
			   "",
			   str_time_login);
	}

	return 0;
}

static int user_online_list_draw_items(int page_id, USER_ONLINE_INFO *p_users, int user_count)
{
	char str_time_login[LINE_BUFFER_LEN];
	char str_time_idle[LINE_BUFFER_LEN];
	const char *p_action_title;
	time_t tm_now;
	time_t tm_duration;
	struct tm *p_tm;
	int i;

	clrline(4, 23);

	time(&tm_now);

	for (i = 0; i < user_count; i++)
	{
		tm_duration = tm_now - p_users[i].login_tm;
		p_tm = gmtime(&tm_duration);
		if (p_tm == NULL)
		{
			log_error("Invalid time duration");
			str_time_login[0] = '\0';
		}
		else if (p_tm->tm_yday > 0)
		{
			snprintf(str_time_login, sizeof(str_time_login),
					 "%dd%dh", p_tm->tm_yday, p_tm->tm_hour);
		}
		else if (p_tm->tm_hour > 0)
		{
			snprintf(str_time_login, sizeof(str_time_login),
					 "%d:%.2d", p_tm->tm_hour, p_tm->tm_min);
		}
		else
		{
			snprintf(str_time_login, sizeof(str_time_login),
					 "%d\'%.2d\"", p_tm->tm_min, p_tm->tm_sec);
		}

		tm_duration = tm_now - p_users[i].last_tm;
		p_tm = gmtime(&tm_duration);
		if (p_tm == NULL)
		{
			log_error("Invalid time duration");
			str_time_idle[0] = '\0';
		}
		else if (p_tm->tm_min > 0)
		{
			snprintf(str_time_idle, sizeof(str_time_idle),
					 "%d\'%d\"", p_tm->tm_min, p_tm->tm_sec);
		}
		else
		{
			snprintf(str_time_idle, sizeof(str_time_idle),
					 "%d\"", p_tm->tm_sec);
		}

		p_action_title = (p_users[i].current_action_title != NULL
							  ? p_users[i].current_action_title
							  : p_users[i].current_action);

		moveto(4 + i, 1);

		prints("  %6d  %s%*s %s%*s %s%*s %s%*s %s",
			   p_users[i].id + 1,
			   p_users[i].user_info.username,
			   BBS_username_max_len - str_length(p_users[i].user_info.username, 1),
			   "",
			   p_users[i].user_info.nickname,
			   BBS_nickname_max_len / 2 - str_length(p_users[i].user_info.nickname, 1),
			   "",
			   str_time_login,
			   8 - str_length(str_time_login, 1),
			   "",
			   str_time_idle,
			   6 - str_length(str_time_idle, 1),
			   "",
			   p_action_title);
	}

	return 0;
}

static enum select_cmd_t user_list_select(int total_page, int item_count, int *p_page_id, int *p_selected_index)
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

		if (ch != KEY_NULL && ch != KEY_TIMEOUT)
		{
			BBS_last_access_tm = time(NULL);

			// Refresh current action
			if (user_online_update(NULL) < 0)
			{
				log_error("user_online_update(NULL) error");
			}
		}

		switch (ch)
		{
		case KEY_NULL: // broken pipe
			log_debug("KEY_NULL");
		case KEY_ESC:
		case KEY_LEFT:
			return EXIT_LIST; // exit list
		case KEY_TIMEOUT:
			if (time(NULL) - BBS_last_access_tm >= BBS_max_user_idle_time)
			{
				log_debug("User input timeout");
				return EXIT_LIST; // exit list
			}
			continue;
		case CR:
		case KEY_RIGHT:
			if (item_count > 0)
			{
				return VIEW_USER;
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
					*p_selected_index = BBS_user_limit_per_page - 1;
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
		case 's':
			return SEARCH_USER;
		case KEY_F5:
			return REFRESH_LIST;
		case 'h':
			return SHOW_HELP;
		default:
			break;
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
	}

	return EXIT_LIST;
}

int user_list_display(int online_user)
{
	char page_info_str[LINE_BUFFER_LEN];
	USER_INFO users[BBS_user_limit_per_page];
	USER_ONLINE_INFO online_users[BBS_user_limit_per_page];
	int user_count = 0;
	int page_count = 0;
	int page_id = 0;
	int selected_index = 0;
	int ret = 0;

	user_list_draw_screen(online_user);

	if (online_user)
	{
		ret = query_user_online_list(page_id, online_users, &user_count, &page_count);
		if (ret < 0)
		{
			log_error("query_user_online_list(page_id=%d) error", page_id);
			return -2;
		}
	}
	else
	{
		ret = query_user_list(page_id, users, &user_count, &page_count);
		if (ret < 0)
		{
			log_error("query_user_list(page_id=%d) error", page_id);
			return -2;
		}
	}

	if (user_count == 0) // empty list
	{
		selected_index = 0;
	}

	while (!SYS_server_exit)
	{
		if (online_user)
		{
			ret = user_online_list_draw_items(page_id, online_users, user_count);
			if (ret < 0)
			{
				log_error("user_online_list_draw_items(page_id=%d) error", page_id);
				return -3;
			}
		}
		else
		{
			ret = user_list_draw_items(page_id, users, user_count);
			if (ret < 0)
			{
				log_error("user_list_draw_items(page_id=%d) error", page_id);
				return -3;
			}
		}

		snprintf(page_info_str, sizeof(page_info_str),
				 "\033[33m[第\033[36m%d\033[33m/\033[36m%d\033[33m页]",
				 page_id + 1, MAX(page_count, 1));

		show_bottom(page_info_str);
		iflush();

		if (user_online_update(online_user ? "USER_ONLINE" : "USER_LIST") < 0)
		{
			log_error("user_online_update(%s) error",
					  (online_user ? "USER_ONLINE" : "USER_LIST"));
		}

		ret = user_list_select(page_count, user_count, &page_id, &selected_index);
		switch (ret)
		{
		case EXIT_LIST:
			return 0;
		case REFRESH_LIST:
		case CHANGE_PAGE:
			if (online_user)
			{
				ret = query_user_online_list(page_id, online_users, &user_count, &page_count);
				if (ret < 0)
				{
					log_error("query_user_online_list(page_id=%d) error", page_id);
					return -2;
				}
			}
			else
			{
				ret = query_user_list(page_id, users, &user_count, &page_count);
				if (ret < 0)
				{
					log_error("query_user_list(page_id=%d) error", page_id);
					return -2;
				}
			}

			if (user_count == 0) // empty list
			{
				selected_index = 0;
			}
			else if (selected_index >= user_count)
			{
				selected_index = user_count - 1;
			}
			break;
		case VIEW_USER:
			user_info_display(online_user ? &(online_users[selected_index].user_info) : &(users[selected_index]));
			user_list_draw_screen(online_user);
			break;
		case SEARCH_USER:
			user_list_search();
			user_list_draw_screen(online_user);
			break;
		case SHOW_HELP:
			// Display help information
			display_file(DATA_READ_HELP, 1);
			user_list_draw_screen(online_user);
			break;
		default:
			log_error("Unknown command %d", ret);
		}
	}

	return 0;
}

int user_list_search(void)
{
	const int users_per_line = 5;
	const int max_user_lines = 20;
	const int max_user_cnt = users_per_line * max_user_lines + 1;

	char username[BBS_username_max_len + 1];
	int32_t uid_list[max_user_cnt];
	char username_list[max_user_cnt][BBS_username_max_len + 1];
	int ret;
	int i;
	USER_INFO user_info;
	char user_intro[BBS_user_intro_max_len + 1];
	int ok;
	int ch;

	username[0] = '\0';

	clearscr();

	while (!SYS_server_exit)
	{
		clrline(3, SCREEN_ROWS);
		get_data(2, 1, "查找谁: ", username, sizeof(username), BBS_username_max_len);

		if (username[0] == '\0')
		{
			return 0;
		}

		// Verify format
		for (i = 0, ok = 1; ok && username[i] != '\0'; i++)
		{
			if (!(isalpha((int)username[i]) || (i > 0 && (isdigit((int)username[i]) || username[i] == '_'))))
			{
				ok = 0;
			}
		}
		if (ok && i > BBS_username_max_len)
		{
			ok = 0;
		}
		if (!ok)
		{
			moveto(3, 1);
			clrtoeol();
			prints("用户名格式非法");
			continue;
		}

		clrline(3, SCREEN_ROWS);

		ret = query_user_info_by_username(username, max_user_cnt, uid_list, username_list);

		if (ret < 0)
		{
			log_error("query_user_info_by_username(%s) error", username);
			return -1;
		}
		else if (ret > 1)
		{
			for (i = 0; i < MIN(ret, users_per_line * max_user_lines); i++)
			{
				moveto(4 + i / users_per_line, 3 + i % users_per_line * (BBS_username_max_len + 3));
				prints("%s", username_list[i]);
			}
			moveto(SCREEN_ROWS, 1);
			if (ret > users_per_line * max_user_lines)
			{
				prints("还有更多...");
			}

			moveto(3, 1);
			prints("存在多个匹配的用户，按\033[1;33mEnter\033[m精确查找");
			iflush();

			ch = igetch_t(BBS_max_user_idle_time);
			switch (ch)
			{
			case KEY_NULL:
			case KEY_TIMEOUT:
				return -1;
			case KEY_ESC:
				return 0;
			case CR:
				ret = (strcasecmp(username_list[0], username) == 0 ? 1 : 0);
				break;
			default:
				i = (int)strnlen(username, sizeof(username) - 1);
				if (i + 1 <= BBS_username_max_len && (isalnum(ch) || ch == '_'))
				{
					username[i] = (char)ch;
					username[i + 1] = '\0';
				}
				continue;
			}
		}

		clrline(3, SCREEN_ROWS);
		if (ret == 0)
		{
			moveto(3, 1);
			prints("没有找到符合条件的用户");
			press_any_key();
			return 0;
		}
		else // ret == 1
		{
			if (query_user_info_by_uid(uid_list[0], &user_info, user_intro, sizeof(user_intro)) <= 0)
			{
				log_error("query_user_info_by_uid(uid=%d) error", uid_list[0]);
				return -2;
			}
			else if (user_info_display(&user_info) < 0)
			{
				log_error("user_info_display(uid=%d) error", uid_list[0]);
				return -3;
			}
			return 1;
		}
	}

	return 0;
}
