/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * trie_dict
 *   - trie-tree based dict feature
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "log.h"
#include "trie_dict.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

struct trie_node_pool_t
{
	size_t shm_size;
	int node_count_limit;
	int node_count;
	TRIE_NODE *p_node_free_list;
};
typedef struct trie_node_pool_t TRIE_NODE_POOL;

static char trie_node_shm_name[FILE_PATH_LEN];
static TRIE_NODE_POOL *p_trie_node_pool;

int trie_dict_init(const char *filename, int node_count_limit)
{
	char filepath[FILE_PATH_LEN];
	int fd;
	size_t size;
	void *p_shm;
	TRIE_NODE *p_trie_nodes;
	int i;

	if (filename == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_trie_node_pool != NULL)
	{
		log_error("trie_dict_pool already initialized\n");
		return -1;
	}

	if (node_count_limit <= 0 || node_count_limit > TRIE_NODE_PER_POOL)
	{
		log_error("trie_dict_init(%d) error: invalid node_count_limit\n", node_count_limit);
		return -1;
	}

	// Allocate shared memory
	size = sizeof(TRIE_NODE_POOL) + sizeof(TRIE_NODE) * (size_t)node_count_limit;

	strncpy(filepath, filename, sizeof(filepath) - 1);
	filepath[sizeof(filepath) - 1] = '\0';
	snprintf(trie_node_shm_name, sizeof(trie_node_shm_name), "/TRIE_DICT_SHM_%s", basename(filepath));

	if (shm_unlink(trie_node_shm_name) == -1 && errno != ENOENT)
	{
		log_error("shm_unlink(%s) error (%d)\n", trie_node_shm_name, errno);
		return -2;
	}

	if ((fd = shm_open(trie_node_shm_name, O_CREAT | O_EXCL | O_RDWR, 0600)) == -1)
	{
		log_error("shm_open(%s) error (%d)\n", trie_node_shm_name, errno);
		return -2;
	}
	if (ftruncate(fd, (off_t)size) == -1)
	{
		log_error("ftruncate(size=%d) error (%d)\n", size, errno);
		close(fd);
		return -2;
	}

	p_shm = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0L);
	if (p_shm == MAP_FAILED)
	{
		log_error("mmap() error (%d)\n", errno);
		close(fd);
		return -2;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)\n", errno);
		return -1;
	}

	p_trie_node_pool = p_shm;
	p_trie_node_pool->shm_size = size;
	p_trie_node_pool->node_count_limit = node_count_limit;
	p_trie_node_pool->node_count = 0;

	p_trie_nodes = (TRIE_NODE *)((char *)p_shm + sizeof(TRIE_NODE_POOL));
	p_trie_node_pool->p_node_free_list = &(p_trie_nodes[0]);
	for (i = 0; i < node_count_limit - 1; i++)
	{
		p_trie_nodes[i].p_nodes[0] = &(p_trie_nodes[i + 1]);
	}
	p_trie_nodes[node_count_limit - 1].p_nodes[0] = NULL;

	return 0;
}

int trie_dict_cleanup(void)
{
	if (p_trie_node_pool == NULL)
	{
		return -1;
	}

	detach_trie_dict_shm();

	if (shm_unlink(trie_node_shm_name) == -1 && errno != ENOENT)
	{
		log_error("shm_unlink(%s) error (%d)\n", trie_node_shm_name, errno);
		return -2;
	}

	trie_node_shm_name[0] = '\0';

	return 0;
}

int get_trie_dict_shm_readonly(void)
{
	int fd;
	struct stat sb;
	size_t size;
	void *p_shm;

	if ((fd = shm_open(trie_node_shm_name, O_RDONLY, 0600)) == -1)
	{
		log_error("shm_open(%s) error (%d)\n", trie_node_shm_name, errno);
		return -2;
	}

	if (fstat(fd, &sb) < 0)
	{
		log_error("fstat(fd) error (%d)\n", errno);
		close(fd);
		return -2;
	}

	size = (size_t)sb.st_size;

	p_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0L);
	if (p_shm == MAP_FAILED)
	{
		log_error("mmap() error (%d)\n", errno);
		close(fd);
		return -2;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)\n", errno);
		return -1;
	}

	if (p_trie_node_pool->shm_size != size)
	{
		log_error("Shared memory size mismatch (%ld != %ld)\n", p_trie_node_pool->shm_size, size);
		munmap(p_shm, size);
		return -3;
	}

	p_trie_node_pool = p_shm;

	return 0;
}

int set_trie_dict_shm_readonly(void)
{
	if (p_trie_node_pool != NULL && munmap(p_trie_node_pool, p_trie_node_pool->shm_size) < 0)
	{
		log_error("munmap() error (%d)\n", errno);
		return -2;
	}

	if (get_trie_dict_shm_readonly() < 0)
	{
		log_error("get_trie_dict_shm_readonly() error\n");
		return -3;
	}

	return 0;
}

int detach_trie_dict_shm(void)
{
	if (p_trie_node_pool != NULL && munmap(p_trie_node_pool, p_trie_node_pool->shm_size) < 0)
	{
		log_error("munmap() error (%d)\n", errno);
		return -1;
	}

	p_trie_node_pool = NULL;

	return 0;
}

TRIE_NODE *trie_dict_create(void)
{
	TRIE_NODE *p_dict = NULL;

	if (p_trie_node_pool != NULL && p_trie_node_pool->p_node_free_list != NULL)
	{
		p_dict = p_trie_node_pool->p_node_free_list;
		p_trie_node_pool->p_node_free_list = p_dict->p_nodes[0];

		memset(p_dict, 0, sizeof(*p_dict));

		p_trie_node_pool->node_count++;
	}
	else if (p_trie_node_pool != NULL)
	{
		log_error("trie_dict_create() error: node depleted %d >= %d\n", p_trie_node_pool->node_count, p_trie_node_pool->node_count_limit);
	}

	return p_dict;
}

void trie_dict_destroy(TRIE_NODE *p_dict)
{
	if (p_trie_node_pool == NULL || p_dict == NULL)
	{
		return;
	}

	for (int i = 0; i < TRIE_CHILDREN; i++)
	{
		if (p_dict->p_nodes[i] != NULL)
		{
			trie_dict_destroy(p_dict->p_nodes[i]);
		}
	}

	memset(p_dict, 0, sizeof(*p_dict));

	p_dict->p_nodes[0] = p_trie_node_pool->p_node_free_list;
	p_trie_node_pool->p_node_free_list = p_dict;

	p_trie_node_pool->node_count--;
}

int trie_dict_set(TRIE_NODE *p_dict, const char *key, int64_t value)
{
	int offset;

	if (p_dict == NULL)
	{
		return -1;
	}

	while (key != NULL && *key != '\0')
	{
		offset = (256 + *key) % 256;
		if (offset < 0 || offset >= TRIE_CHILDREN) // incorrect key character
		{
			return -1;
		}

		if (*(key + 1) == '\0')
		{
			if (p_dict->flags[offset] == 0 || p_dict->values[offset] != value)
			{
				p_dict->values[offset] = value;
				p_dict->flags[offset] = 1;
				return 1; // Set to new value
			}
			return 0; // Unchanged
		}
		else
		{
			if (p_dict->p_nodes[offset] == NULL)
			{
				if ((p_dict->p_nodes[offset] = trie_dict_create()) == NULL)
				{
					return -2; // OOM
				}
			}
			p_dict = p_dict->p_nodes[offset];
			key++;
		}
	}

	return -1; // NULL key
}

int trie_dict_get(TRIE_NODE *p_dict, const char *key, int64_t *p_value)
{
	int offset;

	if (p_dict == NULL)
	{
		return -1;
	}

	while (key != NULL && *key != '\0')
	{
		offset = (256 + *key) % 256;
		if (offset < 0 || offset >= TRIE_CHILDREN) // incorrect key character
		{
			return -1;
		}

		if (*(key + 1) == '\0')
		{
			if (p_dict->flags[offset] != 0)
			{
				*p_value = p_dict->values[offset];
				return 1; // Found
			}
			else
			{
				return 0; // Not exist
			}
		}
		else if (p_dict->p_nodes[offset] == NULL)
		{
			return 0; // Not exist
		}
		else
		{
			p_dict = p_dict->p_nodes[offset];
			key++;
		}
	}

	return -1; // NULL key
}

int trie_dict_del(TRIE_NODE *p_dict, const char *key)
{
	int offset;

	if (p_dict == NULL)
	{
		return -1;
	}

	while (key != NULL && *key != '\0')
	{
		offset = (256 + *key) % 256;
		if (offset < 0 || offset >= TRIE_CHILDREN) // incorrect key character
		{
			return -1;
		}

		if (*(key + 1) == '\0')
		{
			if (p_dict->flags[offset] != 0)
			{
				p_dict->flags[offset] = 0;
				p_dict->values[offset] = 0;
				return 1; // Done
			}
			else
			{
				return 0; // Not exist
			}
		}
		else if (p_dict->p_nodes[offset] == NULL)
		{
			return 0; // Not exist
		}
		else
		{
			p_dict = p_dict->p_nodes[offset];
			key++;
		}
	}

	return -1; // NULL key
}

static void _trie_dict_traverse(TRIE_NODE *p_dict, trie_dict_traverse_cb cb, char *key, int depth)
{
	if (p_dict == NULL || depth >= TRIE_MAX_KEY_LEN)
	{
		return;
	}

	for (int i = 0; i < TRIE_CHILDREN; i++)
	{
		if (p_dict->flags[i] != 0)
		{
			key[depth] = (char)i;
			key[depth + 1] = '\0';
			(*cb)(key, p_dict->values[i]);
		}

		if (p_dict->p_nodes[i] != NULL && depth + 1 < TRIE_MAX_KEY_LEN)
		{
			key[depth] = (char)i;
			_trie_dict_traverse(p_dict->p_nodes[i], cb, key, depth + 1);
		}
	}
}

void trie_dict_traverse(TRIE_NODE *p_dict, trie_dict_traverse_cb cb)
{
	char key[TRIE_MAX_KEY_LEN + 1];

	if (p_dict == NULL)
	{
		return;
	}

	_trie_dict_traverse(p_dict, cb, key, 0);
}

int trie_dict_used_nodes(void)
{
	return (p_trie_node_pool == NULL ? -1 : p_trie_node_pool->node_count);
}
