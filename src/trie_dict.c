/***************************************************************************
						 trie_dict.c  -  description
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

#include "trie_dict.h"
#include <stdlib.h>
#include <stdio.h>

TRIE_NODE *trie_dict_create(void)
{
	TRIE_NODE *p_dict;

	p_dict = (TRIE_NODE *)calloc(1, sizeof(TRIE_NODE));

	return p_dict;
}

void trie_dict_destroy(TRIE_NODE *p_dict)
{
	if (p_dict == NULL)
	{
		return;
	}

	for (int i = 0; i < TRIE_CHILDREN; i++)
	{
		if (p_dict->p_nodes[i] != NULL)
		{
			trie_dict_destroy(p_dict->p_nodes[i]);
			p_dict->p_nodes[i] = NULL;
		}

		p_dict->flags[i] = 0;
	}

	free(p_dict);
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
		offset = *key;
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
		offset = *key;
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
		offset = *key;
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
