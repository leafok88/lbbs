/***************************************************************************
						  database.h  -  description
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

#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <mysql.h>

#define SQL_BUFFER_LEN 2048

// Database
extern char DB_host[256];
extern char DB_username[50];
extern char DB_password[50];
extern char DB_database[50];
extern char DB_timezone[50];

extern MYSQL *db_open();

#endif //_DATABASE_H_
