/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * trie_dict
 *   - trie-tree based dict feature
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _TRIE_DICT_H_
#define _TRIE_DICT_H_

#include <stdint.h>

#define TRIE_CHILDREN 256
#define TRIE_MAX_KEY_LEN 1023
#define TRIE_NODE_PER_POOL 5000

struct trie_node_t
{
	int64_t values[TRIE_CHILDREN];
	int8_t flags[TRIE_CHILDREN];
	struct trie_node_t *p_nodes[TRIE_CHILDREN];
};
typedef struct trie_node_t TRIE_NODE;

typedef void (*trie_dict_traverse_cb)(const char *, int64_t);

extern int trie_dict_init(const char *filename, int node_count_limit);
extern void trie_dict_cleanup(void);

extern int set_trie_dict_shm_readonly(void);
extern int detach_trie_dict_shm(void);

extern TRIE_NODE *trie_dict_create(void);
extern void trie_dict_destroy(TRIE_NODE *p_dict);

extern int trie_dict_set(TRIE_NODE *p_dict, const char *key, int64_t value);
extern int trie_dict_get(TRIE_NODE *p_dict, const char *key, int64_t *p_value);
extern int trie_dict_del(TRIE_NODE *p_dict, const char *key);

extern void trie_dict_traverse(TRIE_NODE *p_dict, trie_dict_traverse_cb cb);

extern int trie_dict_used_nodes(void);

#endif //_TRIE_DICT_H_
