/***************************************************************************
						  login.c  -  description
							 -------------------
	begin                : Mon Oct 20 2004
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

#include "login.h"
#include "register.h"
#include "bbs.h"
#include "user_priv.h"
#include "reg_ex.h"
#include "common.h"
#include "log.h"
#include "io.h"
#include "screen.h"
#include "database.h"
#include <string.h>
#include <mysql.h>
#include <regex.h>
#include <unistd.h>

void login_fail()
{
	char temp[256];

	strcpy(temp, app_home_dir);
	strcat(temp, "data/login_error.txt");
	display_file(temp);

	sleep(1);
}

int bbs_login()
{
	char username[20], password[20];
	int count, ok;

	// Input username
	count = 0;
	ok = 0;
	while (!ok)
	{
		prints("\033[1;33m请输入帐号\033[m(试用请输入 `\033[1;36mguest\033[m', "
			   "注册请输入`\033[1;31mnew\033[m'): ");
		iflush();

		str_input(username, 19, DOECHO);
		count++;

		if (strcmp(username, "guest") == 0)
		{
			MYSQL * db = db_open();
			if (db == NULL)
			{
				return -1;
			}
		
			load_guest_info(db);

			mysql_close(db);

			return 0;
		}

		if (strcmp(username, "new") == 0)
		{
			if (user_register() == 0)
				return 0;
			else
				return -2;
		}

		if (strlen(username) > 0)
		{
			// Input password
			prints("\033[1;37m请输入密码\033[m: ");
			iflush();

			str_input(password, 19, NOECHO);

			MYSQL * db = db_open();
			if (db == NULL)
			{
				return -1;
			}
		
			ok = (check_user(db, username, password) == 0);

			mysql_close(db);
		}
		if (count >= 3 && !ok)
		{
			login_fail();
			return -1;
		}
	}

	return 0;
}

int check_user(MYSQL *db, char *username, char *password)
{
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[1024];
	long int BBS_uid;
	int ret;

	// Verify format
	if (ireg("^[A-Za-z][A-Za-z0-9]{2,11}$", username, 0, NULL) != 0 ||
		ireg("^[A-Za-z0-9]{5,12}$", password, 0, NULL) != 0)
	{
		prints("\033[1;31m用户名或密码格式错误...\033[m\r\n");
		iflush();
		return 1;
	}

	// Begin transaction
	if (mysql_query(db, "SET autocommit=0") != 0)
	{
		log_error("SET autocommit=0 failed\n");
		return -1;
	}

	if (mysql_query(db, "BEGIN") != 0)
	{
		log_error("Begin transaction failed\n");
		return -1;
	}

	sprintf(sql,
			"SELECT UID, username, p_login FROM user_list "
			"WHERE username = '%s' AND password = SHA2('%s', 256) AND enable",
			username, password);
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
		BBS_uid = atol(row[0]);
		strcpy(BBS_username, row[1]);
		int p_login = atoi(row[2]);

		mysql_free_result(rs);

		// Add user login log
		sprintf(sql,
				"INSERT INTO user_login_log(UID, login_dt, login_ip) "
				"VALUES(%ld, NOW(), '%s')",
				BBS_uid, hostaddr_client);
		if (mysql_query(db, sql) != 0)
		{
			log_error("Insert into user_login_log failed\n");
			return -1;
		}

		// Commit transaction
		if (mysql_query(db, "COMMIT") != 0)
		{
			log_error("Commit transaction failed\n");
			return -1;
		}

		if (p_login == 0)
		{
			mysql_free_result(rs);

			prints("\033[1;31m您目前无权登陆...\033[m\r\n");
			iflush();
			return 1;
		}
	}
	else
	{
		mysql_free_result(rs);

		sprintf(sql,
				"INSERT INTO user_err_login_log(username, password, login_dt, login_ip) "
				"VALUES('%s', '%s', NOW(), '%s')",
				username, password, hostaddr_client);
		if (mysql_query(db, sql) != 0)
		{
			log_error("Insert into user_err_login_log failed\n");
			return -1;
		}

		// Commit transaction
		if (mysql_query(db, "COMMIT") != 0)
		{
			log_error("Commit transaction failed\n");
			return -1;
		}

		prints("\033[1;31m错误的用户名或密码...\033[m\r\n");
		iflush();
		return 1;
	}

	// Set AUTOCOMMIT = 1
	if (mysql_query(db, "SET autocommit=1") != 0)
	{
		log_error("SET autocommit=1 failed\n");
		return -1;
	}

	ret = load_user_info(db, BBS_uid);

	switch (ret)
	{
	case 0: // Login successfully
		break;
	case -1: // Load data error
		prints("\033[1;31m读取用户数据错误...\033[m\r\n");
		iflush();
		return -1;
	case -2: // Unused
		prints("\033[1;31m请通过Web登录更新用户许可协议...\033[m\r\n");
		iflush();
		return 1;
	case -3: // Dead
		prints("\033[1;31m很遗憾，您已经永远离开了我们的世界！\033[m\r\n");
		iflush();
		return 1;
	default:
		return -2;
	}

	sprintf(sql,
			"UPDATE user_pubinfo SET visit_count = visit_count + 1, "
			"last_login_dt = NOW() WHERE UID = %ld",
			BBS_uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Update user_pubinfo failed\n");
		return -1;
	}

	BBS_last_access_tm = BBS_login_tm = time(0);

	return 0;
}

int load_user_info(MYSQL *db, long int BBS_uid)
{
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[1024];
	int life;
	time_t last_login_dt;

	sprintf(sql,
			"SELECT life, UNIX_TIMESTAMP(last_login_dt) "
			"FROM user_pubinfo WHERE UID = %ld",
			BBS_uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_pubinfo failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_pubinfo data failed\n");
		return -1;
	}
	if (row = mysql_fetch_row(rs))
	{
		life = atoi(row[0]);
		last_login_dt = (time_t)atol(row[1]);
	}
	else
	{
		mysql_free_result(rs);
		return (-1); // Data not found
	}
	mysql_free_result(rs);

	if (life != 333 && life != 365 && life != 666 && life != 999 && // Not immortal
		time(0) - last_login_dt > 60 * 60 * 24 * life)
	{
		return (-3); // Dead
	}

	load_priv(db, &BBS_priv, BBS_uid);

	return 0;
}

int load_guest_info(MYSQL *db)
{
	MYSQL_RES *rs;
	MYSQL_ROW row;

	strcpy(BBS_username, "guest");

	load_priv(db, &BBS_priv, 0);

	BBS_last_access_tm = BBS_login_tm = time(0);

	return 0;
}
