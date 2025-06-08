/***************************************************************************
						   editor.h  -  description
							 -------------------
	copyright            : (C) 2004-2025 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _EDITOR_H_
#define _EDITOR_H_

#include "screen.h"

#define MAX_EDITOR_DATA_LINES 65536
#define MAX_EDITOR_DATA_LINE_LENGTH 1024

struct editor_data_t
{
	char *p_data_lines[MAX_EDITOR_DATA_LINES];
	long data_line_total;
	char *p_display_lines[MAX_EDITOR_DATA_LINES];
	long display_line_lengths[MAX_EDITOR_DATA_LINES];
	long display_line_total;
};
typedef struct editor_data_t EDITOR_DATA;

extern EDITOR_DATA *editor_data_load(const char *p_data);
extern long editor_data_save(const EDITOR_DATA *p_editor_data, char *p_data, size_t buf_len);
extern void editor_data_cleanup(EDITOR_DATA *p_editor_data);

extern int editor_display(EDITOR_DATA *p_editor_data);

#endif //_EDITOR_H_
