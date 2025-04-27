/***************************************************************************
						  bbs_cmd.h  -  description
							 -------------------
	begin                : Wed Mar 16 2005
	copyright            : (C) 2005 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef _BBS_CMD_H_
#define _BBS_CMD_H_

#define MAX_CMD_LENGTH 20
#define MAX_CMD_ID 100

#define MENU_OK 0x0
#define UNKNOWN_CMD 0xff
#define EXITBBS 0xfe
#define REDRAW 0x1
#define NOREDRAW 0x2

struct _bbs_cmd
{
	char cmd[MAX_CMD_LENGTH];
	int (*p_handle)(const char *p_param);
};

typedef struct _bbs_cmd BBS_CMD;

extern BBS_CMD bbs_cmd_list[MAX_CMD_ID];

#endif //_BBS_CMD_H_
