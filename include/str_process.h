/***************************************************************************
						  str_process.h  -  description
							 -------------------
	Copyright            : (C) 2004-2025 by Leaflet
	Email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _STR_PROCESS_H_
#define _STR_PROCESS_H_

#include <stdio.h>

extern int split_line(const char *buffer, int max_len, int *p_eol);

extern int split_file_lines(FILE *fin, int max_len, long *p_line_offsets, int max_line_cnt);

#endif //_STR_PROCESS_H_
