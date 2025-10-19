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
	char *p_display_lines[MAX_EDITOR_DATA_LINES];
	long display_line_lengths[MAX_EDITOR_DATA_LINES]; // string length of display line
	int display_line_widths[MAX_EDITOR_DATA_LINES]; // display width of display line
	long display_line_total;
};
typedef struct editor_data_t EDITOR_DATA;

struct editor_ctx_t
{
	int reach_begin;
	int reach_end;
	long line_cursor;
	char msg[MSG_EXT_MAX_LEN];
};
typedef struct editor_ctx_t EDITOR_CTX;

extern int editor_memory_pool_init(void);
extern void editor_memory_pool_cleanup(void);

extern EDITOR_DATA *editor_data_load(const char *p_data);
extern long editor_data_save(const EDITOR_DATA *p_editor_data, char *p_data, size_t buf_len);
extern void editor_data_cleanup(EDITOR_DATA *p_editor_data);

extern int editor_display(EDITOR_DATA *p_editor_data);

extern int editor_data_insert(EDITOR_DATA *p_editor_data, long *p_display_line, long *p_offset,
							  const char *str, int str_len, long *p_last_updated_line);

extern int editor_data_delete(EDITOR_DATA *p_editor_data, long *p_display_line, long *p_offset,
							  long *p_last_updated_line, int del_line);

#endif //_EDITOR_H_
