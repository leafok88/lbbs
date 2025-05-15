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
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _STR_PROCESS_H_
#define _STR_PROCESS_H_

#include <stdio.h>

extern int split_line(const char *buffer, int max_display_len, int *p_eol, int *p_display_len);

extern int split_data_lines(const char *p_buf, int max_display_len, long *p_line_offsets, int max_line_cnt);

#endif //_STR_PROCESS_H_
