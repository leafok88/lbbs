/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * database
 *   - configuration and function of DB connection
 *
 * Copyright (C) 2004-2025 by Leaflet <leaflet@leafok.com>
 */

#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <mysql/mysql.h>

#define SQL_BUFFER_LEN 10240

// Database
extern char DB_host[256];
extern char DB_username[50];
extern char DB_password[50];
extern char DB_database[50];
extern char DB_timezone[50];

extern MYSQL *db_open();

#endif //_DATABASE_H_
