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
#include "trie_dict.h"
#include "str_process.h"
#include "log.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define FILE_MMAP_COUNT_LIMIT 256

static FILE_MMAP *p_file_mmap_pool = NULL;
static int file_mmap_count = 0;
static int file_mmap_free_index = -1;
static TRIE_NODE *p_trie_file_dict = NULL;

int file_loader_init(int max_file_mmap_count)
{
	if (max_file_mmap_count > FILE_MMAP_COUNT_LIMIT)
	{
		log_error("file_loader_init(%d) argument error\n", max_file_mmap_count);
		return -1;
	}

	if (p_file_mmap_pool != NULL || p_trie_file_dict != NULL)
	{
		log_error("File loader already initialized\n");
		return -1;
	}

	p_file_mmap_pool = (FILE_MMAP *)calloc((size_t)max_file_mmap_count, sizeof(FILE_MMAP));
	if (p_file_mmap_pool == NULL)
	{
		log_error("calloc(%d p_file_mmap_pool) error\n", max_file_mmap_count);
		return -2;
	}

	file_mmap_count = max_file_mmap_count;
	file_mmap_free_index = 0;

	p_trie_file_dict = trie_dict_create();
	if (p_trie_file_dict == NULL)
	{
		log_error("trie_dict_create() error\n");
		return -2;
	}

	return 0;
}

void file_loader_cleanup(void)
{
	if (p_trie_file_dict != NULL)
	{
		trie_dict_destroy(p_trie_file_dict);
		p_trie_file_dict = NULL;
	}

	if (p_file_mmap_pool != NULL)
	{
		for (int i = 0; i < file_mmap_count; i++)
		{
			if (p_file_mmap_pool[i].p_data == NULL)
			{
				continue;
			}

			if (munmap(p_file_mmap_pool[i].p_data, p_file_mmap_pool[i].size) < 0)
			{
				log_error("munmap() error (%d)\n", errno);
			}
		}
		free(p_file_mmap_pool);
		p_file_mmap_pool = NULL;
	}

	file_mmap_count = 0;
	file_mmap_free_index = -1;
}

int load_file_mmap(const char *filename)
{
	int fd;
	struct stat sb;
	void *p_data;
	size_t size;
	int file_mmap_index = 0;

	if ((fd = open(filename, O_RDONLY)) < 0)
	{
		log_error("open(%s) error (%d)\n", filename, errno);
		return -1;
	}

	if (fstat(fd, &sb) < 0)
	{
		log_error("fstat(fd) error (%d)\n", errno);
		return -1;
	}

	size = (size_t)sb.st_size;

	p_data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0L);
	if (p_data == MAP_FAILED)
	{
		log_error("mmap() error (%d)\n", errno);
		return -2;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)\n", errno);
		return -1;
	}

	if (trie_dict_get(p_trie_file_dict, filename, (int64_t *)&file_mmap_index) == 0) // Not exist
	{
		if (file_mmap_free_index == -1)
		{
			log_error("file_mmap_pool is depleted\n");
			return -3;
		}

		file_mmap_index = file_mmap_free_index;

		if (trie_dict_set(p_trie_file_dict, filename, (int64_t)file_mmap_index) != 1)
		{
			log_error("trie_dict_set(%s) error\n", filename);

			if (munmap(p_data, size) < 0)
			{
				log_error("munmap() error (%d)\n", errno);
			}

			return -2;
		}

		do
		{
			file_mmap_free_index++;
			if (file_mmap_free_index >= file_mmap_count)
			{
				file_mmap_free_index = 0;
			}
			if (file_mmap_free_index == file_mmap_index) // loop
			{
				file_mmap_free_index = -1;
				break;
			}
		} while (p_file_mmap_pool[file_mmap_free_index].p_data != NULL);
	}
	else
	{
		// Unload existing data
		if (munmap(p_file_mmap_pool[file_mmap_index].p_data, p_file_mmap_pool[file_mmap_index].size) < 0)
		{
			log_error("munmap() error (%d)\n", errno);
		}
	}

	p_file_mmap_pool[file_mmap_index].p_data = p_data;
	p_file_mmap_pool[file_mmap_index].size = size;

	p_file_mmap_pool[file_mmap_index].line_total =
		split_data_lines(p_data, SCREEN_COLS, p_file_mmap_pool[file_mmap_index].line_offsets, MAX_FILE_LINES);

	return 0;
}

int unload_file_mmap(const char *filename)
{
	int file_mmap_index = 0;

	if (trie_dict_get(p_trie_file_dict, filename, (int64_t *)&file_mmap_index) != 1)
	{
		log_error("trie_dict_get(%s) not found\n", filename);
		return -1;
	}

	if (munmap(p_file_mmap_pool[file_mmap_index].p_data, p_file_mmap_pool[file_mmap_index].size) < 0)
	{
		log_error("munmap() error (%d)\n", errno);
	}

	p_file_mmap_pool[file_mmap_index].p_data = NULL;
	p_file_mmap_pool[file_mmap_index].size = 0L;

	if (file_mmap_free_index == -1)
	{
		file_mmap_free_index = file_mmap_index;
	}

	if (trie_dict_del(p_trie_file_dict, filename) != 1)
	{
		log_error("trie_dict_del(%s) error\n", filename);
		return -2;
	}

	return 0;
}

const FILE_MMAP *get_file_mmap(const char *filename)
{
	int file_mmap_index = file_mmap_free_index;

	if (p_file_mmap_pool == NULL || p_trie_file_dict == NULL)
	{
		log_error("File loader not initialized\n");
		return NULL;
	}

	if (trie_dict_get(p_trie_file_dict, filename, (int64_t *)&file_mmap_index) != 1) // Not exist
	{
		log_error("trie_dict_get(%s) not found\n", filename);
		return NULL;
	}

	return (&(p_file_mmap_pool[file_mmap_index]));
}
