/***************************************************************************
						  bbs_cmd.h  -  description
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
#ifndef _BBS_CMD_H_
#define _BBS_CMD_H_

#define MAX_CMD_LENGTH 20

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
