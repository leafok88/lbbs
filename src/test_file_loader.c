/***************************************************************************
						file_loader.c  -  description
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

#include "file_loader.h"
#include "log.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/shm.h>

const char *files[] = {
	"../data/welcome.txt",
	"../data/copyright.txt",
	"../data/register.txt",
	"../data/license.txt",
	"../data/login_error.txt",
	"../data/read_help.txt"};

int files_cnt = 6;

int main(int argc, char *argv[])
{
	int ret;
	int i;
	const void *p_shm;
	size_t data_len;
	long line_total;
	const void *p_data;
	const long *p_line_offsets;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_std_redirect(STDOUT_FILENO);
	log_err_redirect(STDERR_FILENO);

	ret = file_loader_init();
	if (ret < 0)
	{
		printf("file_loader_init() error (%d)\n", ret);
		return ret;
	}

	ret = file_loader_init();
	if (ret == 0)
	{
		printf("Rerun file_loader_init() error\n");
	}

	printf("Testing #1\n");

	for (i = 0; i < files_cnt; i++)
	{
		if (load_file_shm(files[i]) < 0)
		{
			printf("Load file %s error\n", files[i]);
		}
	}

	for (i = 0; i < files_cnt; i++)
	{
		if ((p_shm = get_file_shm(files[i], &data_len, &line_total, &p_data, &p_line_offsets)) == NULL)
		{
			printf("Get file shm %s error\n", files[i]);
		}
		else
		{
			printf("File: %s size: %ld lines: %ld\n", files[i], data_len, line_total);
		}
	}

	printf("Testing #2\n");

	for (i = 0; i < files_cnt; i++)
	{
		if (unload_file_shm(files[i]) < 0)
		{
			printf("Unload file %s error\n", files[i]);
		}
	}

	for (i = 0; i < files_cnt; i++)
	{
		if (load_file_shm(files[i]) < 0)
		{
			printf("Load file %s error\n", files[i]);
		}
	}

	printf("Testing #3\n");

	for (i = 0; i < files_cnt; i++)
	{
		if (i % 2 == 0)
		{
			if (unload_file_shm(files[i]) < 0)
			{
				printf("Unload file %s error\n", files[i]);
			}
		}
	}

	for (i = 0; i < files_cnt; i++)
	{
		if (i % 2 != 0)
		{
			if ((p_shm = get_file_shm(files[i], &data_len, &line_total, &p_data, &p_line_offsets)) == NULL)
			{
				printf("Get file shm %s error\n", files[i]);
			}
			else
			{
				printf("File: %s size: %ld lines: %ld\n", files[i], data_len, line_total);
			}
		}
	}

	file_loader_cleanup();
	file_loader_cleanup();

	log_end();

	return 0;
}
