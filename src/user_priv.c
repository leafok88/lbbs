/***************************************************************************
						  user_priv.c  -  description
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

#include "user_priv.h"
#include "bbs.h"
#include "common.h"
#include "database.h"
#include "log.h"
#include <stdio.h>
#include <mysql.h>

BBS_user_priv BBS_priv;

int checklevel(BBS_user_priv *p_priv, int level)
{
	if (level == P_GUEST)
	{
		return 1;
	}

	return ((p_priv->level & level) ? 1 : 0);
}

int setpriv(BBS_user_priv *p_priv, int sid, int priv)
{
	int i;
	if (sid > 0)
	{
		for (i = 0; i < p_priv->s_count; i++)
		{
			if (p_priv->s_priv_list[i].sid == sid)
			{
				p_priv->s_priv_list[i].s_priv = priv;
				return 0;
			}
		}
		if (i < BBS_max_section)
		{
			p_priv->s_priv_list[i].s_priv = priv;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		p_priv->g_priv = priv;
	}

	return 0;
}

int getpriv(BBS_user_priv *p_priv, int sid)
{
	int i;
	for (i = 0; i < p_priv->s_count; i++)
	{
		if (p_priv->s_priv_list[i].sid == sid)
			return p_priv->s_priv_list[i].s_priv;
	}

	return (sid >= 0 ? p_priv->g_priv : S_NONE);
}

int checkpriv(BBS_user_priv *p_priv, int sid, int priv)
{
	return (((getpriv(p_priv, sid) & priv)) == priv ? 1 : 0);
}

int load_priv(MYSQL *db, BBS_user_priv *p_priv, long int uid)
{
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];

	p_priv->uid = uid;
	p_priv->level = (uid == 0 ? P_GUEST : P_USER);
	p_priv->g_priv = S_DEFAULT;

	if (db == NULL)
		return 1;

	// Permission
	snprintf(sql, sizeof(sql), "SELECT p_post, p_msg FROM user_list WHERE UID = %ld AND verified",
			uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_list failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_list data failed\n");
		return -1;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		p_priv->g_priv |= (atoi(row[0]) ? S_POST : 0);
		p_priv->g_priv |= (atoi(row[1]) ? S_MSG : 0);
	}
	mysql_free_result(rs);

	// Admin
	snprintf(sql, sizeof(sql), "SELECT major FROM admin_config WHERE UID = %ld "
				 "AND enable AND (NOW() BETWEEN begin_dt AND end_dt)",
			uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query admin_config failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get admin_config data failed\n");
		return -1;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		p_priv->level |= (atoi(row[0]) ? P_ADMIN_M : P_ADMIN_S);
		p_priv->g_priv |= (atoi(row[0]) ? S_ALL : S_ADMIN);
	}
	mysql_free_result(rs);

	// Section Master
	snprintf(sql, sizeof(sql), "SELECT section_master.SID, major FROM section_master "
				 "INNER JOIN section_config ON section_master.SID = section_config.SID "
				 "WHERE UID = %ld AND section_master.enable AND section_config.enable "
				 "AND (NOW() BETWEEN begin_dt AND end_dt)",
			uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query section_master failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get section_master data failed\n");
		return -1;
	}
	while ((row = mysql_fetch_row(rs)))
	{
		p_priv->level |= (atoi(row[1]) ? P_MAN_M : P_MAN_S);
		setpriv(p_priv, atoi(row[0]), getpriv(p_priv, atoi(row[0])) | (atoi(row[1]) ? S_MAN_M : S_MAN_S));
	}
	mysql_free_result(rs);

	// Section status
	snprintf(sql, sizeof(sql), "SELECT SID, exp_get, read_user_level, write_user_level FROM section_config "
				 "INNER JOIN section_class ON section_config.CID = section_class.CID "
				 "WHERE section_config.enable AND section_class.enable "
				 "ORDER BY SID");
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query section_config failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get section_config data failed\n");
		return -1;
	}
	while ((row = mysql_fetch_row(rs)))
	{
		int priv = getpriv(p_priv, atoi(row[0]));
		if (p_priv->level < atoi(row[2]))
		{
			priv &= (~S_LIST);
		}
		if (p_priv->level < atoi(row[3]))
		{
			priv &= (~S_POST);
		}
		if (!atoi(row[1]))
		{
			priv &= (~S_GETEXP);
		}
		setpriv(p_priv, atoi(row[0]), priv);
	}
	mysql_free_result(rs);

	// Section ban
	snprintf(sql, sizeof(sql), "SELECT SID FROM ban_user_list WHERE UID = %ld AND enable "
				 "AND (NOW() BETWEEN ban_dt AND unban_dt)",
			uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query ban_user_list failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get ban_user_list data failed\n");
		return -1;
	}
	while ((row = mysql_fetch_row(rs)))
	{
		setpriv(p_priv, atoi(row[0]),
				getpriv(p_priv, atoi(row[0])) & (~S_POST));
	}
	mysql_free_result(rs);

	return 0;
}
