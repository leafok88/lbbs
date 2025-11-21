/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * database
 *   - configuration and function of DB connection
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _DATABASE_H_
#define _DATABASE_H_

#include "common.h"
#include <mysql.h>

enum database_constant_t
{
    SQL_BUFFER_LEN = 10240,
    DB_host_max_len = 256,
    DB_username_max_len = 50,
    DB_password_max_len = 50,
    DB_database_max_len = 50,
    DB_timezone_max_len = 50,
};

// Database
extern char DB_ca_cert[FILE_PATH_LEN];
extern char DB_host[DB_host_max_len + 1];
extern char DB_username[DB_username_max_len + 1];
extern char DB_password[DB_password_max_len + 1];
extern char DB_database[DB_database_max_len + 1];
extern char DB_timezone[DB_timezone_max_len + 1];

extern MYSQL *db_open();

#endif //_DATABASE_H_
