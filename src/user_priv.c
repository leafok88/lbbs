/***************************************************************************
						  user_priv.c  -  description
							 -------------------
	begin                : Mon Oct 22 2004
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

#include "bbs.h"
#include "common.h"
#include <mysql.h>

int checklevel(BBS_user_priv *p_priv, int level)
{
	return (((p_priv->level & level)) ^ level ? 0 : 1);
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
	return (((getpriv(p_priv, sid) & priv)) ^ priv ? 0 : 1);
}

int load_priv(MYSQL *db, BBS_user_priv *p_priv, long int uid,
			  long int auth_uid, int priv_excluse)
{
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[1024];
	int i;

	p_priv->uid = uid;
	p_priv->auid = auth_uid;
	p_priv->level = (uid == 0 ? P_GUEST : P_USER);
	p_priv->level |= (auth_uid == 0 ? P_GUEST : P_AUTH_USER);
	p_priv->g_priv = S_DEFAULT;

	if (db == NULL)
		return 1;

	// Admin
	sprintf(sql, "select aid,major from admin_config where UID=%ld"
				 " and enable and (now() between begin_dt and end_dt)",
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
	if (row = mysql_fetch_row(rs))
	{
		p_priv->level |= (atoi(row[1]) ? P_ADMIN_M : P_ADMIN_S);
		p_priv->g_priv |= (atoi(row[1]) ? S_ALL : S_ADMIN);
	}
	mysql_free_result(rs);

	// Permission
	sprintf(sql, "select p_post,p_msg,p_mail "
				 "from user_list where UID=%ld",
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
	if (row = mysql_fetch_row(rs))
	{
		p_priv->g_priv |= (atoi(row[0]) ? S_POST : 0);
		p_priv->g_priv |= (atoi(row[1]) ? S_MSG : 0);
		p_priv->g_priv |= (atoi(row[2]) ? S_MAIL : 0);
	}
	mysql_free_result(rs);

	// Verified
	sprintf(sql, "select verified from user_list where"
				 " UID=%ld",
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
	if (row = mysql_fetch_row(rs))
		p_priv->g_priv &= (atoi(row[0]) ? p_priv->g_priv : S_DEFAULT);
	mysql_free_result(rs);

	// IP ban
	sprintf(sql, "select begin_ip,end_ip from ban_ip_list"
				 " where ('%s' between begin_ip and end_ip) and enable",
			hostaddr_client);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query ban_ip_list failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get ban_ip_list data failed\n");
		return -1;
	}
	if (mysql_num_rows(rs) > 0)
		p_priv->g_priv &= S_DEFAULT;
	mysql_free_result(rs);

	// Section Class Master
	sprintf(sql, "select SID from section_class_master"
				 " left join section_config on section_class_master.CID"
				 "=section_config.CID where UID=%ld and section_class_master.enable"
				 " and (now() between begin_dt and end_dt)",
			uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query section_class_master failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get section_class_master data failed\n");
		return -1;
	}
	while (row = mysql_fetch_row(rs))
	{
		p_priv->level |= P_MAN_C;
		setpriv(p_priv, atoi(row[0]), getpriv(p_priv, atoi(row[0])) | S_MAN_M);
	}
	mysql_free_result(rs);

	// Section Master
	sprintf(sql, "select SID,major from section_master where"
				 " UID=%ld and enable and (now() between begin_dt and"
				 " end_dt)",
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
	while (row = mysql_fetch_row(rs))
	{
		p_priv->level |= (atoi(row[1]) ? P_MAN_M : P_MAN_S);
		setpriv(p_priv, atoi(row[0]), getpriv(p_priv, atoi(row[0])) | (atoi(row[1]) ? S_MAN_M : S_MAN_S));
	}
	mysql_free_result(rs);

	// Section status
	sprintf(sql, "select SID,exp_get,read_user_level,"
				 "write_user_level from section_config"
				 " left join section_class on section_config.CID="
				 "section_class.CID where section_config.enable and"
				 " section_class.enable order by SID");
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
	while (row = mysql_fetch_row(rs))
	{
		if (p_priv->level < atoi(row[2]))
			setpriv(p_priv, atoi(row[0]),
					getpriv(p_priv, atoi(row[0])) & (~S_LIST));
		if (p_priv->level < atoi(row[3]))
			setpriv(p_priv, atoi(row[0]),
					getpriv(p_priv, atoi(row[0])) & (~S_POST));
		if (!atoi(row[1]))
			setpriv(p_priv, atoi(row[0]),
					getpriv(p_priv, atoi(row[0])) & (~S_GETEXP));
	}
	mysql_free_result(rs);

	// Section User priv
	sprintf(sql, "select SID,`read`,`write` from section_user_priv"
				 " where UID=%ld order by SID",
			uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query section_user_priv failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get section_user_priv data failed\n");
		return -1;
	}
	while (row = mysql_fetch_row(rs))
	{
		setpriv(p_priv, atoi(row[0]),
				atoi(row[1]) ? (getpriv(p_priv, atoi(row[0])) | S_LIST)
							 : (getpriv(p_priv, atoi(row[0])) & ~S_LIST));
		setpriv(p_priv, atoi(row[0]),
				atoi(row[2]) ? (getpriv(p_priv, atoi(row[0])) | S_POST)
							 : (getpriv(p_priv, atoi(row[0])) & ~S_POST));
	}
	mysql_free_result(rs);

	// Section ban
	sprintf(sql, "select SID from ban_user_list where"
				 " UID=%ld and enable and unban_UID=0 and"
				 " (now() between ban_dt and unban_dt)",
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
	while (row = mysql_fetch_row(rs))
	{
		setpriv(p_priv, atoi(row[0]),
				getpriv(p_priv, atoi(row[0])) & (~S_POST));
	}
	mysql_free_result(rs);

	// Priv exclusion
	p_priv->g_priv &= (~priv_excluse);
	for (i = 0; i < p_priv->s_count; i++)
		p_priv->s_priv_list[i].s_priv &= (~priv_excluse);

	if (priv_excluse & S_MAN_M)
		p_priv->level &= (P_AUTH_USER | P_USER);

	return 0;
}
