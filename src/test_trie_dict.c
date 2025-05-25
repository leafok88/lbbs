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
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define TEST_VAL ((int64_t)(0xcb429a63a017661f)) // int64_t
#define TRIE_DICT_SHM_FILE "~trie_dict_shm.dat"

const char *keys[] = {
	"ABCDEFG",
	"abcdefg",
	"../$@^",
	"1234567890",
	"1234657890",
	"HZ²âÊÔºº×ÖHZ", // GBK Chinese might not be displayed in UTF-8 env
	"P3p4P3z4P_",
	"_bbBBz_Z_",
	""};

int keys_cnt = 8; // + 1 to trigger error handling of blank key

void test_traverse_cb(const char *key, int64_t value)
{
	printf("Traverse key: %s Len=%ld\n", key, strlen(key));
}

int main(int argc, char *argv[])
{
	TRIE_NODE *p_dict;
	int ret;
	int64_t value;
	FILE *fp;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_std_redirect(STDOUT_FILENO);
	log_err_redirect(STDERR_FILENO);

	if ((fp = fopen(TRIE_DICT_SHM_FILE, "w")) == NULL)
	{
		log_error("fopen(%s) error\n", TRIE_DICT_SHM_FILE);
		return -1;
	}
	fclose(fp);

	for (int i = 0; i < keys_cnt; i++)
	{
		printf("Check key %d [%s] len=%ld\n", i, keys[i], strlen(keys[i]));
	}

	if (trie_dict_init(TRIE_DICT_SHM_FILE) < 0)
	{
		printf("trie_dict_init failed\n");
		return -1;
	}

	printf("Testing #1 ...\n");

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

	trie_dict_traverse(p_dict, test_traverse_cb);

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

	trie_dict_traverse(p_dict, test_traverse_cb);

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

	printf("Testing #2 ...\n");

	for (int i = 0; i < 100000; i++)
	{
		p_dict = trie_dict_create();

		if (p_dict == NULL)
		{
			printf("OOM\n");
			return -1;
		}

		trie_dict_destroy(p_dict);
		p_dict = NULL;
	}

	printf("Testing #3 ...\n");

	if (set_trie_dict_shm_readonly() < 0)
	{
		printf("load_trie_dict_shm_readonly() error\n");
	}

	// if (unload_trie_dict_shm() < 0)
	// {
	// 	printf("unload_trie_dict_shm() error\n");
	// }

	trie_dict_cleanup();

	printf("Done\n");

	if (unlink(TRIE_DICT_SHM_FILE) < 0)
	{
		log_error("unlink(%s) error\n", TRIE_DICT_SHM_FILE);
		return -1;
	}

	log_end();

	return 0;
}
