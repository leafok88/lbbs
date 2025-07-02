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

#define MAX_SPLIT_FILE_LINES 65536

extern int str_length(const char *str, int skip_ctrl_seq);

extern int split_line(const char *buffer, int max_display_len, int *p_eol, int *p_display_len, int skip_ctrl_seq);

// p_line_widths may be NULL. Otherwise, the widths of each line will be store into the pointed array
extern long split_data_lines(const char *p_buf, int max_display_len, long *p_line_offsets, long line_offsets_count,
							 int skip_ctrl_seq, int *p_line_widths);

extern int str_filter(char *buffer, int skip_ctrl_seq);

#endif //_STR_PROCESS_H_
