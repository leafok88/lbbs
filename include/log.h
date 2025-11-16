/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * log
 *   - logger
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>

enum log_level_t
{
	LOG_LEVEL_COMMON = 1,
	LOG_LEVEL_ERROR = 2,
};

extern int log_begin(const char *common_log_file, const char *error_log_file);
extern void log_end();

extern int log_printf(enum log_level_t log_level, const char *app_file, int app_line, const char *format, ...);

#define log_common(...) log_printf(LOG_LEVEL_COMMON, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_printf(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

extern int log_common_redir(int fd);
extern int log_error_redir(int fd);

extern int log_restart(void);

#endif //_LOG_H_
