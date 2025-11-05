/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * user_priv
 *   - basic operations of user privilege
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _USER_PRIV_H_
#define _USER_PRIV_H_

#include "bbs.h"
#include <mysql/mysql.h>

// User privilege
enum user_priv_t
{
	S_NONE = 0x0,
	S_LIST = 0x1,
	S_GETEXP = 0x2,
	S_POST = 0x4,
	S_MSG = 0x8,
	S_MAN_S = 0x20,
	S_MAN_M = 0x60, //(0x40 | 0x20)
	S_ADMIN = 0xe0, //(0x80 | 0x40 | 0x20)
	S_ALL = 0xff,
	S_DEFAULT = 0x3, // 0x1 | 0x2
};

enum user_level_t
{
	P_GUEST = 0x0,	   // 游客
	P_USER = 0x1,	   // 普通用户
	P_AUTH_USER = 0x2, // Reserved
	P_MAN_S = 0x4,	   // 副版主
	P_MAN_M = 0x8,	   // 正版主
	P_MAN_C = 0x10,	   // Reserved
	P_ADMIN_S = 0x20,  // 副系统管理员
	P_ADMIN_M = 0x40,  // 主系统管理员
};

struct user_priv
{
	int uid;
	int level;
	int g_priv;
	struct
	{
		int sid;
		int s_priv;
		int is_favor;
	} s_priv_list[BBS_max_section];
	int s_count;
};

typedef struct user_priv BBS_user_priv;

extern BBS_user_priv BBS_priv;

// Check whether user level matches any bit in the given param
inline int checklevel(BBS_user_priv *p_priv, int level)
{
	if (level == P_GUEST)
	{
		return 1;
	}

	return ((p_priv->level & level) ? 1 : 0);
}

// Check whether user level is equal or greater than the given param
inline int checklevel2(BBS_user_priv *p_priv, int level)
{
	return ((p_priv->level >= level) ? 1 : 0);
}

extern int setpriv(BBS_user_priv *p_priv, int sid, int priv, int is_favor);

extern int getpriv(BBS_user_priv *p_priv, int sid, int *p_is_favor);

inline int checkpriv(BBS_user_priv *p_priv, int sid, int priv)
{
	int is_favor = 0;
	return (((getpriv(p_priv, sid, &is_favor) & priv)) == priv ? 1 : 0);
}

inline int is_favor(BBS_user_priv *p_priv, int sid)
{
	int is_favor = 0;
	getpriv(p_priv, sid, &is_favor);
	return is_favor;
}

extern int load_priv(MYSQL *db, BBS_user_priv *p_priv, int uid);

#endif //_USER_PRIV_H_
