/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * database
 *   - configuration and function of DB connection
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "database.h"
#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <mysql.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// Global declaration for database
char DB_ca_cert[FILE_PATH_LEN] = "conf/ca_cert.pem";
char DB_host[DB_host_max_len + 1];
char DB_username[DB_username_max_len + 1];
char DB_password[DB_password_max_len + 1];
char DB_database[DB_database_max_len + 1];
char DB_timezone[DB_timezone_max_len + 1];

MYSQL *db_open()
{
	MYSQL *db = NULL;
#ifdef HAVE_MARIADB_CLIENT
	my_bool verify_server_cert = 0;
#else
	unsigned int ssl_mode = SSL_MODE_PREFERRED;
#endif
	char sql[SQL_BUFFER_LEN];
	int fd;

	db = mysql_init(NULL);
	if (db == NULL)
	{
		log_error("mysql_init() failed\n");
		return NULL;
	}

	fd = open(DB_ca_cert, O_RDONLY);
	if (fd == -1)
	{
		if (errno != ENOENT)
		{
			log_error("open(%s) error: %d\n", DB_ca_cert, errno);
		}
	}
	else
	{
		close(fd);
#ifndef HAVE_MARIADB_CLIENT
		ssl_mode = SSL_MODE_VERIFY_CA;
#endif
	}

	if (mysql_ssl_set(db, NULL, NULL, DB_ca_cert, NULL, NULL) != 0)
	{
		log_error("mysql_ssl_set() error\n");
		return NULL;
	}

#ifdef HAVE_MARIADB_CLIENT
	if (mysql_optionsv(db, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify_server_cert) != 0)
	{
		log_error("mysql_optionsv() error\n");
		return NULL;
	}
#else
	if (mysql_options(db, MYSQL_OPT_SSL_MODE, &ssl_mode) != 0)
	{
		log_error("mysql_options() error\n");
		return NULL;
	}
#endif

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
