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

#include "io.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

FILE *fp_log_std;
FILE *fp_log_err;

int log_begin(char *file_log_std, char *file_log_err)
{
	fp_log_std = fopen(file_log_std, "a");
	if (fp_log_std == NULL)
	{
		perror("log_begin failed\n");
		return -1;
	}

	fp_log_err = fopen(file_log_err, "a");
	if (fp_log_err == NULL)
	{
		perror("log_begin failed\n");
		return -2;
	}

	return 0;
}

void log_end()
{
	fclose(fp_log_std);
	fclose(fp_log_err);
}

int log_head(char *buf, size_t len)
{
	time_t t;
	char s_time[256];
	t = time(0);

	strftime(s_time, sizeof(s_time), "%Y-%m-%d %H:%M:%S", localtime(&t));
	snprintf(buf, len, "[%s] [%d] ", s_time, getpid());

	return 0;
}

int log_std(const char *format, ...)
{
	va_list args;
	int retval;
	char buf[1024];

	log_head(buf, sizeof(buf));
	strncat(buf, format, sizeof(buf) - strnlen(buf, sizeof(buf)));

	va_start(args, format);
	retval = vfprintf(fp_log_std, buf, args);
	va_end(args);

	fflush(fp_log_std);

	return retval;
}

int log_error(const char *format, ...)
{
	va_list args;
	int retval;
	char buf[1024];

	log_head(buf, sizeof(buf));
	strncat(buf, format, sizeof(buf) - strnlen(buf, sizeof(buf)));

	va_start(args, format);
	retval = vfprintf(fp_log_err, buf, args);
	va_end(args);

	fflush(fp_log_err);

	return retval;
}

int log_std_redirect(int fd)
{
	int ret;
	close(fileno(fp_log_std));
	ret = dup2(fd, fileno(fp_log_std));
	return ret;
}

int log_err_redirect(int fd)
{
	int ret;
	close(fileno(fp_log_err));
	ret = dup2(fd, fileno(fp_log_err));
	return ret;
}
