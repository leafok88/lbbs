/***************************************************************************
						  log.c  -  description
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

#include "common.h"
#include "io.h"
#include "log.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#define STR_LOG_TIME_MAX_LEN 50

static FILE *fp_common_log;
static FILE *fp_error_log;

int log_begin(char *common_log_file, char *error_log_file)
{
	fp_common_log = fopen(common_log_file, "a");
	if (fp_common_log == NULL)
	{
		perror("log_begin failed\n");
		return -1;
	}

	fp_error_log = fopen(error_log_file, "a");
	if (fp_error_log == NULL)
	{
		perror("log_begin failed\n");
		return -2;
	}

	return 0;
}

void log_end()
{
	fclose(fp_common_log);
	fclose(fp_error_log);
}

inline static void log_head(char *buf, size_t len, int log_level, const char *app_file, int app_line)
{
	time_t t;
	struct tm gm_tm;
	char s_time[STR_LOG_TIME_MAX_LEN + 1];

	time(&t);
	gmtime_r(&t, &gm_tm);
	strftime(s_time, sizeof(s_time), "%Y-%m-%d %H:%M:%S", &gm_tm);

	if (log_level == LOG_LEVEL_COMMON)
	{
		snprintf(buf, len, "[%s] [%d] [INFO] ", s_time, getpid());
	}
	else // if (log_level == LOG_LEVEL_ERROR)
	{
		snprintf(buf, len, "[%s] [%d] [ERROR] [%s:%d] ", s_time, getpid(), app_file, app_line);
	}
}

int log_printf(int log_level, const char *app_file, int app_line, const char *format, ...)
{
	va_list args;
	int retval;
	char buf[LINE_BUFFER_LEN];
	FILE *fp_log;

	fp_log = (log_level == LOG_LEVEL_ERROR ? fp_error_log : fp_common_log);

	log_head(buf, sizeof(buf), log_level, app_file, app_line);
	strncat(buf, format, sizeof(buf) - strnlen(buf, sizeof(buf)));

	va_start(args, format);
	retval = vfprintf(fp_log, buf, args);
	va_end(args);

	fflush(fp_log);

	return retval;
}

int log_common_redir(int fd)
{
	int ret;
	close(fileno(fp_common_log));
	ret = dup2(fd, fileno(fp_common_log));
	return ret;
}

int log_error_redir(int fd)
{
	int ret;
	close(fileno(fp_error_log));
	ret = dup2(fd, fileno(fp_error_log));
	return ret;
}
