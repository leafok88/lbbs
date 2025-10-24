/***************************************************************************
						 test_bbs.c  -  description
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

#include "bbs.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
	int i;
	int user_level;

	const int user_points[] = {
		-50, -1, 0, 1, 49, 50, 51, 499, 500, 501, 99999, 100000, 100001};

	const int user_cnt = sizeof(user_points) / sizeof(int);

	for (i = 0; i < user_cnt; i++)
	{
		user_level = get_user_level(user_points[i]);
		printf("%10d\t%d\t%s\n", user_points[i], user_level, get_user_level_name(user_level));
	}

	return 0;
}
