/***************************************************************************
                          main.c  -  description
                             -------------------
    begin                : Mon Oct 11 2004
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
#include "io.h"
#include "database.h"
#include <mysql.h>
#include <string.h>

int
bbs_welcome ()
{
  char buffer[256], sql[1024], temp[256];
  long u_online = 0, u_anonymous = 0, u_total = 0,
    max_u_online = 0, u_login_count = 0;
  MYSQL *db;
  MYSQL_RES *rs;
  MYSQL_ROW row;

  db = (MYSQL *) db_open ();
  if (db == NULL)
    {
      return -1;
    }

  strcpy (sql,
	  "select SID as cc from user_online where current_action not in"
	  " ('max_user_limit','max_ip_limit','max_session_limit','exit')"
	  " group by SID");
  if (mysql_query (db, sql) != 0)
    {
      log_error ("Query user_online failed\n");
      return -2;
    }
  if ((rs = mysql_store_result (db)) == NULL)
    {
      log_error ("Get user_online data failed\n");
      return -2;
    }
  u_online = mysql_num_rows (rs);
  mysql_free_result (rs);

  strcpy (sql,
	  "select SID as cc from user_online where UID=0 and current_action not in"
	  " ('max_user_limit','max_ip_limit','max_session_limit','exit')"
	  " group by SID");
  if (mysql_query (db, sql) != 0)
    {
      log_error ("Query user_online failed\n");
      return -2;
    }
  if ((rs = mysql_store_result (db)) == NULL)
    {
      log_error ("Get user_online data failed\n");
      return -2;
    }
  u_anonymous = mysql_num_rows (rs);
  mysql_free_result (rs);

  strcpy (sql, "select count(*) as cc from user_list where enable");
  if (mysql_query (db, sql) != 0)
    {
      log_error ("Query user_list failed\n");
      return -2;
    }
  if ((rs = mysql_store_result (db)) == NULL)
    {
      log_error ("Get user_list data failed\n");
      return -2;
    }
  if (row = mysql_fetch_row (rs))
    {
      u_total = atol (row[0]);
    }
  mysql_free_result (rs);

  strcpy (sql, "select max(ID) as login_count from user_login_log");
  if (mysql_query (db, sql) != 0)
    {
      log_error ("Query user_login_log failed\n");
      return -2;
    }
  if ((rs = mysql_store_result (db)) == NULL)
    {
      log_error ("Get user_login_log data failed\n");
      return -2;
    }
  if (row = mysql_fetch_row (rs))
    {
      u_login_count = atol (row[0]);
    }
  mysql_free_result (rs);

  mysql_close (db);

  //Log max user_online
  FILE *fin, *fout;
  strcpy (temp, app_home_dir);
  strcat (temp, "data/max_user_online.dat");
  if ((fin = fopen (temp, "r")) != NULL)
    {
      fscanf (fin, "%ld", &max_u_online);
      fclose (fin);
    }
  if (u_online > max_u_online)
    {
      max_u_online = u_online;
      if ((fout = fopen (temp, "w")) == NULL)
	{
	  log_error ("Open max_user_online.dat failed\n");
	  return -3;
	}
      fprintf (fout, "%ld\n", max_u_online);
      fclose (fout);
    }

  //Display logo
  strcpy (temp, app_home_dir);
  strcat (temp, "data/welcome.txt");
  display_file (temp);

  //Display welcome message
  prints ("\033[1;35m欢迎光临\033[33m 【 %s 】 \033[35mBBS\r\n"
	  "\033[32m目前上站人数 [\033[36m%ld/%ld\033[32m] "
	  "匿名游客[\033[36m%ld\033[32m] "
	  "注册用户数[\033[36m%ld/%ld\033[32m]\r\n"
	  "从 [\033[36m%s\033[32m] 起，最高人数记录："
	  "[\033[36m%ld\033[32m]，累计访问人次：[\033[36m%ld\033[32m]\r\n",
	  BBS_name, u_online, BBS_max_client, u_anonymous, u_total,
	  BBS_max_user, BBS_start_dt, max_u_online, u_login_count);

  return 0;
}
