/***************************************************************************
						 trie_dict.h  -  description
							 -------------------
	Copyright            : (C) 2004-2025 by Leaflet
	Email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _TRIE_DICT_H_
#define _TRIE_DICT_H_

#include <stdint.h>

#define TRIE_CHILDREN 256
#define TRIE_MAX_KEY_LEN 1023

struct trie_node_t
{
	int64_t values[TRIE_CHILDREN];
	int8_t flags[TRIE_CHILDREN];
	struct trie_node_t *p_nodes[TRIE_CHILDREN];
};
typedef struct trie_node_t TRIE_NODE;

typedef void (*trie_dict_traverse_cb)(const char *, int64_t);

extern TRIE_NODE *trie_dict_create(void);
extern void trie_dict_destroy(TRIE_NODE *p_dict);

extern int trie_dict_set(TRIE_NODE *p_dict, const char *key, int64_t value);
extern int trie_dict_get(TRIE_NODE *p_dict, const char *key, int64_t *p_value);
extern int trie_dict_del(TRIE_NODE *p_dict, const char *key);

extern void trie_dict_traverse(TRIE_NODE *p_dict, trie_dict_traverse_cb cb);

#endif //_TRIE_DICT_H_
