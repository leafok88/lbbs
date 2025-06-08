/***************************************************************************
						  login.c  -  description
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

#define _POSIX_C_SOURCE 200112L

#include "login.h"
#include "bbs.h"
#include "user_priv.h"
#include "common.h"
#include "log.h"
#include "io.h"
#include "screen.h"
#include "database.h"
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <regex.h>
#include <stdlib.h>
#include <unistd.h>
#include <mysql/mysql.h>

int bbs_login(void)
{
	char username[BBS_username_max_len + 1];
	char password[BBS_password_max_len + 1];
	int i = 0;
	int ok = 0;

	for (; !SYS_server_exit && !ok && i < BBS_login_retry_times; i++)
	{
		prints("\033[1;33m请输入帐号\033[m(试用请输入`\033[1;36mguest\033[m', "
			   "注册请输入`\033[1;31mnew\033[m'): ");
		iflush();

		if (str_input(username, sizeof(username), DOECHO) < 0)
		{
			continue;
		}

		if (strcmp(username, "guest") == 0)
		{
			load_guest_info();

			return 0;
		}

		if (strcmp(username, "new") == 0)
		{
			display_file(DATA_REGISTER, 1);

			return 0;
		}

		if (username[0] != '\0')
		{
			prints("\033[1;37m请输入密码\033[m: ");
			iflush();

			if (str_input(password, sizeof(password), NOECHO) < 0)
			{
				continue;
			}

			ok = (check_user(username, password) == 0);
			iflush();
		}
	}

	if (!ok)
	{
		display_file(DATA_LOGIN_ERROR, 1);
		return -1;
	}

	log_common("User \"%s\"(%ld) login from %s:%d\n",
			   BBS_username, BBS_priv.uid, hostaddr_client, port_client);

	return 0;
}

int check_user(const char *username, const char *password)
{
	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int ret;
	int BBS_uid = 0;
	char client_addr[IP_ADDR_LEN];
	int i;
	int ok = 1;
	char user_tz_env[BBS_user_tz_max_len + 2];

	db = db_open();
	if (db == NULL)
	{
		return -1;
	}

	// Verify format
	for (i = 0; ok && username[i] != '\0'; i++)
	{
		if (!(isalpha(username[i]) || (i > 0 && isdigit(username[i]))))
		{
			ok = 0;
		}
	}
	if (ok && (i < 3 || i > 12))
	{
		ok = 0;
	}
	for (i = 0; ok && password[i] != '\0'; i++)
	{
		if (!isalnum(password[i]))
		{
			ok = 0;
		}
	}
	if (ok && (i < 5 || i > 12))
	{
		ok = 0;
	}

	if (!ok)
	{
		prints("\033[1;31m用户名或密码格式错误...\033[m\r\n");
		return 1;
	}

	// Begin transaction
	if (mysql_query(db, "SET autocommit=0") != 0)
	{
		log_error("SET autocommit=0 error: %s\n", mysql_error(db));
		mysql_close(db);
		return -1;
	}

	if (mysql_query(db, "BEGIN") != 0)
	{
		log_error("Begin transaction error: %s\n", mysql_error(db));
		mysql_close(db);
		return -1;
	}

	// Failed login attempts from the same source (subnet /24) during certain time period
	strncpy(client_addr, hostaddr_client, sizeof(client_addr) - 1);
	client_addr[sizeof(client_addr) - 1] = '\0';

	snprintf(sql, sizeof(sql),
			 "SELECT COUNT(*) AS err_count FROM user_err_login_log "
			 "WHERE login_dt >= SUBDATE(NOW(), INTERVAL 10 MINUTE) "
			 "AND login_ip LIKE '%s'",
			 ip_mask(client_addr, 1, '%'));
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_list error: %s\n", mysql_error(db));
		mysql_close(db);
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_list data failed\n");
		mysql_close(db);
		return -1;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		if (atoi(row[0]) >= 2)
		{
			mysql_free_result(rs);
			mysql_close(db);

			prints("\033[1;31m来源存在多次失败登陆尝试，请稍后再试\033[m\r\n");

			return 1;
		}
	}
	mysql_free_result(rs);

	// Failed login attempts against the current username during certain time period
	snprintf(sql, sizeof(sql),
			 "SELECT COUNT(*) AS err_count FROM user_err_login_log "
			 "WHERE username = '%s' AND login_dt >= SUBDATE(NOW(), INTERVAL 1 DAY)",
			 username);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_list error: %s\n", mysql_error(db));
		mysql_close(db);
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_list data failed\n");
		mysql_close(db);
		return -1;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		if (atoi(row[0]) >= 5)
		{
			mysql_free_result(rs);
			mysql_close(db);

			prints("\033[1;31m账户存在多次失败登陆尝试，请使用Web方式登录\033[m\r\n");

			return 1;
		}
	}
	mysql_free_result(rs);

	snprintf(sql, sizeof(sql),
			 "SELECT UID, username, p_login FROM user_list "
			 "WHERE username = '%s' AND password = SHA2('%s', 256) AND enable",
			 username, password);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_list error: %s\n", mysql_error(db));
		mysql_close(db);
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_list data failed\n");
		mysql_close(db);
		return -1;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		BBS_uid = atoi(row[0]);
		strncpy(BBS_username, row[1], sizeof(BBS_username) - 1);
		BBS_username[sizeof(BBS_username) - 1] = '\0';
		int p_login = atoi(row[2]);

		mysql_free_result(rs);

		// Add user login log
		snprintf(sql, sizeof(sql),
				 "INSERT INTO user_login_log(UID, login_dt, login_ip) "
				 "VALUES(%d, NOW(), '%s')",
				 BBS_uid, hostaddr_client);
		if (mysql_query(db, sql) != 0)
		{
			log_error("Insert into user_login_log error: %s\n", mysql_error(db));
			mysql_close(db);
			return -1;
		}

		// Commit transaction
		if (mysql_query(db, "COMMIT") != 0)
		{
			log_error("Commit transaction error: %s\n", mysql_error(db));
			mysql_close(db);
			return -1;
		}

		if (p_login == 0)
		{
			mysql_free_result(rs);
			mysql_close(db);

			prints("\033[1;31m您目前无权登陆...\033[m\r\n");
			return 1;
		}
	}
	else
	{
		mysql_free_result(rs);
		mysql_close(db);

		snprintf(sql, sizeof(sql),
				 "INSERT INTO user_err_login_log(username, password, login_dt, login_ip) "
				 "VALUES('%s', '%s', NOW(), '%s')",
				 username, password, hostaddr_client);
		if (mysql_query(db, sql) != 0)
		{
			log_error("Insert into user_err_login_log error: %s\n", mysql_error(db));
			return -1;
		}

		// Commit transaction
		if (mysql_query(db, "COMMIT") != 0)
		{
			log_error("Commit transaction error: %s\n", mysql_error(db));
			mysql_close(db);
			return -1;
		}

		prints("\033[1;31m错误的用户名或密码...\033[m\r\n");
		mysql_close(db);
		return 1;
	}

	// Set AUTOCOMMIT = 1
	if (mysql_query(db, "SET autocommit=1") != 0)
	{
		log_error("SET autocommit=1 error: %s\n", mysql_error(db));
		mysql_close(db);
		return -1;
	}

	ret = load_user_info(db, BBS_uid);

	switch (ret)
	{
	case 0: // Login successfully
		break;
	case -1: // Load data error
		prints("\033[1;31m读取用户数据错误...\033[m\r\n");
		mysql_close(db);
		return -1;
	case -2: // Unused
		prints("\033[1;31m请通过Web登录更新用户许可协议...\033[m\r\n");
		mysql_close(db);
		return 1;
	case -3: // Dead
		prints("\033[1;31m很遗憾，您已经永远离开了我们的世界！\033[m\r\n");
		mysql_close(db);
		return 1;
	default:
		mysql_close(db);
		return -2;
	}

	snprintf(sql, sizeof(sql),
			 "UPDATE user_pubinfo SET visit_count = visit_count + 1, "
			 "last_login_dt = NOW() WHERE UID = %d",
			 BBS_uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Update user_pubinfo error: %s\n", mysql_error(db));
		mysql_close(db);
		return -1;
	}

	if (user_online_add(db) != 0)
	{
		mysql_close(db);
		return -1;
	}

	BBS_last_access_tm = BBS_login_tm = time(0);

	// Set user tz to process env
	if (BBS_user_tz[0] != '\0')
	{
		user_tz_env[0] = ':';
		strncpy(user_tz_env + 1, BBS_user_tz, sizeof(user_tz_env) - 2);
		user_tz_env[sizeof(user_tz_env) - 1] = '\0';

		if (setenv("TZ", user_tz_env, 1) == -1)
		{
			log_error("setenv(TZ = %s) error %d\n", user_tz_env, errno);
			return -3;
		}

		tzset();
	}

	mysql_close(db);

	return 0;
}

int load_user_info(MYSQL *db, int BBS_uid)
{
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int life;
	time_t last_login_dt;

	snprintf(sql, sizeof(sql),
			 "SELECT life, UNIX_TIMESTAMP(last_login_dt), user_timezone "
			 "FROM user_pubinfo WHERE UID = %d",
			 BBS_uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_pubinfo error: %s\n", mysql_error(db));
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_pubinfo data failed\n");
		return -1;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		life = atoi(row[0]);
		last_login_dt = (time_t)atol(row[1]);

		strncpy(BBS_user_tz, row[2], sizeof(BBS_user_tz) - 1);
		BBS_user_tz[sizeof(BBS_user_tz) - 1] = '\0';
	}
	else
	{
		mysql_free_result(rs);
		return -1; // Data not found
	}
	mysql_free_result(rs);

	if (life != 333 && life != 365 && life != 666 && life != 999 && // Not immortal
		time(0) - last_login_dt > 60 * 60 * 24 * life)
	{
		return -3; // Dead
	}

	if (load_priv(db, &BBS_priv, BBS_uid) != 0)
	{
		return -1;
	}

	return 0;
}

int load_guest_info(void)
{
	MYSQL *db;

	db = db_open();
	if (db == NULL)
	{
		return -1;
	}

	strncpy(BBS_username, "guest", sizeof(BBS_username) - 1);
	BBS_username[sizeof(BBS_username) - 1] = '\0';

	if (load_priv(db, &BBS_priv, 0) != 0)
	{
		return -1;
	}

	if (user_online_add(db) != 0)
	{
		return -1;
	}

	BBS_last_access_tm = BBS_login_tm = time(0);

	mysql_close(db);
	
	return 0;
}

int user_online_add(MYSQL *db)
{
	char sql[SQL_BUFFER_LEN];

	if (user_online_del(db) != 0)
	{
		return -1;
	}

	snprintf(sql, sizeof(sql),
			 "INSERT INTO user_online(SID, UID, ip, login_tm, last_tm) "
			 "VALUES('Telnet_Process_%d', %d, '%s', NOW(), NOW())",
			 getpid(), BBS_priv.uid, hostaddr_client);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Add user_online error: %s\n", mysql_error(db));
		return -1;
	}

	return 0;
}

int user_online_del(MYSQL *db)
{
	char sql[SQL_BUFFER_LEN];

	snprintf(sql, sizeof(sql),
			 "DELETE FROM user_online WHERE SID = 'Telnet_Process_%d'",
			 getpid());
	if (mysql_query(db, sql) != 0)
	{
		log_error("Delete user_online error: %s\n", mysql_error(db));
		return -1;
	}

	return 0;
}
