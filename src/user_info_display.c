/***************************************************************************
					 user_list_display.c  -  description
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

#include "lml.h"
#include "log.h"
#include "screen.h"
#include "str_process.h"
#include "user_list.h"
#include "user_info_display.h"
#include <string.h>
#include <sys/param.h>

#define BBS_max_sessions_per_user 10

static int display_user_intro_key_handler(int *p_key, DISPLAY_CTX *p_ctx)
{
	return 0;
}

int user_info_display(USER_INFO *p_user_info)
{
	USER_ONLINE_INFO sessions[BBS_max_sessions_per_user];
	int session_cnt = BBS_max_sessions_per_user;
	char action_str[LINE_BUFFER_LEN];
	char *p;
	int ret;
	int i;

	action_str[0] = '\0';

	ret = query_user_online_info_by_uid(p_user_info->uid, sessions, &session_cnt, 0);
	if (ret < 0)
	{
		log_error("query_user_online_info_by_uid(uid=%d, cnt=%d) error: %d\n",
				  p_user_info->uid, session_cnt, ret);
	}
	else
	{
		p = action_str;
		for (i = 0; i < session_cnt; i++)
		{
			if (p + strlen(sessions[i].current_action_title) + 3 >= action_str + sizeof(action_str)) // buffer overflow
			{
				log_error("action_str of user(uid=%d) truncated at i=%d\n", p_user_info->uid, i);
				break;
			}
			*p++ = '[';
			p = stpncpy(p, sessions[i].current_action_title, strlen(sessions[i].current_action_title));
			*p++ = ']';
			*p++ = ' ';
		}
		*p = '\0';
	}

	char intro_f[BBS_user_intro_max_len];
	char profile_f[BUFSIZ];
	long line_offsets[BBS_user_intro_max_line + 1];
	long lines;

	lml_render(p_user_info->intro, intro_f, sizeof(intro_f), 0);

	snprintf(profile_f, sizeof(profile_f),
			 "已选中用户 [%s]\n发帖数：%d\n%s\n%s\n",
			 p_user_info->username,
			 get_user_article_cnt(p_user_info->uid),
			 action_str,
			 intro_f);

	lines = split_data_lines(profile_f, SCREEN_COLS, line_offsets, MIN(SCREEN_ROWS - 1, BBS_user_intro_max_line + 4), 1, NULL);

	clearscr();
	display_data(profile_f, lines, line_offsets, 2, display_user_intro_key_handler, DATA_READ_HELP);
	press_any_key();

	return 0;
}
