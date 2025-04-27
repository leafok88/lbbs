/***************************************************************************
						  passwd.c  -  description
							 -------------------
	begin                : Mon Oct 22 2004
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

#include <string.h>

int verify_pass_complexity(const char *password, const char *username, int len)
{
	int ch_repeat[128], i, verify_ok = 1, pass_len,
						   num_count = 0, upper_case = 0, lower_case = 0;

	pass_len = strlen(password);

	for (i = 0; i < 128; i++)
		ch_repeat[i] = 0;

	if (pass_len < len)
		verify_ok = 0;
	if (strstr(password, username) != NULL)
		verify_ok = 0;
	for (i = 0; i < pass_len && verify_ok == 1; i++)
	{
		ch_repeat[password[i]]++;
		if (ch_repeat[password[i]] > 2)
			verify_ok = 0;
		if (isdigit(password[i]))
			num_count++;
		if (isupper(password[i]))
			upper_case++;
		if (islower(password[i]))
			lower_case++;
		if (num_count > 2)
			verify_ok = 0;
	}
	if (upper_case == 0 || lower_case == 0 || num_count == 0)
		verify_ok = 0;

	return (verify_ok);
}
