/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * test_file_loader
 *   - tester for shared memory based file loader
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "file_loader.h"
#include "log.h"
#include "trie_dict.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>

static const char TRIE_DICT_SHM_FILE[] = "~trie_dict_shm.dat";

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
	FILE *fp;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_common_redir(STDOUT_FILENO);
	log_error_redir(STDERR_FILENO);

	if ((fp = fopen(TRIE_DICT_SHM_FILE, "w")) == NULL)
	{
		log_error("fopen(%s) error\n", TRIE_DICT_SHM_FILE);
		return -1;
	}
	fclose(fp);

	if (trie_dict_init(TRIE_DICT_SHM_FILE, TRIE_NODE_PER_POOL) < 0)
	{
		printf("trie_dict_init failed\n");
		return -1;
	}

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
		if (load_file(files[i]) < 0)
		{
			printf("Load file %s error\n", files[i]);
		}
	}

	for (i = 0; i < files_cnt; i++)
	{
		if ((p_shm = get_file_shm_readonly(files[i], &data_len, &line_total, &p_data, &p_line_offsets)) == NULL)
		{
			printf("Get file shm %s error\n", files[i]);
		}
		else
		{
			printf("File: %s size: %ld lines: %ld\n", files[i], data_len, line_total);

			for (int j = 0; j < line_total; j++)
			{
				printf("Line %d: %ld ~ %ld\n", j, p_line_offsets[j], p_line_offsets[j + 1]);
			}

			if (detach_file_shm(p_shm) < 0)
			{
				log_error("detach_file_shm(%s) error\n", files[i]);
			}
		}
	}

	printf("Testing #2\n");

	for (i = 0; i < files_cnt; i++)
	{
		if (unload_file(files[i]) < 0)
		{
			printf("Unload file %s error\n", files[i]);
		}
	}

	for (i = 0; i < files_cnt; i++)
	{
		if (load_file(files[i]) < 0)
		{
			printf("Load file %s error\n", files[i]);
		}
	}

	printf("Testing #3\n");

	for (i = 0; i < files_cnt; i++)
	{
		if (i % 2 == 0)
		{
			if (unload_file(files[i]) < 0)
			{
				printf("Unload file %s error\n", files[i]);
			}
		}
	}

	for (i = 0; i < files_cnt; i++)
	{
		if (i % 2 != 0)
		{
			if ((p_shm = get_file_shm_readonly(files[i], &data_len, &line_total, &p_data, &p_line_offsets)) == NULL)
			{
				printf("Get file shm %s error\n", files[i]);
			}
			else
			{
				printf("File: %s size: %ld lines: %ld\n", files[i], data_len, line_total);

				if (detach_file_shm(p_shm) < 0)
				{
					log_error("detach_file_shm(%s) error\n", files[i]);
				}
			}
		}
	}

	file_loader_cleanup();
	file_loader_cleanup();

	trie_dict_cleanup();

	if (unlink(TRIE_DICT_SHM_FILE) < 0)
	{
		log_error("unlink(%s) error\n", TRIE_DICT_SHM_FILE);
		return -1;
	}

	log_end();

	return 0;
}
