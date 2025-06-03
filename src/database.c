/***************************************************************************
						 database.c  -  description
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
	MYSQL *db;
	char sql[SQL_BUFFER_LEN];

	db = mysql_init(NULL);
	if (db == NULL)
	{
		log_error("mysql_init() failed\n");
		return NULL;
	}

	db = mysql_real_connect(db, DB_host, DB_username, DB_password, DB_database,
							0, NULL, 0);
	if (db == NULL)
	{
		log_error("mysql_connect() failed\n");
		return NULL;
	}

	if (mysql_set_character_set(db, "gb2312") != 0)
	{
		log_error("Set character set failed\n");
		return NULL;
	}

	snprintf(sql, sizeof(sql),
			"SET time_zone = '%s'",
			DB_timezone);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Set timezone error: %s\n", mysql_error(db));
		return NULL;
	}

	return db;
}
