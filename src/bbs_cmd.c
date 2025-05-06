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
#include <string.h>

BBS_CMD bbs_cmd_list[MAX_CMD_ID] = {
	{"RunMBEM", exec_mbem},
	{"EXITBBS", exitbbs},
	{"LICENSE", license},
	{"COPYRIGHT", copyright},
	{"RELOADMENU", reloadbbsmenu},
	{"SHUTDOWN", shutdownbbs}};

int exec_cmd(const char *cmd, const char *param)
{
	int i;

	for (i = 0; i < MAX_CMD_ID && bbs_cmd_list[i].p_handle != 0; i++)
	{
		if (strcmp(cmd, bbs_cmd_list[i].cmd) == 0)
		{
			return ((*(bbs_cmd_list[i].p_handle))(param));
		}
	}

	return UNKNOWN_CMD;
}
