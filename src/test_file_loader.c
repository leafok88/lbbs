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

const char *files[] = {
	"../data/welcome.txt",
	"../data/copyright.txt",
	"../data/register.txt",
	"../data/license.txt",
	"../data/login_error.txt"};

int files_cnt = 5;

int main(int argc, char *argv[])
{
	int ret;
	int file_loader_pool_size = 2;
	int i;
	const FILE_MMAP *p_file_mmap;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_std_redirect(STDOUT_FILENO);
	log_err_redirect(STDERR_FILENO);

	ret = file_loader_init(file_loader_pool_size);
	if (ret < 0)
	{
		printf("file_loader_init() error (%d)\n", ret);
		return ret;
	}

	ret = file_loader_init(file_loader_pool_size);
	if (ret == 0)
	{
		printf("Rerun file_loader_init() error\n");
	}

	printf("Testing #1\n");

	for (i = 0; i < file_loader_pool_size; i++)
	{
		if (load_file_mmap(files[i]) < 0)
		{
			printf("Load file %s error\n", files[i]);
		}
	}

	for (i = 0; i < file_loader_pool_size; i++)
	{
		if ((p_file_mmap = get_file_mmap(files[i])) == NULL)
		{
			printf("Get file mmap %s error\n", files[i]);
		}
		else
		{
			printf("File: %s size: ", files[i]);
			printf("size: %ld lines: %ld\n", p_file_mmap->size, p_file_mmap->line_total);
		}
	}

	for (i = 0; i < file_loader_pool_size; i++)
	{
		if (unload_file_mmap(files[i]) < 0)
		{
			printf("Unload file %s error\n", files[i]);
		}
	}

	printf("Testing #2\n");

	for (i = 0; i < files_cnt; i++)
	{
		if (i >= file_loader_pool_size)
		{
			if (unload_file_mmap(files[i - file_loader_pool_size]) < 0)
			{
				printf("Unload file %s error\n", files[i]);
			}
		}

		if (load_file_mmap(files[i]) < 0)
		{
			printf("Load file %s error\n", files[i]);
		}
	}

	printf("Testing #3\n");

	for (i = 0; i < files_cnt; i++)
	{
		if ((p_file_mmap = get_file_mmap(files[i])) == NULL)
		{
			printf("Get file mmap %s error\n", files[i]);
		}
		else
		{
			printf("File: %s size: ", files[i]);
			printf("size: %ld lines: %ld\n", p_file_mmap->size, p_file_mmap->line_total);
		}
	}

	file_loader_cleanup();
	file_loader_cleanup();

	log_end();

	return 0;
}
