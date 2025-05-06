/***************************************************************************
						  user_priv.h  -  description
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

#ifndef _USER_PRIV_H_
#define _USER_PRIV_H_

#include "bbs.h"
#include <mysql.h>

// User privilege
#define S_NONE 0x0
#define S_LIST 0x1
#define S_GETEXP 0x2
#define S_POST 0x4
#define S_MSG 0x8
#define S_MAN_S 0x20
#define S_MAN_M 0x60 //(0x40 | 0x20)
#define S_ADMIN 0xe0 //(0x80 | 0x40 | 0x20)
#define S_ALL 0xff
#define S_DEFAULT 0x3 // 0x1 | 0x2

#define P_GUEST 0x0		//游客
#define P_USER 0x1		//普通用户
//#define P_AUTH_USER 0x2 // Reserved
#define P_MAN_S 0x4		//副版主
#define P_MAN_M 0x8		//正版主
#define P_MAN_C 0x10	// Reserved
#define P_ADMIN_S 0x20	//副系统管理员
#define P_ADMIN_M 0x40	//主系统管理员

struct user_priv
{
	long int uid;
	int level;
	int g_priv;
	struct
	{
		int sid;
		int s_priv;
	} s_priv_list[BBS_max_section];
	int s_count;
};

typedef struct user_priv BBS_user_priv;

extern BBS_user_priv BBS_priv;

extern int checklevel(BBS_user_priv *p_priv, int level);

extern int setpriv(BBS_user_priv *p_priv, int sid, int priv);

extern int getpriv(BBS_user_priv *p_priv, int sid);

extern int checkpriv(BBS_user_priv *p_priv, int sid, int priv);

extern int load_priv(MYSQL *db, BBS_user_priv *p_priv, long int uid);

#endif //_USER_PRIV_H_
