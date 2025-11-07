/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bbs_cmd
 *   - manager of menu command handler
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#include "bbs_cmd.h"
#include "menu_proc.h"
#include "trie_dict.h"
#include <string.h>

static const BBS_CMD bbs_cmd_list[] = {
	{"LIST_SECTION", list_section},
	{"RunMBEM", exec_mbem},
	{"EXITBBS", exit_bbs},
	{"LICENSE", license},
	{"COPYRIGHT", copyright},
	{"VERSION", version},
	{"RELOADCONF", reload_bbs_conf},
	{"SHUTDOWN", shutdown_bbs},
	{"M_FAVOR_SECTION", favor_section_filter},
	{"VIEW_EX_ARTICLE", view_ex_article},
	{"LIST_EX_SECTION", list_ex_section},
	{"TOP10", show_top10_menu},
	{"LOCATE_ARTICLE", locate_article},
	{"FAVOR_TOPIC", favor_topic},
	{"LIST_USER", list_user},
	{"LIST_ONLINE_USER", list_online_user},
	{"EDIT_INTRO", edit_intro},
	{"EDIT_SIGN", edit_sign}
};

static const int bbs_cmd_count = sizeof(bbs_cmd_list) / sizeof(BBS_CMD);

static TRIE_NODE *p_bbs_cmd_dict;

int load_cmd()
{
	if (p_bbs_cmd_dict != NULL)
	{
		return -1;
	}

	p_bbs_cmd_dict = trie_dict_create();

	if (p_bbs_cmd_dict == NULL)
	{
		return -2;
	}

	for (int i = 0; i < bbs_cmd_count; i++)
	{
		if (trie_dict_set(p_bbs_cmd_dict, bbs_cmd_list[i].cmd, (int64_t)i) != 1)
		{
			return -3;
		}
	}

	return 0;
}

void unload_cmd()
{
	trie_dict_destroy(p_bbs_cmd_dict);
	p_bbs_cmd_dict = NULL;
}

bbs_cmd_handler get_cmd_handler(const char *cmd)
{
	int64_t i;

	if (p_bbs_cmd_dict == NULL)
	{
		return NULL;
	}

	if (trie_dict_get(p_bbs_cmd_dict, cmd, &i) != 1)
	{
		return NULL;
	}

	return (bbs_cmd_list[i].handler);
}
