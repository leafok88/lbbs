/***************************************************************************
						 register.c  -  description
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
	display_file_ex(DATA_REGISTER, 1, 1);

	return -1;
}
