/***************************************************************************
							bbs.h  -  description
							 -------------------
	begin                : Mon Oct 18 2004
	copyright            : (C) 2004 by Leaflet
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

#ifndef _BBS_H_
#define _BBS_H_

#include <time.h>

// BBS
#define BBS_max_section 1024
#define BBS_max_username_length 20

extern char BBS_id[20];
extern char BBS_name[50];
extern char BBS_server[256];
extern char BBS_address[50];
extern unsigned int BBS_port;
extern long BBS_max_client;
extern long BBS_max_user;
extern char BBS_start_dt[50];

// User privilege
#define S_NONE 0x0
#define S_LIST 0x1
#define S_GETEXP 0x2
#define S_POST 0x4
#define S_MSG 0x8
#define S_MAIL 0x10
#define S_MAN_S 0x20
#define S_MAN_M 0x60 //(0x40 | 0x20)
#define S_ADMIN 0xe0 //(0x80 | 0x40 | 0x20)
#define S_ALL 0xff
#define S_DEFAULT 0x3 // 0x1 | 0x2

#define P_GUEST 0x0		// �ο�
#define P_USER 0x1		// ��ͨ�û�
#define P_AUTH_USER 0x2 // ��֤�û�
#define P_MAN_S 0x4		// ������
#define P_MAN_M 0x8		// �����?
#define P_MAN_C 0x10	// 8Ŀ���?
#define P_ADMIN_S 0x20	// ��ϵͳ����Ա
#define P_ADMIN_M 0x40	// ��ϵͳ����Ա

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

// Session
#define MAX_DELAY_TIME 600

extern char BBS_username[BBS_max_username_length];
extern BBS_user_priv BBS_priv;
extern int BBS_passwd_complex;
extern int BBS_user_money;

extern time_t BBS_login_tm;
extern time_t BBS_last_access_tm;
extern time_t BBS_last_sub_tm;

extern char BBS_current_section_name[20];

#endif //_BBS_H_
