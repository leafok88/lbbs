/***************************************************************************
					  test_trie_dict.c  -  description
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
#include <stdio.h>

#define TEST_VAL ((int64_t)(0xcb429a63a017661f)) // int64_t

const char *keys[] = {
	"ABCDEFG",
	"abcdefg",
	"1234567890",
	"P3p4P3z4P_",
	"_bbBBz_Z_",
	""};

int keys_cnt = 6;

int main(int argc, char *argv[])
{
	TRIE_NODE *p_dict;
	int ret;
	int64_t value;

	p_dict = trie_dict_create();

	if (p_dict == NULL)
	{
		printf("OOM\n");
		return -1;
	}

	ret = trie_dict_set(p_dict, NULL, TEST_VAL);
	if (ret != -1)
	{
		printf("Set NULL key error (%d)\n", ret);
	}

	ret = trie_dict_del(p_dict, NULL);
	if (ret != -1)
	{
		printf("Del NULL key error (%d)\n", ret);
	}

	for (int i = 0; i < keys_cnt; i++)
	{
		ret = trie_dict_set(p_dict, keys[i], TEST_VAL >> i);
		if (ret != 1)
		{
			printf("Set non-existing [%s] error (%d)\n", keys[i], ret);
		}
	}

	for (int i = 0; i < keys_cnt; i++)
	{
		ret = trie_dict_get(p_dict, keys[i], &value);
		if (ret != 1)
		{
			printf("Get [%s] error (%d)\n", keys[i], ret);
		}
		else if (value != TEST_VAL >> i)
		{
			printf("Value of [%s] is incorrect (%ld != %ld)\n", keys[i], value, TEST_VAL >> i);
		}
	}

	for (int i = 0; i < keys_cnt; i++)
	{
		if (i % 2 == 0)
		{
			ret = trie_dict_del(p_dict, keys[i]);
			if (ret != 1)
			{
				printf("Del existing [%s] error (%d)\n", keys[i], ret);
			}
		}
		else
		{
			ret = trie_dict_set(p_dict, keys[i], TEST_VAL >> i);
			if (ret != 0)
			{
				printf("Set existing [%s] with the same value error (%d)\n", keys[i], ret);
			}
		}
	}

	for (int i = 0; i < keys_cnt; i++)
	{
		if (i % 2 == 0)
		{
			ret = trie_dict_get(p_dict, keys[i], &value);
			if (ret != 0)
			{
				printf("Get non-existing [%s] error (%d)\n", keys[i], ret);
			}
		}
		else
		{
			ret = trie_dict_get(p_dict, keys[i], &value);
			if (ret != 1)
			{
				printf("Get [%s] error (%d)\n", keys[i], ret);
			}
			else if (value != TEST_VAL >> i)
			{
				printf("Value of [%s] is incorrect (%ld != %ld)\n", keys[i], value, TEST_VAL >> i);
			}
		}
	}

	for (int i = 0; i < keys_cnt; i++)
	{
		if (i % 2 == 0)
		{
			ret = trie_dict_del(p_dict, keys[i]);
			if (ret != 0)
			{
				printf("Del non-existing [%s] error (%d)\n", keys[i], ret);
			}
		}
		else
		{
			ret = trie_dict_set(p_dict, keys[i], TEST_VAL << i);
			if (ret != 1)
			{
				printf("Set existing [%s] with different value error (%d)\n", keys[i], ret);
			}
		}
	}

	for (int i = 0; i < keys_cnt; i++)
	{
		if (i % 2 == 0)
		{
			ret = trie_dict_get(p_dict, keys[i], &value);
			if (ret != 0)
			{
				printf("Get non-existing [%s] error (%d)\n", keys[i], ret);
			}
		}
		else
		{
			ret = trie_dict_get(p_dict, keys[i], &value);
			if (ret != 1)
			{
				printf("Get [%s] error (%d)\n", keys[i], ret);
			}
			else if (value != TEST_VAL << i)
			{
				printf("Value of [%s] is incorrect (%ld != %ld)\n", keys[i], value, TEST_VAL << i);
			}
		}
	}

	trie_dict_destroy(p_dict);
	p_dict = NULL;

	printf("Done\n");

	return 0;
}
