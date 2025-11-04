/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * log
 *   - logger
 *
 * Copyright (C) 2004-2025 by Leaflet <leaflet@leafok.com>
 */

#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>

enum
{
	LOG_LEVEL_COMMON = 1,
	LOG_LEVEL_ERROR = 2,
};

extern int log_begin(char *common_log_file, char *error_log_file);
extern void log_end();

extern int log_printf(int log_level, const char *app_file, int app_line, const char *format, ...);

#define log_common(...) log_printf(LOG_LEVEL_COMMON, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_printf(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

extern int log_common_redir(int fd);
extern int log_error_redir(int fd);

#endif //_LOG_H_
