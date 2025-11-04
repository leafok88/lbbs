/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * database
 *   - configuration and function of DB connection
 *
 * Copyright (C) 2004-2025 by Leaflet <leaflet@leafok.com>
 */

#include "common.h"
#include "database.h"
#include "log.h"
#include <stdio.h>
#include <mysql/mysql.h>

// Global declaration for database
char DB_host[256];
char DB_username[50];
char DB_password[50];
char DB_database[50];
char DB_timezone[50];

MYSQL *db_open()
{
	MYSQL *db = NULL;
	char sql[SQL_BUFFER_LEN];

	db = mysql_init(NULL);
	if (db == NULL)
	{
		log_error("mysql_init() failed\n");
		return NULL;
	}

	if (mysql_real_connect(db, DB_host, DB_username, DB_password, DB_database,
						   0, NULL, 0) == NULL)
	{
		log_error("mysql_real_connect() error: %s\n", mysql_error(db));
		mysql_close(db);
		return NULL;
	}

	if (mysql_set_character_set(db, "utf8") != 0)
	{
		log_error("Set character set error: %s\n", mysql_error(db));
		mysql_close(db);
		return NULL;
	}

	snprintf(sql, sizeof(sql),
			 "SET time_zone = '%s'",
			 DB_timezone);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Set timezone error: %s\n", mysql_error(db));
		mysql_close(db);
		return NULL;
	}

	return db;
}
