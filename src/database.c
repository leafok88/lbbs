/***************************************************************************
						 database.c  -  description
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

#include "common.h"
#include <mysql.h>
#include <stdio.h>

MYSQL *
db_open()
{
	MYSQL *db;

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

	if (mysql_query(db, "SET CHARACTER SET gb2312") != 0)
	{
		log_error("SET CHARACTER SET failed\n");
		return NULL;
	}

	if (mysql_query(db, "SET NAMES \'gb2312\'") != 0)
	{
		log_error("SET NAMES failed\n");
		return NULL;
	}

	return db;
}
