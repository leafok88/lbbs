/***************************************************************************
						file_loader.h  -  description
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

#ifndef _FILE_LOADER_H_
#define _FILE_LOADER_H_

#include "common.h"
#include <stddef.h>

struct file_mmap_t
{
	size_t size;
	void *p_data;
	long line_offsets[MAX_FILE_LINES];
	long line_total;
};
typedef struct file_mmap_t FILE_MMAP;

extern int file_loader_init(int max_file_mmap_count);
extern void file_loader_cleanup(void);

extern int load_file_mmap(const char *filename);
extern int unload_file_mmap(const char *filename);

extern const FILE_MMAP *get_file_mmap(const char *filename);

#endif //_FILE_LOADER_H_
