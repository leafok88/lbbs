/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * trie_dict
 *   - trie-tree based dict feature
 *
 * Copyright (C) 2004-2025 by Leaflet <leaflet@leafok.com>
 */

#include "log.h"
#include "trie_dict.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

struct trie_node_pool_t
{
	int shmid;
	int node_count_limit;
	int node_count;
	TRIE_NODE *p_node_free_list;
};
typedef struct trie_node_pool_t TRIE_NODE_POOL;

static TRIE_NODE_POOL *p_trie_node_pool;

int trie_dict_init(const char *filename, int node_count_limit)
{
	int shmid;
	int proj_id;
	key_t key;
	size_t size;
	void *p_shm;
	TRIE_NODE *p_trie_nodes;
	int i;

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
	proj_id = (int)(time(NULL) % getpid());
	key = ftok(filename, proj_id);
	if (key == -1)
	{
		log_error("ftok(%s, %d) error (%d)\n", filename, proj_id, errno);
		return -2;
	}

	size = sizeof(TRIE_NODE_POOL) + sizeof(TRIE_NODE) * (size_t)node_count_limit;
	shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
	if (shmid == -1)
	{
		log_error("shmget(size = %d) error (%d)\n", size, errno);
		return -3;
	}
	p_shm = shmat(shmid, NULL, 0);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(shmid = %d) error (%d)\n", shmid, errno);
		return -3;
	}

	p_trie_node_pool = p_shm;
	p_trie_node_pool->shmid = shmid;
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

void trie_dict_cleanup(void)
{
	int shmid;

	if (p_trie_node_pool == NULL)
	{
		return;
	}

	shmid = p_trie_node_pool->shmid;

	detach_trie_dict_shm();

	if (shmid != 0 && shmctl(shmid, IPC_RMID, NULL) == -1)
	{
		log_error("shmctl(shmid = %d, IPC_RMID) error (%d)\n", shmid, errno);
	}
}

int set_trie_dict_shm_readonly(void)
{
	int shmid;
	void *p_shm;

	if (p_trie_node_pool == NULL)
	{
		log_error("trie_dict_pool not initialized\n");
		return -1;
	}

	shmid = p_trie_node_pool->shmid;

	// Remap shared memory in read-only mode
	p_shm = shmat(shmid, p_trie_node_pool, SHM_RDONLY | SHM_REMAP);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(trie_node_pool shmid=%d) error (%d)\n", shmid, errno);
		return -1;
	}

	p_trie_node_pool = p_shm;

	return 0;
}

int detach_trie_dict_shm(void)
{
	if (p_trie_node_pool != NULL && shmdt(p_trie_node_pool) == -1)
	{
		log_error("shmdt(trie_node_pool) error (%d)\n", errno);
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
