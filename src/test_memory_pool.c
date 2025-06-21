/***************************************************************************
					test_memory_pool.c  -  description
							 -------------------
	copyright            : (C) 2004-2025 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "log.h"
#include "memory_pool.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define NODE_SIZE 1023
#define NODE_PER_CHUNK 1000
#define CHUNK_COUNT_LIMIT 100

int main(int argc, char *argv[])
{
	MEMORY_POOL *p_pool;
	void *p_nodes[NODE_PER_CHUNK * CHUNK_COUNT_LIMIT];
	int i;
	int j;
	int k;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_common_redir(STDOUT_FILENO);
	log_error_redir(STDERR_FILENO);

	p_pool = memory_pool_init(NODE_SIZE, NODE_PER_CHUNK, CHUNK_COUNT_LIMIT);
	if (p_pool == NULL)
	{
		return -2;
	}

	for (j = 0; j < 3; j++)
	{
		printf("Testing times #%d ...\n", j + 1);

		for (i = 0; i < NODE_PER_CHUNK * CHUNK_COUNT_LIMIT; i++)
		{
			if (j == 0 && i % NODE_PER_CHUNK == 0)
			{
				printf("Allocating nodes, total=%d, allocated=%d, free=%d\n", p_pool->node_count_total, p_pool->node_count_allocated, p_pool->node_count_free);
			}

			p_nodes[i] = memory_pool_alloc(p_pool);

			if (memory_pool_check_node(p_pool, p_nodes[i]) != 0)
			{
				printf("Error of node %d address\n", i);
			}
		}

		printf("Allocate completed, total=%d, allocated=%d, free=%d\n", p_pool->node_count_total, p_pool->node_count_allocated, p_pool->node_count_free);

		if (memory_pool_alloc(p_pool) != NULL)
		{
			printf("Allocate node error, expected is NULL\n");
		}

		for (i = 0; i < NODE_PER_CHUNK * CHUNK_COUNT_LIMIT; i++)
		{
			if (memory_pool_check_node(p_pool, p_nodes[i]) != 0)
			{
				printf("Error of node %d address\n", i);
			}

			if (j > 0)
			{
				for (k = (int)sizeof(void *); k < NODE_SIZE; k++)
				{
					if ((*(char *)(p_nodes[i] + k)) != 'A' + j - 1)
					{
						printf("Value of node[%d] at offset %d not equal to value set %c\n",
							   i, k, 'A' + j - 1);
					}
				}
			}
		}

		for (i = 0; i < NODE_PER_CHUNK * CHUNK_COUNT_LIMIT; i++)
		{
			if (j == 0 && i % NODE_PER_CHUNK == 0)
			{
				printf("Free nodes, total=%d, allocated=%d, free=%d\n", p_pool->node_count_total, p_pool->node_count_allocated, p_pool->node_count_free);
			}

			memset(p_nodes[i], 'A' + j, NODE_SIZE);

			memory_pool_free(p_pool, p_nodes[i]);
		}

		printf("Free completed, total=%d, allocated=%d, free=%d\n", p_pool->node_count_total, p_pool->node_count_allocated, p_pool->node_count_free);
	}

	memory_pool_cleanup(p_pool);

	log_end();

	return 0;
}
