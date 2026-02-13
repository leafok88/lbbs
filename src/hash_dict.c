/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * hash_dict
 *   - hash-map based dict feature
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hash_dict.h"
#include "log.h"
#include <stdlib.h>

enum _hash_dict_constant_t
{
	MP_NODE_COUNT_PER_CHUNK = 1000,
};

// HASH_DICT_BUCKET_SIZE * bucket_count should be a prime number
static const unsigned int hash_dict_prime_list[] = {
	49157,
	98317,
	196613,
	393241,
	786433,
	1572869,
	3145739,
	6291469,
	12582917,
	25165843,
	50331653,
};

static const unsigned int hash_dict_prime_list_count = sizeof(hash_dict_prime_list) / sizeof(hash_dict_prime_list[0]);

// External definition for inline function
extern inline unsigned int hash_dict_item_count(HASH_DICT *p_dict);

HASH_DICT *hash_dict_create(int item_count_limit)
{
	HASH_DICT *p_dict = NULL;

	if (item_count_limit <= 0)
	{
		log_error("Invalid item_count_limit(%d)<=0", item_count_limit);
		return NULL;
	}

	p_dict = (HASH_DICT *)malloc(sizeof(HASH_DICT));
	if (p_dict == NULL)
	{
		log_error("malloc(HASH_DICT) error");
		return NULL;
	}

	p_dict->prime_index = hash_dict_prime_list_count - 1;
	for (unsigned int i = 0; i < hash_dict_prime_list_count; i++)
	{
		if (item_count_limit < hash_dict_prime_list[i])
		{
			p_dict->prime_index = i;
			break;
		}
	}

	p_dict->bucket_count = (hash_dict_prime_list[p_dict->prime_index] + HASH_DICT_BUCKET_SIZE - 1) / HASH_DICT_BUCKET_SIZE;

	p_dict->p_item_pool = memory_pool_init(sizeof(HASH_ITEM), MP_NODE_COUNT_PER_CHUNK, item_count_limit / MP_NODE_COUNT_PER_CHUNK + 1);
	if (p_dict->p_item_pool == NULL)
	{
		log_error("memory_pool_init(HASH_ITEM) error");
		free(p_dict);
		return NULL;
	}

	for (unsigned int i = 0; i < p_dict->bucket_count; i++)
	{
		p_dict->buckets[i] = calloc(HASH_DICT_BUCKET_SIZE, sizeof(HASH_ITEM *));
		if (p_dict->buckets[i] == NULL)
		{
			log_error("calloc(HASH_DICT_BUCKET_SIZE, HASH_ITEM) error at bucket %d", i);
			p_dict->bucket_count = i;
			hash_dict_destroy(p_dict);
			return NULL;
		}
	}

	p_dict->item_count = 0;

	return p_dict;
}

void hash_dict_destroy(HASH_DICT *p_dict)
{
	HASH_ITEM *p_item;
	HASH_ITEM *p_next;

	if (p_dict == NULL)
	{
		return;
	}

	for (unsigned int i = 0; i < p_dict->bucket_count; i++)
	{
		for (unsigned int j = 0; j < HASH_DICT_BUCKET_SIZE; j++)
		{
			p_item = p_dict->buckets[i][j];
			while (p_item != NULL)
			{
				p_next = p_item->p_next;
				memory_pool_free(p_dict->p_item_pool, p_item);
				p_item = p_next;
			}
		}
		free(p_dict->buckets[i]);
	}

	memory_pool_cleanup(p_dict->p_item_pool);
	free(p_dict);
}

int hash_dict_set(HASH_DICT *p_dict, uint64_t key, int64_t value)
{
	uint64_t bucket_index;
	uint64_t item_index_in_bucket;
	HASH_ITEM *p_item;

	if (p_dict == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	bucket_index = (key % (HASH_DICT_BUCKET_SIZE * p_dict->bucket_count)) / HASH_DICT_BUCKET_SIZE;
	item_index_in_bucket = key % HASH_DICT_BUCKET_SIZE;

	p_item = p_dict->buckets[bucket_index][item_index_in_bucket];
	while (p_item != NULL)
	{
		if (p_item->key == key)
		{
			p_item->value = value;
			return 1;
		}
		p_item = p_item->p_next;
	}

	p_item = (HASH_ITEM *)memory_pool_alloc(p_dict->p_item_pool);
	if (p_item == NULL)
	{
		log_error("memory_pool_alloc(HASH_ITEM) error");
		return -1;
	}

	p_item->key = key;
	p_item->value = value;
	p_item->p_next = p_dict->buckets[bucket_index][item_index_in_bucket];
	p_dict->buckets[bucket_index][item_index_in_bucket] = p_item;

	(p_dict->item_count)++;

	return 0;
}

int hash_dict_inc(HASH_DICT *p_dict, uint64_t key, int64_t value_inc)
{
	uint64_t bucket_index;
	uint64_t item_index_in_bucket;
	HASH_ITEM *p_item;

	if (p_dict == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	bucket_index = (key % (HASH_DICT_BUCKET_SIZE * p_dict->bucket_count)) / HASH_DICT_BUCKET_SIZE;
	item_index_in_bucket = key % HASH_DICT_BUCKET_SIZE;

	p_item = p_dict->buckets[bucket_index][item_index_in_bucket];
	while (p_item != NULL)
	{
		if (p_item->key == key)
		{
			p_item->value += value_inc;
			return 1;
		}
		p_item = p_item->p_next;
	}

	return 0;
}

int hash_dict_get(HASH_DICT *p_dict, uint64_t key, int64_t *p_value)
{
	uint64_t bucket_index;
	uint64_t item_index_in_bucket;
	HASH_ITEM *p_item;

	if (p_dict == NULL || p_value == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	bucket_index = (key % (HASH_DICT_BUCKET_SIZE * p_dict->bucket_count)) / HASH_DICT_BUCKET_SIZE;
	item_index_in_bucket = key % HASH_DICT_BUCKET_SIZE;

	p_item = p_dict->buckets[bucket_index][item_index_in_bucket];
	while (p_item != NULL)
	{
		if (p_item->key == key)
		{
			*p_value = p_item->value;
			return 1;
		}
		p_item = p_item->p_next;
	}

	return 0;
}

int hash_dict_del(HASH_DICT *p_dict, uint64_t key)
{
	uint64_t bucket_index;
	uint64_t item_index_in_bucket;
	HASH_ITEM *p_item;
	HASH_ITEM *p_prior;

	if (p_dict == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	bucket_index = (key % (HASH_DICT_BUCKET_SIZE * p_dict->bucket_count)) / HASH_DICT_BUCKET_SIZE;
	item_index_in_bucket = key % HASH_DICT_BUCKET_SIZE;

	p_item = p_dict->buckets[bucket_index][item_index_in_bucket];
	p_prior = NULL;
	while (p_item != NULL)
	{
		if (p_item->key == key)
		{
			if (p_prior == NULL)
			{
				p_dict->buckets[bucket_index][item_index_in_bucket] = p_item->p_next;
			}
			else
			{
				p_prior->p_next = p_item->p_next;
			}
			memory_pool_free(p_dict->p_item_pool, p_item);
			(p_dict->item_count)--;
			return 1;
		}
		p_prior = p_item;
		p_item = p_item->p_next;
	}

	return 0;
}
