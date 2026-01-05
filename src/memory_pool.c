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

MEMORY_POOL *memory_pool_init(size_t node_size, size_t node_count_per_chunk, int chunk_count_limit)
{
	MEMORY_POOL *p_pool;

	if (node_size < sizeof(void *))
	{
		log_error("Error: node_size < sizeof(void *)");
		return NULL;
	}

	p_pool = malloc(sizeof(MEMORY_POOL));
	if (p_pool == NULL)
	{
		log_error("malloc(MEMORY_POOL) error: OOM");
		return NULL;
	}

	p_pool->node_size = node_size;
	p_pool->node_count_per_chunk = node_count_per_chunk;
	p_pool->chunk_count_limit = chunk_count_limit;
	p_pool->chunk_count = 0;
	p_pool->p_free = NULL;

	p_pool->p_chunks = malloc(sizeof(void *) * (size_t)chunk_count_limit);
	if (p_pool->p_chunks == NULL)
	{
		log_error("malloc(sizeof(void *) * %d) error: OOM", chunk_count_limit);
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
	free(p_pool);
}

inline static void *memory_pool_add_chunk(MEMORY_POOL *p_pool)
{
	void *p_chunk;
	void *p_node;
	size_t i;

	if (p_pool->chunk_count >= p_pool->chunk_count_limit)
	{
		log_error("Chunk count limit %d reached", p_pool->chunk_count);
		return NULL;
	}
	p_chunk = malloc(p_pool->node_size * p_pool->node_count_per_chunk);
	if (p_chunk == NULL)
	{
		log_error("malloc(%d * %d) error: OOM", p_pool->node_size, p_pool->node_count_per_chunk);
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

	if (p_pool->p_free == NULL && memory_pool_add_chunk(p_pool) == NULL)
	{
		log_error("Add chunk error");
		return NULL;
	}

	p_node = p_pool->p_free;
	memcpy(&(p_pool->p_free), p_node, sizeof(p_pool->p_free));

	(p_pool->node_count_free)--;
	(p_pool->node_count_allocated)++;

	return p_node;
}

void memory_pool_free(MEMORY_POOL *p_pool, void *p_node)
{
	if (p_pool == NULL)
	{
		log_error("NULL pointer error");
		return;
	}

	// For test and debug
#ifdef _DEBUG
	memory_pool_check_node(p_pool, p_node);
#endif

	memcpy(p_node, &(p_pool->p_free), sizeof(p_pool->p_free));
	p_pool->p_free = p_node;

	(p_pool->node_count_free)++;
	(p_pool->node_count_allocated)--;
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
						  i, p_node >= p_pool->p_chunks[i], (char *)(p_pool->p_chunks[i]) + chunk_size);
				return -3;
			}
		}
	}

	log_error("Address of node is not in range of chunks");
	return -2;
}
