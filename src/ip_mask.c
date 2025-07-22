/***************************************************************************
						  ip_mask.c  -  description
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

#include "ip_mask.h"
#include <string.h>

const char *ip_mask(char *s, int level, char mask)
{
	char *p = s;

	if (level <= 0)
	{
		return s;
	}
	if (level > 4)
	{
		level = 4;
	}

	for (int i = 0; i < 4 - level; i++)
	{
		p = strchr(p, '.');
		if (p == NULL)
		{
			return s;
		}
		p++;
	}

	for (int i = 0; i < level; i++)
	{
		*p = mask;
		p++;
		if (i < level - 1)
		{
			*p = '.';
			p++;
		}
	}
	*p = '\0';

	return s;
}
