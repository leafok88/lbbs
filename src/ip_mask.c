/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ip_mask
 *   - IP address mask
 *
 * Copyright (C) 2004-2025 by Leaflet <leaflet@leafok.com>
 */

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
