/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * hash_dict
 *   - hash-map based dict feature
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _HASH_DICT_H_
#define _HASH_DICT_H_

#include "memory_pool.h"
#include <stdint.h>

enum hash_dict_constant_t
{
	HASH_DICT_BUCKET_SIZE = 65536,
	HASH_DICT_BUCKET_MAX_COUNT = 1024,
};

struct hash_item_t
{
	uint64_t key;
	int64_t value;
	struct hash_item_t *p_next;
};
typedef struct hash_item_t HASH_ITEM;

struct hash_dict_t
{
	HASH_ITEM **buckets[HASH_DICT_BUCKET_MAX_COUNT];
	unsigned int bucket_count;
	unsigned int prime_index;
	MEMORY_POOL *p_item_pool;
	unsigned int item_count;
};
typedef struct hash_dict_t HASH_DICT;

extern HASH_DICT *hash_dict_create(int item_count_limit);
extern void hash_dict_destroy(HASH_DICT *p_dict);

extern int hash_dict_set(HASH_DICT *p_dict, uint64_t key, int64_t value);
extern int hash_dict_get(HASH_DICT *p_dict, uint64_t key, int64_t *p_value);
extern int hash_dict_del(HASH_DICT *p_dict, uint64_t key);

inline unsigned int hash_dict_item_count(HASH_DICT *p_dict)
{
	if (p_dict == NULL)
	{
		return 0;
	}
	return p_dict->item_count;
}

#endif //_HASH_DICT_H_
