/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bbs_cmd
 *   - manager of menu command handler
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _BBS_CMD_H_
#define _BBS_CMD_H_

enum menu_return_t
{
	MENU_OK = 0x0,
	REDRAW = 0x1,
	NOREDRAW = 0x2,
	EXITMENU = 0x3,
	UNKNOWN_CMD = 0xff,
	EXITBBS = 0xfe,
};

typedef int (*bbs_cmd_handler)(void *p_param);

struct _bbs_cmd
{
	const char *cmd;
	bbs_cmd_handler handler;
};

typedef struct _bbs_cmd BBS_CMD;

extern int load_cmd();

extern bbs_cmd_handler get_cmd_handler(const char *cmd);

#endif //_BBS_CMD_H_
