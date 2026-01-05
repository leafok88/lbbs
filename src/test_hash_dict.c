/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * test_hash_dict
 *   - tester for hash dict
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hash_dict.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	HASH_DICT *p_dict;
	uint64_t key;
	int64_t value;
	unsigned int i;
	int ret;
	const int dict_item_count = 10000 * 10000;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_common_redir(STDOUT_FILENO);
	log_error_redir(STDERR_FILENO);

	p_dict = hash_dict_create(dict_item_count);
	if (p_dict == NULL)
	{
		printf("hash_dict_create() error\n");
		return -2;
	}

	for (i = 0; i < dict_item_count; i++)
	{
		key = i * 37 + 13;
		ret = hash_dict_get(p_dict, key, &value);
		if (ret != 0)
		{
			printf("hash_dict_get(%lu) ret=%d error\n", key, ret);
			break;
		}
	}

	for (i = 0; i < dict_item_count; i++)
	{
		key = i * 37 + 13;
		if (i % 2 == 1)
		{
			if (hash_dict_inc(p_dict, key, i * 3 + 7) != 0)
			{
				printf("hash_dict_inc(%lu) error\n", key);
				break;
			}
		}

		if (hash_dict_set(p_dict, key, i * 3 + 7) != 0)
		{
			printf("hash_dict_set(%lu) error\n", key);
			break;
		}
	}

	ret = (int)hash_dict_item_count(p_dict);
	if (ret != dict_item_count)
	{
		printf("hash_dict_item_count()=%d error\n", ret);
	}

	for (i = 0; i < dict_item_count; i++)
	{
		key = i * 37 + 13;
		ret = hash_dict_get(p_dict, key, &value);
		if (ret != 1)
		{
			printf("hash_dict_get(%lu) ret=%d error\n", key, ret);
			break;
		}
		if (value != i * 3 + 7)
		{
			printf("hash_dict_get(%lu) value=%ld error\n", key, value);
			break;
		}
	}

	for (i = 0; i < dict_item_count; i++)
	{
		key = i * 37 + 13;
		if (hash_dict_inc(p_dict, key, i * 5 + 17) != 1)
		{
			printf("hash_dict_inc(%lu) error\n", key);
			break;
		}
	}

	for (i = 0; i < dict_item_count; i++)
	{
		key = i * 37 + 13;
		ret = hash_dict_get(p_dict, key, &value);
		if (ret != 1)
		{
			printf("hash_dict_get(%lu) ret=%d error\n", key, ret);
			break;
		}
		if (value != i * 3 + 7 + i * 5 + 17)
		{
			printf("hash_dict_get(%lu) value=%ld error\n", key, value);
			break;
		}
	}

	ret = (int)hash_dict_item_count(p_dict);
	if (ret != dict_item_count)
	{
		printf("hash_dict_item_count()=%d error\n", ret);
	}

	for (i = 0; i < dict_item_count; i++)
	{
		key = i * 37 + 13;
		if (hash_dict_del(p_dict, key) != 1)
		{
			printf("hash_dict_del(%lu) error\n", key);
			break;
		}
	}

	ret = (int)hash_dict_item_count(p_dict);
	if (ret != 0)
	{
		printf("hash_dict_item_count()=%d error\n", ret);
	}

	for (i = 0; i < dict_item_count; i++)
	{
		key = i * 37 + 13;
		ret = hash_dict_get(p_dict, key, &value);
		if (ret != 0)
		{
			printf("hash_dict_get(%lu) ret=%d error\n", key, ret);
			break;
		}
	}

	for (i = 0; i < dict_item_count; i++)
	{
		key = i * 37 + 13;
		if (hash_dict_del(p_dict, key) != 0)
		{
			printf("hash_dict_del(%lu) error\n", key);
			break;
		}
	}

	hash_dict_destroy(p_dict);

	log_end();

	return 0;
}
