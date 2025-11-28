/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * login
 *   - user authentication and online status manager
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bbs.h"
#include "common.h"
#include "database.h"
#include "io.h"
#include "ip_mask.h"
#include "log.h"
#include "login.h"
#include "screen.h"
#include "user_priv.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>
#include <mysql.h>
#include <sys/param.h>

static const int BBS_username_min_len = 3; // common len = 5, special len = 3
static const int BBS_password_min_len = 5; // legacy len = 5, current len = 6

static const int BBS_allowed_login_failures_within_interval = 10;
static const int BBS_login_failures_count_interval = 10; // minutes
static const int BBS_allowed_login_failures_per_account = 3;

const int BBS_login_retry_times = 3;

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

			return -1;
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
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int ret = 0;
	int BBS_uid = 0;
	char client_addr[IP_ADDR_LEN];
	int i;
	int ok = 1;
	char user_tz_env[BBS_user_tz_max_len + 2];

	db = db_open();
	if (db == NULL)
	{
		ret = -1;
		goto cleanup;
	}

	// Verify format
	for (i = 0; ok && username[i] != '\0'; i++)
	{
		if (!(isalpha((int)username[i]) || (i > 0 && (isdigit((int)username[i]) || username[i] == '_'))))
		{
			ok = 0;
		}
	}
	if (ok && (i < BBS_username_min_len || i > BBS_username_max_len))
	{
		ok = 0;
	}
	for (i = 0; ok && password[i] != '\0'; i++)
	{
		if (!isalnum((int)password[i]))
		{
			ok = 0;
		}
	}
	if (ok && (i < BBS_password_min_len || i > BBS_password_max_len))
	{
		ok = 0;
	}

	if (!ok)
	{
		prints("\033[1;31m用户名或密码格式错误...\033[m\r\n");
		ret = 1;
		goto cleanup;
	}

	// Begin transaction
	if (mysql_query(db, "SET autocommit=0") != 0)
	{
		log_error("SET autocommit=0 error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	if (mysql_query(db, "BEGIN") != 0)
	{
		log_error("Begin transaction error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	// Failed login attempts from the same source (subnet /24) during certain time period
	strncpy(client_addr, hostaddr_client, sizeof(client_addr) - 1);
	client_addr[sizeof(client_addr) - 1] = '\0';

	snprintf(sql, sizeof(sql),
			 "SELECT COUNT(*) AS err_count FROM user_err_login_log "
			 "WHERE login_dt >= SUBDATE(NOW(), INTERVAL %d MINUTE) "
			 "AND login_ip LIKE '%s'",
			 BBS_login_failures_count_interval,
			 ip_mask(client_addr, 1, '%'));
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_list error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_list data failed\n");
		ret = -1;
		goto cleanup;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		if (atoi(row[0]) >= BBS_allowed_login_failures_within_interval)
		{
			prints("\033[1;31m来源存在多次失败登陆尝试，请稍后再试，或使用Web方式访问\033[m\r\n");
			ret = 1;
			goto cleanup;
		}
	}
	mysql_free_result(rs);
	rs = NULL;

	// Failed login attempts against the current username since last successful login
	snprintf(sql, sizeof(sql),
			 "SELECT COUNT(*) AS err_count FROM user_err_login_log "
			 "LEFT JOIN user_list ON user_err_login_log.username = user_list.username "
			 "LEFT JOIN user_pubinfo ON user_list.UID = user_pubinfo.UID "
			 "WHERE user_err_login_log.username = '%s' "
			 "AND (user_err_login_log.login_dt >= user_pubinfo.last_login_dt "
			 "OR user_pubinfo.last_login_dt IS NULL)",
			 username);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_list error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_list data failed\n");
		ret = -1;
		goto cleanup;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		if (atoi(row[0]) >= BBS_allowed_login_failures_per_account)
		{
			prints("\033[1;31m账户存在多次失败登陆尝试，请使用Web方式登录解锁\033[m\r\n");
			ret = 1;
			goto cleanup;
		}
	}
	mysql_free_result(rs);
	rs = NULL;

	snprintf(sql, sizeof(sql),
			 "SELECT UID, username, p_login FROM user_list "
			 "WHERE username = '%s' AND password = SHA2('%s', 256) AND enable",
			 username, password);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_list error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_list data failed\n");
		ret = -1;
		goto cleanup;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		BBS_uid = atoi(row[0]);
		strncpy(BBS_username, row[1], sizeof(BBS_username) - 1);
		BBS_username[sizeof(BBS_username) - 1] = '\0';
		int p_login = atoi(row[2]);

		mysql_free_result(rs);
		rs = NULL;

		// Add user login log
		snprintf(sql, sizeof(sql),
				 "INSERT INTO user_login_log(UID, login_dt, login_ip) "
				 "VALUES(%d, NOW(), '%s')",
				 BBS_uid, hostaddr_client);
		if (mysql_query(db, sql) != 0)
		{
			log_error("Insert into user_login_log error: %s\n", mysql_error(db));
			ret = -1;
			goto cleanup;
		}

		// Commit transaction
		if (mysql_query(db, "COMMIT") != 0)
		{
			log_error("Commit transaction error: %s\n", mysql_error(db));
			ret = -1;
			goto cleanup;
		}

		if (p_login == 0)
		{
			prints("\033[1;31m您目前无权登陆...\033[m\r\n");
			ret = 1;
			goto cleanup;
		}
	}
	else
	{
		mysql_free_result(rs);
		rs = NULL;

		snprintf(sql, sizeof(sql),
				 "INSERT INTO user_err_login_log(username, password, login_dt, login_ip) "
				 "VALUES('%s', '%s', NOW(), '%s')",
				 username, password, hostaddr_client);
		if (mysql_query(db, sql) != 0)
		{
			log_error("Insert into user_err_login_log error: %s\n", mysql_error(db));
			ret = -1;
			goto cleanup;
		}

		// Commit transaction
		if (mysql_query(db, "COMMIT") != 0)
		{
			log_error("Commit transaction error: %s\n", mysql_error(db));
			ret = -1;
			goto cleanup;
		}

		prints("\033[1;31m错误的用户名或密码...\033[m\r\n");
		ret = 1;
		goto cleanup;
	}

	// Set AUTOCOMMIT = 1
	if (mysql_query(db, "SET autocommit=1") != 0)
	{
		log_error("SET autocommit=1 error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	ret = load_user_info(db, BBS_uid);

	switch (ret)
	{
	case 0: // Login successfully
		break;
	case -1: // Load data error
		prints("\033[1;31m读取用户数据错误...\033[m\r\n");
		ret = -1;
		goto cleanup;
	case -2: // Unused
		prints("\033[1;31m请通过Web登录更新用户许可协议...\033[m\r\n");
		ret = 1;
		goto cleanup;
	case -3: // Dead
		prints("\033[1;31m很遗憾，您已经永远离开了我们的世界！\033[m\r\n");
		ret = 1;
		goto cleanup;
	default:
		ret = -2;
		goto cleanup;
	}

	if (!SSH_v2 && checklevel2(&BBS_priv, P_MAN_S))
	{
		prints("\033[1;31m非普通账户必须使用SSH方式登录\033[m\r\n");
		ret = 1;
		goto cleanup;
	}

	snprintf(sql, sizeof(sql),
			 "UPDATE user_pubinfo SET visit_count = visit_count + 1, "
			 "last_login_dt = NOW() WHERE UID = %d",
			 BBS_uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Update user_pubinfo error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	if (user_online_add(db) != 0)
	{
		ret = -1;
		goto cleanup;
	}

	BBS_last_access_tm = BBS_login_tm = time(NULL);

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

cleanup:
	mysql_free_result(rs);
	mysql_close(db);

	return ret;
}

int load_user_info(MYSQL *db, int BBS_uid)
{
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int life;
	time_t last_login_dt;
	int ret = 0;

	snprintf(sql, sizeof(sql),
			 "SELECT life, UNIX_TIMESTAMP(last_login_dt), user_timezone, exp, nickname "
			 "FROM user_pubinfo WHERE UID = %d",
			 BBS_uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_pubinfo error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user_pubinfo data failed\n");
		ret = -1;
		goto cleanup;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		life = atoi(row[0]);
		last_login_dt = (time_t)atol(row[1]);

		strncpy(BBS_user_tz, row[2], sizeof(BBS_user_tz) - 1);
		BBS_user_tz[sizeof(BBS_user_tz) - 1] = '\0';

		BBS_user_exp = atoi(row[3]);

		strncpy(BBS_nickname, row[4], sizeof(BBS_nickname));
		BBS_nickname[sizeof(BBS_nickname) - 1] = '\0';
	}
	else
	{
		ret = -1; // Data not found
		goto cleanup;
	}
	mysql_free_result(rs);
	rs = NULL;

	if (life != 333 && life != 365 && life != 666 && life != 999 && // Not immortal
		time(NULL) - last_login_dt > 60 * 60 * 24 * life)
	{
		ret = -3; // Dead
		goto cleanup;
	}

	if (load_priv(db, &BBS_priv, BBS_uid) != 0)
	{
		ret = -1;
		goto cleanup;
	}

cleanup:
	mysql_free_result(rs);

	return ret;
}

int load_guest_info(void)
{
	MYSQL *db = NULL;
	int ret = 0;

	db = db_open();
	if (db == NULL)
	{
		ret = -1;
		goto cleanup;
	}

	strncpy(BBS_username, "guest", sizeof(BBS_username) - 1);
	BBS_username[sizeof(BBS_username) - 1] = '\0';

	BBS_user_exp = 0;

	strncpy(BBS_nickname, "Guest", sizeof(BBS_nickname));
	BBS_nickname[sizeof(BBS_nickname) - 1] = '\0';

	if (load_priv(db, &BBS_priv, 0) != 0)
	{
		ret = -1;
		goto cleanup;
	}

	if (user_online_add(db) != 0)
	{
		ret = -1;
		goto cleanup;
	}

	BBS_last_access_tm = BBS_login_tm = time(NULL);

cleanup:
	mysql_close(db);

	return ret;
}

int user_online_add(MYSQL *db)
{
	char sql[SQL_BUFFER_LEN];

	snprintf(sql, sizeof(sql),
			 "INSERT INTO visit_log(dt, IP) VALUES(NOW(), '%s')",
			 hostaddr_client);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Add visit log error: %s\n", mysql_error(db));
		return -1;
	}

	if (user_online_del(db) != 0)
	{
		return -2;
	}

	snprintf(sql, sizeof(sql),
			 "INSERT INTO user_online(SID, UID, ip, current_action, login_tm, last_tm) "
			 "VALUES('Telnet_Process_%d', %d, '%s', 'LOGIN', NOW(), NOW())",
			 getpid(), BBS_priv.uid, hostaddr_client);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Add user_online error: %s\n", mysql_error(db));
		return -3;
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

int user_online_exp(MYSQL *db)
{
	char sql[SQL_BUFFER_LEN];

	// +1 exp for every 5 minutes online since last logout
	// but at most 24 hours worth of exp can be gained in Telnet session
	snprintf(sql, sizeof(sql),
			 "UPDATE user_pubinfo SET exp = exp + FLOOR(LEAST(TIMESTAMPDIFF("
			 "SECOND, GREATEST(last_login_dt, IF(last_logout_dt IS NULL, last_login_dt, last_logout_dt)), NOW()"
			 ") / 60 / 5, 12 * 24)), last_logout_dt = NOW() "
			 "WHERE UID = %d",
			 BBS_priv.uid);
	if (mysql_query(db, sql) != 0)
	{
		log_error("Update user_pubinfo error: %s\n", mysql_error(db));
		return -1;
	}

	return 0;
}

int user_online_update(const char *action)
{
	MYSQL *db = NULL;
	char sql[SQL_BUFFER_LEN];

	if ((action == NULL || strcmp(BBS_current_action, action) == 0) &&
		time(NULL) - BBS_current_action_tm < BBS_current_action_refresh_interval) // No change
	{
		return 0;
	}

	if (action != NULL)
	{
		strncpy(BBS_current_action, action, sizeof(BBS_current_action) - 1);
		BBS_current_action[sizeof(BBS_current_action) - 1] = '\0';
	}

	BBS_current_action_tm = time(NULL);

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		return -1;
	}

	snprintf(sql, sizeof(sql),
			 "UPDATE user_online SET current_action = '%s', last_tm = NOW() "
			 "WHERE SID = 'Telnet_Process_%d'",
			 BBS_current_action, getpid());
	if (mysql_query(db, sql) != 0)
	{
		log_error("Update user_online error: %s\n", mysql_error(db));
		return -2;
	}

	mysql_close(db);

	return 1;
}
