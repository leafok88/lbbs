/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * log
 *   - logger
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "io.h"
#include "log.h"
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

enum _log_constant_t
{
	STR_LOG_TIME_MAX_LEN = 50,
};

static char path_common_log[FILE_PATH_LEN];
static char path_error_log[FILE_PATH_LEN];
static FILE *fp_common_log = NULL;
static FILE *fp_error_log = NULL;
static int redir_common_log = 0;
static int redir_error_log = 0;

int log_begin(const char *common_log_file, const char *error_log_file)
{
	strncpy(path_common_log, common_log_file, sizeof(path_common_log) - 1);
	path_common_log[sizeof(path_common_log) - 1] = '\0';
	strncpy(path_error_log, error_log_file, sizeof(path_error_log) - 1);
	path_error_log[sizeof(path_error_log) - 1] = '\0';

	fp_common_log = fopen(path_common_log, "a");
	if (fp_common_log == NULL)
	{
		fprintf(stderr, "fopen(%s) error: %s\n", path_common_log, strerror(errno));
		return -1;
	}

	fp_error_log = fopen(path_error_log, "a");
	if (fp_error_log == NULL)
	{
		fprintf(stderr, "fopen(%s) error: %s\n", path_error_log, strerror(errno));
		fclose(fp_common_log);
		fp_common_log = NULL;
		return -2;
	}

	redir_common_log = 0;
	redir_error_log = 0;

	return 0;
}

void log_end()
{
	if (fp_common_log)
	{
		fclose(fp_common_log);
		fp_common_log = NULL;
	}
	if (fp_error_log)
	{
		fclose(fp_error_log);
		fp_error_log = NULL;
	}
}

inline static int log_head(char *buf, size_t len, int log_level, const char *app_file, int app_line)
{
	time_t t;
	struct tm gm_tm;
	char s_time[STR_LOG_TIME_MAX_LEN + 1];
	int ret;

	time(&t);
	gmtime_r(&t, &gm_tm);
	strftime(s_time, sizeof(s_time), "%Y-%m-%d %H:%M:%S", &gm_tm);

	if (log_level == LOG_LEVEL_COMMON)
	{
		ret = snprintf(buf, len, "[%s] [%d] [INFO] ", s_time, getpid());
	}
	else if (log_level == LOG_LEVEL_ERROR)
	{
		ret = snprintf(buf, len, "[%s] [%d] [ERROR] [%s:%d] ", s_time, getpid(), app_file, app_line);
	}
	else // if (log_level == LOG_LEVEL_DEBUG)
	{
		ret = snprintf(buf, len, "[%s] [%d] [DEBUG] [%s:%d] ", s_time, getpid(), app_file, app_line);
	}

	return ret;
}

int log_printf(enum log_level_t log_level, const char *app_file, int app_line, const char *format, ...)
{
	va_list args;
	char buf[LINE_BUFFER_LEN];
	FILE *fp_log;
	int offset;
	int ret;

	fp_log = (log_level == LOG_LEVEL_COMMON ? fp_common_log : fp_error_log);

	offset = log_head(buf, sizeof(buf), log_level, app_file, app_line);

	va_start(args, format);
	ret = vsnprintf(buf + offset, sizeof(buf) - (size_t)offset, format, args);
	va_end(args);

	if (ret < 0)
	{
		// Encoding error
		return -1;
	}
	else if (offset + ret >= sizeof(buf))
	{
		buf[sizeof(buf) - 2] = '\n'; // Add newline for truncated messages
		buf[sizeof(buf) - 1] = '\0'; // Ensure null termination
		if (fputs(buf, fp_log) == EOF)
		{
			return -3; // Write error
		}
		ret = -2; // Indicate truncation
	}
	else
	{
		if (fputs(buf, fp_log) == EOF)
		{
			return -3; // Write error
		}
		ret = offset + ret;
	}

	if (fflush(fp_log) == EOF)
	{
		return -4; // Flush error
	}

	return ret;
}

int log_common_redir(int fd)
{
	redir_common_log = 1;
	return dup2(fd, fileno(fp_common_log));
}

int log_error_redir(int fd)
{
	redir_error_log = 1;
	return dup2(fd, fileno(fp_error_log));
}

int log_restart(void)
{
	FILE *fp_common = NULL;
	FILE *fp_error = NULL;

	if (!redir_common_log)
	{
		fp_common = fopen(path_common_log, "a");
		if (fp_common == NULL)
		{
			log_error("fopen(%s) error: %s\n", path_common_log, strerror(errno));
			return -1;
		}
	}

	if (!redir_error_log)
	{
		fp_error = fopen(path_error_log, "a");
		if (fp_error == NULL)
		{
			log_error("fopen(%s) error: %s\n", path_error_log, strerror(errno));
			if (fp_common)
			{
				fclose(fp_common);
			}
			return -2;
		}
	}

	// Apply new file pointers
	if (fp_common)
	{
		fclose(fp_common_log);
		fp_common_log = fp_common;
	}
	if (fp_error)
	{
		fclose(fp_error_log);
		fp_error_log = fp_error;
	}

	return 0;
}
