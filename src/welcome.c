/***************************************************************************
						  main.c  -  description
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

#include "welcome.h"
#include "bbs.h"
#include "common.h"
#include "log.h"
#include "io.h"
#include "screen.h"
#include "database.h"
#include <mysql.h>
#include <string.h>

int bbs_welcome()
{
	char sql[SQL_BUFFER_LEN];

	u_int32_t u_online = 0;
	u_int32_t u_anonymous = 0;
	u_int32_t u_total = 0;
	u_int32_t max_u_online = 0;
	u_int32_t u_login_count = 0;

	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;

	db = (MYSQL *)db_open();
	if (db == NULL)
	{
		return -1;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT COUNT(*) AS cc FROM "
			 "(SELECT DISTINCT SID FROM user_online "
			 "WHERE last_tm >= SUBDATE(NOW(), INTERVAL %d SECOND)) AS t1",
			 BBS_user_off_line);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_online failed\n");
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_online data failed\n");
		return -2;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		u_online = (u_int32_t)atoi(row[0]);
	}
	mysql_free_result(rs);

	snprintf(sql, sizeof(sql),
			 "SELECT COUNT(*) AS cc FROM "
			 "(SELECT DISTINCT SID FROM user_online "
			 "WHERE UID = 0 AND last_tm >= SUBDATE(NOW(), INTERVAL %d SECOND)) AS t1",
			 BBS_user_off_line);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_online failed\n");
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_online data failed\n");
		return -2;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		u_anonymous = (u_int32_t)atoi(row[0]);
	}
	mysql_free_result(rs);

	snprintf(sql, sizeof(sql), "SELECT COUNT(UID) AS cc FROM user_list WHERE enable");
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_list failed\n");
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_list data failed\n");
		return -2;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		u_total = (u_int32_t)atoi(row[0]);
	}
	mysql_free_result(rs);

	snprintf(sql, sizeof(sql), "SELECT ID FROM user_login_log ORDER BY ID LIMIT 1");
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_login_log failed\n");
		return -2;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_login_log data failed\n");
		return -2;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		u_login_count = (u_int32_t)atoi(row[0]);
	}
	mysql_free_result(rs);

	mysql_close(db);

	// Log max user_online
	FILE *fin, *fout;
	if ((fin = fopen(VAR_MAX_USER_ONLINE, "r")) != NULL)
	{
		fscanf(fin, "%d", &max_u_online);
		fclose(fin);
	}
	if (u_online > max_u_online)
	{
		max_u_online = u_online;
		if ((fout = fopen(VAR_MAX_USER_ONLINE, "w")) == NULL)
		{
			log_error("Open max_user_online.dat failed\n");
			return -3;
		}
		fprintf(fout, "%d\n", max_u_online);
		fclose(fout);
	}

	// Display logo
	display_file(DATA_WELCOME);

	// Display welcome message
	prints("\033[1;35m欢迎光临\033[33m 【 %s 】 \033[35mBBS\r\n"
		   "\033[32m目前上站人数 [\033[36m%d/%d\033[32m] "
		   "匿名游客[\033[36m%d\033[32m] "
		   "注册用户数[\033[36m%d/%d\033[32m]\r\n"
		   "从 [\033[36m%s\033[32m] 起，最高人数记录："
		   "[\033[36m%d\033[32m]，累计访问人次：[\033[36m%d\033[32m]\r\n",
		   BBS_name, u_online, BBS_max_client, u_anonymous, u_total,
		   BBS_max_user, BBS_start_dt, max_u_online, u_login_count);

	return 0;
}
