/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * test_bbs
 *   - tester for BBS related common functions
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

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
