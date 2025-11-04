/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_loader
 *   - shared memory based file loader
 *
 * Copyright (C) 2004-2025 by Leaflet <leaflet@leafok.com>
 */

#include "file_loader.h"
#include "log.h"
#include "str_process.h"
#include "trie_dict.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>

struct shm_header_t
{
	int shmid;
	size_t data_len;
	long line_total;
};

static TRIE_NODE *p_trie_file_dict = NULL;

int file_loader_init()
{
	if (p_trie_file_dict != NULL)
	{
		log_error("File loader already initialized\n");
		return -1;
	}

	p_trie_file_dict = trie_dict_create();
	if (p_trie_file_dict == NULL)
	{
		log_error("trie_dict_create() error\n");
		return -2;
	}

	return 0;
}

static void trie_file_dict_cleanup_cb(const char *filename, int64_t value)
{
	int shmid = (int)value;

	if (shmid != 0 && shmctl(shmid, IPC_RMID, NULL) == -1)
	{
		log_error("shmctl(shmid=%d, IPC_RMID) error (%d)\n", shmid, errno);
	}
}

void file_loader_cleanup(void)
{
	if (p_trie_file_dict == NULL)
	{
		return;
	}

	trie_dict_traverse(p_trie_file_dict, trie_file_dict_cleanup_cb);
	trie_dict_destroy(p_trie_file_dict);

	p_trie_file_dict = NULL;
}

int load_file(const char *filename)
{
	int fd;
	struct stat sb;
	void *p_data;
	size_t data_len;
	int proj_id;
	key_t key;
	size_t size;
	int shmid;
	void *p_shm;
	long line_total;
	long line_offsets[MAX_SPLIT_FILE_LINES];
	long *p_line_offsets;
	int64_t shmid_old;

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

	data_len = (size_t)sb.st_size;
	p_data = mmap(NULL, data_len, PROT_READ, MAP_SHARED, fd, 0L);
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

	line_total = split_data_lines(p_data, SCREEN_COLS, line_offsets, MAX_SPLIT_FILE_LINES, 1, NULL);

	// Allocate shared memory
	proj_id = (int)(time(NULL) % getpid());
	key = ftok(filename, proj_id);
	if (key == -1)
	{
		log_error("ftok(%s %d) error (%d)\n", filename, proj_id, errno);
		return -2;
	}

	size = sizeof(struct shm_header_t) + data_len + 1 + sizeof(long) * (size_t)(line_total + 1);
	shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
	if (shmid == -1)
	{
		log_error("shmget(size = %d) error (%d)\n", size, errno);
		return -3;
	}
	p_shm = shmat(shmid, NULL, 0);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(shmid=%d) error (%d)\n", shmid, errno);
		return -3;
	}

	((struct shm_header_t *)p_shm)->shmid = shmid;
	((struct shm_header_t *)p_shm)->data_len = data_len;
	((struct shm_header_t *)p_shm)->line_total = line_total;
	memcpy((char *)p_shm + sizeof(struct shm_header_t), p_data, data_len);

	if (munmap(p_data, data_len) < 0)
	{
		log_error("munmap() error (%d)\n", errno);
		return -2;
	}

	p_data = (char *)p_shm + sizeof(struct shm_header_t);
	p_line_offsets = (long *)((char *)p_data + data_len + 1);
	memcpy(p_line_offsets, line_offsets, sizeof(long) * (size_t)(line_total + 1));

	if (shmdt(p_shm) == -1)
	{
		log_error("shmdt(shmid=%d) error (%d)\n", shmid, errno);
		return -3;
	}

	if (trie_dict_get(p_trie_file_dict, filename, &shmid_old) == 1)
	{
		if (shmctl((int)shmid_old, IPC_RMID, NULL) == -1)
		{
			log_error("shmctl(shmid=%d, IPC_RMID) error (%d)\n", (int)shmid_old, errno);
			return -2;
		}
	}

	if (trie_dict_set(p_trie_file_dict, filename, (int64_t)shmid) != 1)
	{
		log_error("trie_dict_set(%s) error\n", filename);

		if (shmid != 0 && shmctl(shmid, IPC_RMID, NULL) == -1)
		{
			log_error("shmctl(shmid=%d, IPC_RMID) error (%d)\n", shmid, errno);
		}

		return -4;
	}

	return 0;
}

int unload_file(const char *filename)
{
	int64_t shmid;

	if (trie_dict_get(p_trie_file_dict, filename, &shmid) != 1)
	{
		log_error("trie_dict_get(%s) not found\n", filename);
		return -1;
	}

	if (shmctl((int)shmid, IPC_RMID, NULL) == -1)
	{
		log_error("shmctl(shmid=%d, IPC_RMID) error (%d)\n", (int)shmid, errno);
	}

	if (trie_dict_del(p_trie_file_dict, filename) != 1)
	{
		log_error("trie_dict_del(%s) error\n", filename);
		return -2;
	}

	return 0;
}

const void *get_file_shm_readonly(const char *filename, size_t *p_data_len, long *p_line_total, const void **pp_data, const long **pp_line_offsets)
{
	int64_t shmid;
	const void *p_shm;

	if (p_trie_file_dict == NULL)
	{
		log_error("File loader not initialized\n");
		return NULL;
	}

	if (trie_dict_get(p_trie_file_dict, filename, &shmid) != 1) // Not exist
	{
		log_error("trie_dict_get(%s) not found\n", filename);
		return NULL;
	}

	p_shm = shmat((int)shmid, NULL, SHM_RDONLY);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(shmid=%d) error (%d)\n", (int)shmid, errno);
		return NULL;
	}

	*p_data_len = ((struct shm_header_t *)p_shm)->data_len;
	*p_line_total = ((struct shm_header_t *)p_shm)->line_total;
	*pp_data = (char *)p_shm + sizeof(struct shm_header_t);
	*pp_line_offsets = (const long *)((const char *)(*pp_data) + *p_data_len + 1);

	return p_shm;
}

int detach_file_shm(const void *p_shm)
{
	if (p_shm == NULL)
	{
		return -2;
	}

	if (shmdt(p_shm) == -1)
	{
		log_error("shmdt() error (%d)\n", errno);
		return -1;
	}

	return 0;
}
