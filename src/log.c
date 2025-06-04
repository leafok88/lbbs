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

#include "log.h"
#include "io.h"
#include "common.h"
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define _POSIX_C_SOURCE 200809L
#include <string.h>

FILE *fp_common_log;
FILE *fp_error_log;

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

int log_head(char *buf, size_t len)
{
	time_t t;
	struct tm gm_tm;
	char s_time[256];

	time(&t);
	gmtime_r(&t, &gm_tm);
	strftime(s_time, sizeof(s_time), "%Y-%m-%d %H:%M:%S", &gm_tm);
	snprintf(buf, len, "[%s] [%d] ", s_time, getpid());

	return 0;
}

int log_common(const char *format, ...)
{
	va_list args;
	int retval;
	char buf[LINE_BUFFER_LEN];

	log_head(buf, sizeof(buf));
	strncat(buf, format, sizeof(buf) - strnlen(buf, sizeof(buf)));

	va_start(args, format);
	retval = vfprintf(fp_common_log, buf, args);
	va_end(args);

	fflush(fp_common_log);

	return retval;
}

int log_error(const char *format, ...)
{
	va_list args;
	int retval;
	char buf[LINE_BUFFER_LEN];

	log_head(buf, sizeof(buf));
	strncat(buf, format, sizeof(buf) - strnlen(buf, sizeof(buf)));

	va_start(args, format);
	retval = vfprintf(fp_error_log, buf, args);
	va_end(args);

	fflush(fp_error_log);

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
