/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_loader
 *   - shared memory based file loader
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "file_loader.h"
#include "log.h"
#include "str_process.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

struct shm_header_t
{
	size_t shm_size;
	size_t data_len;
	long line_total;
};

int load_file(const char *filename)
{
	char filepath[FILE_PATH_LEN];
	char shm_name[FILE_NAME_LEN];
	int fd;
	struct stat sb;
	void *p_data;
	size_t data_len;
	size_t size;
	void *p_shm;
	long line_total;
	long line_offsets[MAX_SPLIT_FILE_LINES];
	long *p_line_offsets;

	if (filename == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	if ((fd = open(filename, O_RDONLY)) < 0)
	{
		log_error("open(%s) error (%d)", filename, errno);
		return -1;
	}

	if (fstat(fd, &sb) < 0)
	{
		log_error("fstat(fd) error (%d)", errno);
		close(fd);
		return -1;
	}

	data_len = (size_t)sb.st_size;
	p_data = mmap(NULL, data_len, PROT_READ, MAP_SHARED, fd, 0L);
	if (p_data == MAP_FAILED)
	{
		log_error("mmap() error (%d)", errno);
		close(fd);
		return -2;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)", errno);
		return -1;
	}

	line_total = split_data_lines(p_data, SCREEN_COLS, line_offsets, MAX_SPLIT_FILE_LINES, 1, NULL);

	// Allocate shared memory
	size = sizeof(struct shm_header_t) + data_len + 1 + sizeof(long) * (size_t)(line_total + 1);

	strncpy(filepath, filename, sizeof(filepath) - 1);
	filepath[sizeof(filepath) - 1] = '\0';
	snprintf(shm_name, sizeof(shm_name), "/FILE_SHM_%s", basename(filepath));

	if (shm_unlink(shm_name) == -1 && errno != ENOENT)
	{
		log_error("shm_unlink(%s) error (%d)", shm_name, errno);
		return -2;
	}

	if ((fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600)) == -1)
	{
		log_error("shm_open(%s) error (%d)", shm_name, errno);
		return -2;
	}
	if (ftruncate(fd, (off_t)size) == -1)
	{
		log_error("ftruncate(size=%d) error (%d)", size, errno);
		close(fd);
		return -2;
	}

	p_shm = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0L);
	if (p_shm == MAP_FAILED)
	{
		log_error("mmap() error (%d)", errno);
		close(fd);
		return -2;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)", errno);
		return -1;
	}

	((struct shm_header_t *)p_shm)->shm_size = size;
	((struct shm_header_t *)p_shm)->data_len = data_len;
	((struct shm_header_t *)p_shm)->line_total = line_total;
	memcpy((char *)p_shm + sizeof(struct shm_header_t), p_data, data_len);

	if (munmap(p_data, data_len) < 0)
	{
		log_error("munmap() error (%d)", errno);
		munmap(p_shm, size);
		return -2;
	}

	p_data = (char *)p_shm + sizeof(struct shm_header_t);
	p_line_offsets = (long *)((char *)p_data + data_len + 1);
	memcpy(p_line_offsets, line_offsets, sizeof(long) * (size_t)(line_total + 1));

	if (munmap(p_shm, size) < 0)
	{
		log_error("munmap() error (%d)", errno);
		return -2;
	}

	return 0;
}

int unload_file(const char *filename)
{
	char filepath[FILE_PATH_LEN];
	char shm_name[FILE_NAME_LEN];

	if (filename == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	strncpy(filepath, filename, sizeof(filepath) - 1);
	filepath[sizeof(filepath) - 1] = '\0';
	snprintf(shm_name, sizeof(shm_name), "/FILE_SHM_%s", basename(filepath));

	if (shm_unlink(shm_name) == -1 && errno != ENOENT)
	{
		log_error("shm_unlink(%s) error (%d)", shm_name, errno);
		return -2;
	}

	return 0;
}

void *get_file_shm_readonly(const char *filename, size_t *p_data_len, long *p_line_total, const void **pp_data, const long **pp_line_offsets)
{
	char filepath[FILE_PATH_LEN];
	char shm_name[FILE_NAME_LEN];
	int fd;
	void *p_shm = NULL;
	struct stat sb;
	size_t size;

	if (filename == NULL || p_data_len == NULL || p_line_total == NULL || pp_data == NULL || pp_line_offsets == NULL)
	{
		log_error("NULL pointer error");
		return NULL;
	}

	strncpy(filepath, filename, sizeof(filepath) - 1);
	filepath[sizeof(filepath) - 1] = '\0';
	snprintf(shm_name, sizeof(shm_name), "/FILE_SHM_%s", basename(filepath));

	if ((fd = shm_open(shm_name, O_RDONLY, 0600)) == -1)
	{
		log_error("shm_open(%s) error (%d)", shm_name, errno);
		return NULL;
	}

	if (fstat(fd, &sb) < 0)
	{
		log_error("fstat(fd) error (%d)", errno);
		close(fd);
		return NULL;
	}

	size = (size_t)sb.st_size;

	p_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0L);
	if (p_shm == MAP_FAILED)
	{
		log_error("mmap() error (%d)", errno);
		close(fd);
		return NULL;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)", errno);
		return NULL;
	}

	if (((struct shm_header_t *)p_shm)->shm_size != size)
	{
		log_error("Shared memory size mismatch (%ld != %ld)", ((struct shm_header_t *)p_shm)->shm_size, size);
		munmap(p_shm, size);
		return NULL;
	}

	*p_data_len = ((struct shm_header_t *)p_shm)->data_len;
	*p_line_total = ((struct shm_header_t *)p_shm)->line_total;
	*pp_data = (char *)p_shm + sizeof(struct shm_header_t);
	*pp_line_offsets = (const long *)((const char *)(*pp_data) + *p_data_len + 1);

	return p_shm;
}

int detach_file_shm(void *p_shm)
{
	size_t size;

	if (p_shm == NULL)
	{
		return -1;
	}

	size = ((struct shm_header_t *)p_shm)->shm_size;

	if (munmap(p_shm, size) < 0)
	{
		log_error("munmap() error (%d)", errno);
		return -2;
	}

	return 0;
}
