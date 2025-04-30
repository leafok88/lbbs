/***************************************************************************
						 register.c  -  description
							 -------------------
	begin                : Mon Oct 20 2004
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

#include "register.h"
#include "bbs.h"
#include "common.h"
#include "io.h"
#include "screen.h"
#include <string.h>
#include <mysql.h>
#include <regex.h>

int user_register()
{
	char temp[256];

	strcpy(temp, app_home_dir);
	strcat(temp, "data/register.txt");
	display_file_ex(temp, 1, 1);

	return -1;
}
