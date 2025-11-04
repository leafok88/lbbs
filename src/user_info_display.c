/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * user_list_display
 *   - display user information
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#include "bbs.h"
#include "ip_mask.h"
#include "lml.h"
#include "log.h"
#include "screen.h"
#include "str_process.h"
#include "user_list.h"
#include "user_info_display.h"
#include "user_priv.h"
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/param.h>

#define BBS_max_sessions_per_user 10
#define LAST_LOGIN_DT_MAX_LEN 50

static int display_user_info_key_handler(int *p_key, DISPLAY_CTX *p_ctx)
{
	return 0;
}

int user_info_display(USER_INFO *p_user_info)
{
	USER_ONLINE_INFO sessions[BBS_max_sessions_per_user];
	int session_cnt = BBS_max_sessions_per_user;
	int article_cnt;
	const char *astro_name;
	char astro_str[LINE_BUFFER_LEN];
	struct tm tm_last_login;
	char str_last_login_dt[LAST_LOGIN_DT_MAX_LEN + 1];
	struct tm tm_last_logout;
	char str_last_logout_dt[LAST_LOGIN_DT_MAX_LEN + 1];
	char login_ip[IP_ADDR_LEN];
	int ip_mask_level;
	const char *p_action_title;
	char action_str[LINE_BUFFER_LEN];
	int life;
	int user_level;
	const char *user_level_name;
	char intro_f[BBS_user_intro_max_len + 1];
	int intro_len;
	char user_info_f[BUFSIZ];
	long line_offsets[BBS_user_intro_max_line + 1];
	long lines;
	char *p;
	const char *q;
	int ret;
	int i;

	if (p_user_info == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	article_cnt = get_user_article_cnt(p_user_info->uid);

	astro_name = get_astro_name(p_user_info->birthday);
	if (p_user_info->gender_pub && toupper(p_user_info->gender) == 'M')
	{
		snprintf(astro_str, sizeof(astro_str),
				 "\033[1;36m%s座\033[m",
				 astro_name);
	}
	else if (p_user_info->gender_pub && toupper(p_user_info->gender) == 'F')
	{
		snprintf(astro_str, sizeof(astro_str),
				 "\033[1;35m%s座\033[m",
				 astro_name);
	}
	else
	{
		snprintf(astro_str, sizeof(astro_str),
				 "\033[1;33m%s座\033[m",
				 astro_name);
	}

	localtime_r(&(p_user_info->last_login_dt), &tm_last_login);
	strftime(str_last_login_dt, sizeof(str_last_login_dt), "%F %H:%M", &tm_last_login);
	if (p_user_info->last_logout_dt <= p_user_info->last_login_dt)
	{
		strncpy(str_last_logout_dt, str_last_login_dt, sizeof(str_last_logout_dt) - 1);
		str_last_logout_dt[sizeof(str_last_logout_dt) - 1] = '\0';
	}
	else
	{
		localtime_r(&(p_user_info->last_logout_dt), &tm_last_logout);
		strftime(str_last_logout_dt, sizeof(str_last_logout_dt), "%F %H:%M", &tm_last_logout);
	}

	action_str[0] = '\0';

	ret = query_user_online_info_by_uid(p_user_info->uid, sessions, &session_cnt, 0);
	if (ret < 0)
	{
		log_error("query_user_online_info_by_uid(uid=%d, cnt=%d) error: %d\n",
				  p_user_info->uid, session_cnt, ret);
		session_cnt = 0;
	}

	p = action_str;
	for (i = 0; i < session_cnt; i++)
	{
		p_action_title = (sessions[i].current_action_title != NULL
							  ? sessions[i].current_action_title
							  : sessions[i].current_action);

		if (p + strlen(p_action_title) + 4 >= action_str + sizeof(action_str)) // buffer overflow
		{
			log_error("action_str of user(uid=%d) truncated at i=%d\n", p_user_info->uid, i);
			break;
		}
		*p++ = '[';
		for (q = p_action_title; *q != '\0';)
		{
			*p++ = *q++;
		}
		*p++ = ']';
		*p++ = ' ';
	}
	*p++ = '\n';
	*p = '\0';

	if (session_cnt > 0)
	{
		strncpy(login_ip, sessions[0].ip, sizeof(login_ip) - 1);
		login_ip[sizeof(login_ip) - 1] = '\0';

		ip_mask_level = checklevel(&BBS_priv, P_ADMIN_M | P_ADMIN_S);
		ip_mask(login_ip, (ip_mask_level ? 1 : 2), '*');
	}
	else
	{
		login_ip[0] = '\0';
	}

	if (p_user_info->life == 333 || p_user_info->life == 365 || p_user_info->life == 666 || p_user_info->life == 999) // Immortal
	{
		life = p_user_info->life;
	}
	else
	{
		life = p_user_info->life - (int)((time(NULL) - p_user_info->last_login_dt) / 86400 + 1);
	}

	user_level = get_user_level(p_user_info->exp);
	user_level_name = get_user_level_name(user_level);

	intro_len = lml_render(p_user_info->intro, intro_f, sizeof(intro_f), SCREEN_COLS, 0);

	snprintf(user_info_f, sizeof(user_info_f),
			 "\n%s (%s) 上站 [%d] 发文 [%d]\n"
			 "上次在 [%s] 从 [%s] 访问本站 经验值 [%d]\n"
			 "离线于 [%s] 生命 [%d] 等级 [%s(%d)] 星座 [%s]\n"
			 "%s\033[1m%s\033[m"
			 "%s\n%s\n",
			 p_user_info->username, p_user_info->nickname, p_user_info->visit_count, article_cnt,
			 str_last_login_dt, (session_cnt > 0 ? login_ip : "未知"), p_user_info->exp,
			 (session_cnt > 0 ? "在线或因断线不详" : str_last_logout_dt), life, user_level_name, user_level + 1, astro_str,
			 (session_cnt > 0 ? "目前在线，状态如下：\n" : ""), (session_cnt > 0 ? action_str : ""),
			 (intro_len > 0 ? "\033[0;36m个人说明档如下：\033[m" : "\033[0;36m没有个人说明档\033[m"),
			 intro_f);

	lines = split_data_lines(user_info_f, SCREEN_COLS + 1, line_offsets, MIN(SCREEN_ROWS - 1, BBS_user_intro_max_line + 8), 1, NULL);

	clearscr();
	display_data(user_info_f, lines, line_offsets, 1, display_user_info_key_handler, DATA_READ_HELP);

	return 0;
}
