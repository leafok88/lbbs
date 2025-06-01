/***************************************************************************
						  bbs_cmd.c  -  description
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
	{"RELOADCONF", reload_bbs_conf},
	{"SHUTDOWN", shutdown_bbs},
	{"M_FAVOUR", favour_section_filter}
};

static const int bbs_cmd_count = 8;

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
