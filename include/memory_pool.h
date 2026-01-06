/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * memory_pool
 *   - memory pool
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifndef _MEMORY_POOL_H_
#define _MEMORY_POOL_H_

#include <stddef.h>
#include <pthread.h>

/* Magic numbers for double-free detection */
#define MEMORY_POOL_MAGIC_ALLOCATED 0xABCD1234U
#define MEMORY_POOL_MAGIC_FREE      0xDEADBEEFU

struct memory_pool_t
{
	size_t node_size;
	size_t node_count_per_chunk;
	int chunk_count;
	int chunk_count_limit;
	void **p_chunks;
	void *p_free;
	int node_count_allocated;
	int node_count_free;
	int node_count_total;
	pthread_mutex_t mutex;
};
typedef struct memory_pool_t MEMORY_POOL;

extern MEMORY_POOL *memory_pool_init(size_t node_size, size_t node_count_per_chunk, int chunk_count_limit);
extern void memory_pool_cleanup(MEMORY_POOL *p_pool);

extern void *memory_pool_alloc(MEMORY_POOL *p_pool);
extern void memory_pool_free(MEMORY_POOL *p_pool, void *p_node);

// For debug only
extern int memory_pool_check_node(MEMORY_POOL *p_pool, void *p_node);

#endif //_MEMORY_POOL_H_
