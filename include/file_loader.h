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

extern int file_loader_init(void);
extern void file_loader_cleanup(void);

extern int load_file(const char *filename);
extern int unload_file(const char *filename);

extern const void *get_file_shm_readonly(const char *filename, size_t *p_data_len, long *p_line_total, const void **pp_data, const long **pp_line_offsets);
extern int detach_file_shm(const void *p_shm);

#endif //_FILE_LOADER_H_
