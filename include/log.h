/***************************************************************************
						   log.h  -  description
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

#ifndef _LOG_H_
#define _LOG_H_

#include <stddef.h>

extern int log_begin(char *common_log_file, char *error_log_file);

extern void log_end();

extern int log_head(char *buf, size_t len);

extern int log_common(const char *format, ...);

extern int log_error(const char *format, ...);

extern int log_common_redir(int fd);

extern int log_error_redir(int fd);

#endif //_LOG_H_
