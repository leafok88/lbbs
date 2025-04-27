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

#include "bbs.h"
#include "common.h"
#include "io.h"
#include "database.h"
#include <mysql.h>
#include <regex.h>

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
			load_guest_info();
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

			ok = (check_user(username, password) == 0);
		}
		if (count >= 3 && !ok)
		{
			login_fail();
			return -1;
		}
	}

	return 0;
}

int check_user(char *username, char *password)
{
	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[1024];
	long int BBS_uid;
	int ret;

	// Verify format
	if (ireg("^[A-Za-z0-9_]{3,14}$", username, 0, NULL) != 0 ||
		ireg("^[A-Za-z0-9]{5,12}$", password, 0, NULL) != 0)
	{
		prints("\033[1;31m用户名或密码格式错误...\033[m\r\n");
		iflush();
		return 1;
	}

	db = (MYSQL *)db_open();
	if (db == NULL)
	{
		return -1;
	}

	sprintf(sql,
			"select UID,username,p_login from user_list where username='%s' "
			"and (password=MD5('%s') or password=SHA2('%s', 256)) and "
			"enable",
			username, password, password);
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
		if (atoi(row[2]) == 0)
		{
			mysql_free_result(rs);
			mysql_close(db);

			prints("\033[1;31m您目前无权登陆...\033[m\r\n");
			iflush();
			return 1;
		}
	}
	else
	{
		mysql_free_result(rs);

		sprintf(sql,
				"insert delayed into user_err_login_log"
				"(username,password,login_dt,login_ip) values"
				"('%s','%s',now(),'%s')",
				username, password, hostaddr_client);
		if (mysql_query(db, sql) != 0)
		{
			log_error("Insert into user_err_login_log failed\n");
			return -1;
		}

		mysql_close(db);

		prints("\033[1;31m错误的用户名或密码...\033[m\r\n");
		iflush();
		return 1;
	}
	mysql_free_result(rs);

	BBS_passwd_complex = verify_pass_complexity(password, username, 6);

	ret = load_user_info(db, BBS_uid);

	switch (ret)
	{
	case 0: // Login successfully
		return 0;
		break;
	case -1: // Load data error
		prints("\033[1;31m读取用户数据错误...\033[m\r\n");
		iflush();
		return -1;
		break;
	case -2: // Unused
		return 0;
		break;
	case -3: // Dead
		prints("\033[1;31m很遗憾，您已经永远离开了我们的世界！\033[m\r\n");
		iflush();
		return 1;
	default:
		return -2;
	}

	mysql_close(db);

	return 0;
}

int load_user_info(MYSQL *db, long int BBS_uid)
{
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[1024];
	long int BBS_auth_uid = 0;
	int life;
	time_t last_login_dt;

	sprintf(sql,
			"select life,UNIX_TIMESTAMP(last_login_dt) "
			"from user_pubinfo where UID=%ld limit 1",
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

	if (time(0) - last_login_dt >= 60 * 60 * 24 * life)
	{
		return (-3); // Dead
	}

	sprintf(sql,
			"select AUID from user_auth where UID=%ld"
			" and enable and expire_dt>now()",
			BBS_uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_auth failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_auth data failed\n");
		return -1;
	}
	if (row = mysql_fetch_row(rs))
	{
		BBS_auth_uid = atol(row[0]);
	}
	else
	{
		BBS_auth_uid = 0;
	}
	mysql_free_result(rs);

	sprintf(sql,
			"insert delayed into user_login_log"
			"(uid,login_dt,login_ip) values(%ld"
			",now(),'%s')",
			BBS_uid, hostaddr_client);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Insert into user_login_log failed\n");
		return -1;
	}

	load_priv(db, &BBS_priv, BBS_uid, BBS_auth_uid,
			  (!BBS_passwd_complex ? S_MAN_M : S_NONE) |
				  (BBS_auth_uid ? S_NONE : S_MAIL));

	BBS_last_access_tm = BBS_login_tm = time(0);
	BBS_last_sub_tm = time(0) - 60;

	sprintf(sql,
			"update user_pubinfo set visit_count=visit_count+1,"
			"last_login_dt=now() where uid=%ld",
			BBS_uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Update user_pubinfo failed\n");
		return -1;
	}

	return 0;
}

int load_guest_info(MYSQL *db, long int BBS_uid)
{
	MYSQL_RES *rs;
	MYSQL_ROW row;

	db = (MYSQL *)db_open();
	if (db == NULL)
	{
		return -1;
	}

	strcpy(BBS_username, "guest");

	load_priv(db, &BBS_priv, 0, 0, S_NONE);

	BBS_last_access_tm = BBS_login_tm = time(0);
	BBS_last_sub_tm = time(0) - 60;

	mysql_close(db);

	return 0;
}
