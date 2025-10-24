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

#include "bbs.h"
#include "common.h"
#include "database.h"
#include "log.h"
#include "user_priv.h"
#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>

BBS_user_priv BBS_priv;

inline static int search_priv(BBS_user_priv *p_priv, int sid, int *p_offset)
{
	int left = 0;
	int right = p_priv->s_count - 1;
	int mid = 0;

	while (left < right)
	{
		mid = (left + right) / 2;

		if (sid < p_priv->s_priv_list[mid].sid)
		{
			right = mid - 1;
		}
		else if (sid > p_priv->s_priv_list[mid].sid)
		{
			left = mid + 1;
		}
		else // if (sid == p_priv->s_priv_list[mid].sid)
		{
			left = mid;
			break;
		}
	}

	*p_offset = left;

	return (sid == p_priv->s_priv_list[left].sid);
}

int setpriv(BBS_user_priv *p_priv, int sid, int priv, int is_favor)
{
	int offset;
	int i;

	if (sid == 0)
	{
		p_priv->g_priv = priv;
		return 0;
	}

	if (search_priv(p_priv, sid, &offset)) //found
	{
		p_priv->s_priv_list[offset].s_priv = priv;
		p_priv->s_priv_list[offset].is_favor = is_favor;
		return 0;
	}

	// not found
	if (p_priv->s_count >= BBS_max_section)
	{
		return -1;
	}

	// move items at [left, p_priv->s_count - 1] to [left + 1, p_priv->s_count]
	for (i = p_priv->s_count - 1; i >= offset; i--)
	{
		p_priv->s_priv_list[i + 1] = p_priv->s_priv_list[i];
	}
	p_priv->s_count++;

	// insert new item at offset left
	p_priv->s_priv_list[offset].sid = sid;
	p_priv->s_priv_list[offset].s_priv = priv;
	p_priv->s_priv_list[offset].is_favor = is_favor;

	return 0;
}

int getpriv(BBS_user_priv *p_priv, int sid, int *p_is_favor)
{
	int offset;

	if (search_priv(p_priv, sid, &offset)) //found
	{
		*p_is_favor = p_priv->s_priv_list[offset].is_favor;
		return p_priv->s_priv_list[offset].s_priv;
	}

	*p_is_favor = 0;
	return (sid >= 0 ? p_priv->g_priv : S_NONE);
}

int load_priv(MYSQL *db, BBS_user_priv *p_priv, int uid)
{
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int priv;
	int is_favor;

	p_priv->uid = uid;
	p_priv->level = (uid == 0 ? P_GUEST : P_USER);
	p_priv->g_priv = S_DEFAULT;
	p_priv->s_count = 0;

	if (db == NULL)
		return 1;

	// Permission
	snprintf(sql, sizeof(sql),
			 "SELECT p_post, p_msg FROM user_list WHERE UID = %d AND verified",
			 uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_list error: %s\n", mysql_error(db));
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
	snprintf(sql, sizeof(sql),
			 "SELECT major FROM admin_config WHERE UID = %d "
			 "AND enable AND (NOW() BETWEEN begin_dt AND end_dt)",
			 uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query admin_config error: %s\n", mysql_error(db));
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
	snprintf(sql, sizeof(sql),
			 "SELECT section_master.SID, major FROM section_master "
			 "INNER JOIN section_config ON section_master.SID = section_config.SID "
			 "WHERE UID = %d AND section_master.enable AND section_config.enable "
			 "AND (NOW() BETWEEN begin_dt AND end_dt)",
			 uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query section_master error: %s\n", mysql_error(db));
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
		priv = (getpriv(p_priv, atoi(row[0]), &is_favor) | (atoi(row[1]) ? S_MAN_M : S_MAN_S));
		setpriv(p_priv, atoi(row[0]), priv, is_favor);
	}
	mysql_free_result(rs);

	// Section status
	snprintf(sql, sizeof(sql),
			 "SELECT SID, exp_get, read_user_level, write_user_level FROM section_config "
			 "INNER JOIN section_class ON section_config.CID = section_class.CID "
			 "WHERE section_config.enable AND section_class.enable "
			 "ORDER BY SID");
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query section_config error: %s\n", mysql_error(db));
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get section_config data failed\n");
		return -1;
	}
	while ((row = mysql_fetch_row(rs)))
	{
		int priv = getpriv(p_priv, atoi(row[0]), &is_favor);
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
		setpriv(p_priv, atoi(row[0]), priv, is_favor);
	}
	mysql_free_result(rs);

	// Section ban
	snprintf(sql, sizeof(sql),
			 "SELECT SID FROM ban_user_list WHERE UID = %d AND enable "
			 "AND (NOW() BETWEEN ban_dt AND unban_dt)",
			 uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query ban_user_list error: %s\n", mysql_error(db));
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get ban_user_list data failed\n");
		return -1;
	}
	while ((row = mysql_fetch_row(rs)))
	{
		priv = getpriv(p_priv, atoi(row[0]), &is_favor) & (~S_POST);
		setpriv(p_priv, atoi(row[0]), priv, is_favor);
	}
	mysql_free_result(rs);

	// User favor section
	snprintf(sql, sizeof(sql),
			 "SELECT SID FROM section_favorite WHERE UID = %d",
			 uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query section_favorite error: %s\n", mysql_error(db));
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get section_favorite data failed\n");
		return -1;
	}
	while ((row = mysql_fetch_row(rs)))
	{
		priv = getpriv(p_priv, atoi(row[0]), &is_favor);
		if (!is_favor)
		{
			setpriv(p_priv, atoi(row[0]), priv, 1);
			priv = getpriv(p_priv, atoi(row[0]), &is_favor);
		}
	}
	mysql_free_result(rs);

	return 0;
}
