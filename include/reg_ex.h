/***************************************************************************
						  reg_ex.h  -  description
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

#ifndef _REG_EX_H_
#define _REG_EX_H_

#include <regex.h>

extern int ireg(const char *pattern, const char *string, size_t nmatch,
				regmatch_t pmatch[]);

#endif //_REG_EX_H_
