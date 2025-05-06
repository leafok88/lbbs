#include "money.h"
#include "database.h"
#include "log.h"
#include "user_priv.h"
#include <stdio.h>
#include <mysql.h>

int BBS_user_money = 0;

int money_balance()
{
	return BBS_user_money;
}

int money_deposit(int money)
{
	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int ret = 0;

	if (money <= 0)
	{
		return 0;
	}

	db = db_open();
	if (db == NULL)
	{
		return -1;
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

	snprintf(sql, sizeof(sql),
			 "SELECT game_money FROM user_pubinfo WHERE UID = %ld FOR UPDATE",
			 BBS_priv.uid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_pubinfo failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user money failed\n");
		return -1;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		BBS_user_money = atoi(row[0]);
	}
	mysql_free_result(rs);

	if (BBS_user_money + money < USER_MONEY_MAX)
	{
		ret = money;
		BBS_user_money += money;
	}
	else if (BBS_user_money < USER_MONEY_MAX)
	{
		ret = USER_MONEY_MAX - BBS_user_money;
		BBS_user_money = USER_MONEY_MAX;
	}
	else // Balance exceeds 1 billion !!!
	{
		mysql_close(db);
		return -2;
	}

	snprintf(sql, sizeof(sql),
			 "UPDATE user_pubinfo SET game_money = %d WHERE UID = %ld",
			 BBS_user_money, BBS_priv.uid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Update user_pubinfo failed\n");
		return -1;
	}

	// Commit transaction
	if (mysql_query(db, "COMMIT") != 0)
	{
		log_error("Commit transaction failed\n");
		return -1;
	}

	mysql_close(db);

	return ret;
}

int money_withdraw(int money)
{
	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int ret = 0;

	if (money <= 0)
	{
		return 0;
	}

	db = db_open();
	if (db == NULL)
	{
		return -1;
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

	snprintf(sql, sizeof(sql),
			 "SELECT game_money FROM user_pubinfo WHERE UID = %ld FOR UPDATE",
			 BBS_priv.uid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_pubinfo failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user money failed\n");
		return -1;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		BBS_user_money = atoi(row[0]);
	}
	mysql_free_result(rs);

	if (BBS_user_money >= money)
	{
		ret = money;
		BBS_user_money -= money;
	}
	else // No enough balance
	{
		mysql_close(db);
		return -2;
	}

	snprintf(sql, sizeof(sql),
			 "UPDATE user_pubinfo SET game_money = %d WHERE UID = %ld",
			 BBS_user_money, BBS_priv.uid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Update user_pubinfo failed\n");
		return -1;
	}

	// Commit transaction
	if (mysql_query(db, "COMMIT") != 0)
	{
		log_error("Commit transaction failed\n");
		return -1;
	}

	mysql_close(db);

	return ret;
}

int money_refresh(void)
{
	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];

	db = db_open();
	if (db == NULL)
	{
		return -1;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT game_money FROM user_pubinfo WHERE UID = %ld FOR UPDATE",
			 BBS_priv.uid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user_pubinfo failed\n");
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get user money failed\n");
		return -1;
	}
	if ((row = mysql_fetch_row(rs)))
	{
		BBS_user_money = atoi(row[0]);
	}
	mysql_free_result(rs);

	mysql_close(db);

	return 0;
}
