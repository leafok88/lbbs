/***************************************************************************
						   log.h  -  description
							 -------------------
	begin                : Wed Mar 16 2004
	copyright            : (C) 2005 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _LOG_H_
#define _LOG_H_

#include <stddef.h>

extern int log_begin(char *file_log_std, char *file_log_err);

extern void log_end();

extern int log_head(char *buf, size_t len);

extern int log_std(const char *format, ...);

extern int log_error(const char *format, ...);

extern int log_std_redirect(int fd);

extern int log_err_redirect(int fd);

#endif //_LOG_H_
