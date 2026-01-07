/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * memory_pool
 *   - memory pool
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "log.h"
#include "memory_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Align size to max_align_t for proper memory alignment */
static inline size_t align_size(size_t size)
{
	size_t alignment = _Alignof(max_align_t);
	return (size + alignment - 1) & ~(alignment - 1);
}

MEMORY_POOL *memory_pool_init(size_t node_size, size_t node_count_per_chunk, int chunk_count_limit)
{
	MEMORY_POOL *p_pool;
	size_t aligned_node_size;

	if (node_size < sizeof(void *))
	{
		log_error("Error: node_size < sizeof(void *)");
		return NULL;
	}

	/* Align node_size for proper memory alignment */
	aligned_node_size = align_size(node_size);

	p_pool = malloc(sizeof(MEMORY_POOL));
	if (p_pool == NULL)
	{
		log_error("malloc(MEMORY_POOL) error: OOM");
		return NULL;
	}

	/* Initialize mutex */
	if (pthread_mutex_init(&p_pool->mutex, NULL) != 0)
	{
		log_error("pthread_mutex_init error");
		free(p_pool);
		return NULL;
	}

	p_pool->node_size = aligned_node_size;
	p_pool->node_count_per_chunk = node_count_per_chunk;
	p_pool->chunk_count_limit = chunk_count_limit;
	p_pool->chunk_count = 0;
	p_pool->p_free = NULL;

	p_pool->p_chunks = malloc(sizeof(void *) * (size_t)chunk_count_limit);
	if (p_pool->p_chunks == NULL)
	{
		log_error("malloc(sizeof(void *) * %d) error: OOM", chunk_count_limit);
		pthread_mutex_destroy(&p_pool->mutex);
		free(p_pool);
		return NULL;
	}

	p_pool->node_count_allocated = 0;
	p_pool->node_count_free = 0;
	p_pool->node_count_total = 0;

	return p_pool;
}

void memory_pool_cleanup(MEMORY_POOL *p_pool)
{
	if (p_pool == NULL)
	{
		return;
	}

	pthread_mutex_lock(&p_pool->mutex);

	if (p_pool->node_count_allocated > 0)
	{
		log_error("Still have %d in-use nodes", p_pool->node_count_allocated);
	}

	while (p_pool->chunk_count > 0)
	{
		(p_pool->chunk_count)--;
		free(p_pool->p_chunks[p_pool->chunk_count]);
	}

	free(p_pool->p_chunks);

	pthread_mutex_unlock(&p_pool->mutex);
	pthread_mutex_destroy(&p_pool->mutex);

	free(p_pool);
}

inline static void *memory_pool_add_chunk(MEMORY_POOL *p_pool)
{
	void *p_chunk;
	void *p_node;
	size_t i;
	size_t chunk_size;

	if (p_pool->chunk_count >= p_pool->chunk_count_limit)
	{
		log_error("Chunk count limit %d reached", p_pool->chunk_count);
		return NULL;
	}

	chunk_size = p_pool->node_size * p_pool->node_count_per_chunk;
	p_chunk = malloc(chunk_size);
	if (p_chunk == NULL)
	{
		log_error("malloc(%zu) error: OOM", chunk_size);
		return NULL;
	}

	p_node = p_pool->p_free;
	memcpy((char *)p_chunk + (p_pool->node_count_per_chunk - 1) * p_pool->node_size, &p_node, sizeof(p_node));
	for (i = 0; i < p_pool->node_count_per_chunk - 1; i++)
	{
		p_node = (char *)p_chunk + (i + 1) * p_pool->node_size;
		memcpy((char *)p_chunk + i * p_pool->node_size, &p_node, sizeof(p_node));
	}

	p_pool->p_chunks[p_pool->chunk_count] = p_chunk;
	(p_pool->chunk_count)++;
	p_pool->node_count_total += (int)p_pool->node_count_per_chunk;
	p_pool->node_count_free += (int)p_pool->node_count_per_chunk;

	p_pool->p_free = p_chunk;

	return p_chunk;
}

void *memory_pool_alloc(MEMORY_POOL *p_pool)
{
	void *p_node;

	if (p_pool == NULL)
	{
		log_error("NULL pointer error");
		return NULL;
	}

	pthread_mutex_lock(&p_pool->mutex);

	if (p_pool->p_free == NULL && memory_pool_add_chunk(p_pool) == NULL)
	{
		log_error("Add chunk error");
		pthread_mutex_unlock(&p_pool->mutex);
		return NULL;
	}

	p_node = p_pool->p_free;
	memcpy(&(p_pool->p_free), p_node, sizeof(p_pool->p_free));

	(p_pool->node_count_free)--;
	(p_pool->node_count_allocated)++;

	pthread_mutex_unlock(&p_pool->mutex);

	return p_node;
}

void memory_pool_free(MEMORY_POOL *p_pool, void *p_node)
{
	uint32_t *p_magic;

	if (p_pool == NULL)
	{
		log_error("NULL pointer error");
		return;
	}

	if (p_node == NULL)
	{
		log_error("Attempt to free NULL pointer");
		return;
	}

	pthread_mutex_lock(&p_pool->mutex);

	// For test and debug
#ifdef _DEBUG
	memory_pool_check_node(p_pool, p_node);
#endif

	/* Check for double-free using magic number at end of node */
	p_magic = (uint32_t *)((char *)p_node + p_pool->node_size - sizeof(uint32_t));
	if (*p_magic == MEMORY_POOL_MAGIC_FREE)
	{
		log_error("Double-free detected for node %p", p_node);
		pthread_mutex_unlock(&p_pool->mutex);
		return;
	}

	/* Mark node as free */
	*p_magic = MEMORY_POOL_MAGIC_FREE;

	memcpy(p_node, &(p_pool->p_free), sizeof(p_pool->p_free));
	p_pool->p_free = p_node;

	(p_pool->node_count_free)++;
	(p_pool->node_count_allocated)--;

	pthread_mutex_unlock(&p_pool->mutex);
}

int memory_pool_check_node(MEMORY_POOL *p_pool, void *p_node)
{
	size_t chunk_size;
	int i;

	if (p_pool == NULL || p_node == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	chunk_size = p_pool->node_size * p_pool->node_count_per_chunk;

	for (i = 0; i < p_pool->chunk_count; i++)
	{
		if (p_node >= p_pool->p_chunks[i] && (char *)p_node < (char *)(p_pool->p_chunks[i]) + chunk_size)
		{
			if ((size_t)((char *)p_node - (char *)(p_pool->p_chunks[i])) % p_pool->node_size == 0)
			{
				return 0; // OK
			}
			else
			{
				log_error("Address of node (%p) is not aligned with border of chunk %d [%p, %p)",
						  p_node, i, p_pool->p_chunks[i], (char *)(p_pool->p_chunks[i]) + chunk_size);
				return -3;
			}
		}
	}

	log_error("Address of node is not in range of chunks");
	return -2;
}
