/***************************************************************************
					   article_cache.c  -  description
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

#include "article_cache.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <strings.h>
#define _POSIX_C_SOURCE 200809L
#include <string.h>

#define ARTICLE_HEADER_MAX_LEN 4096

inline static int article_cache_path(char *file_path, size_t buf_len, const char *cache_dir, const ARTICLE *p_article)
{
	if (file_path == NULL || cache_dir == NULL || p_article == NULL)
	{
		log_error("article_cache_path() NULL pointer error\n");
		return -1;
	}

	snprintf(file_path, buf_len, "%s%d", cache_dir, p_article->cid);

	return 0;
}

int article_cache_generate(const char *cache_dir, const ARTICLE *p_article, const char *content, int overwrite)
{
	char data_file[FILE_PATH_LEN];
	int fd;
	ARTICLE_CACHE cache;
	size_t buf_size;

	if (cache_dir == NULL || p_article == NULL || content == NULL)
	{
		log_error("article_cache_generate() NULL pointer error\n");
		return -1;
	}

	if (article_cache_path(data_file, sizeof(data_file), cache_dir, p_article) < 0)
	{
		log_error("article_cache_path(dir=%s, cid=%d) error\n", cache_dir, p_article->cid);
		return -1;
	}

	if (!overwrite)
	{
		if ((fd = open(data_file, O_RDONLY)) != -1) // check data file
		{
			close(fd);
			return 0;
		}
	}

	buf_size = ARTICLE_HEADER_MAX_LEN + strlen(content) + 1;
	cache.p_data = malloc(buf_size);
	if (cache.p_data == NULL)
	{
		log_error("malloc(size=%ld) error OOM\n", buf_size);
		return -1;
	}

	if ((fd = open(data_file, O_WRONLY | O_CREAT | O_TRUNC, 0640)) == -1)
	{
		log_error("open(%s) error (%d)\n", data_file, errno);
		return -2;
	}

	// TODO: Generate article header
	cache.data_len = (size_t)snprintf(cache.p_data,
									  ARTICLE_HEADER_MAX_LEN + strlen(content) + 1,
									  "%s",
									  content);

	bzero(cache.line_offsets, sizeof(cache.line_offsets));
	cache.line_total = split_data_lines(cache.p_data, SCREEN_COLS, cache.line_offsets, MAX_SPLIT_FILE_LINES);
	if (cache.line_total >= MAX_SPLIT_FILE_LINES)
	{
		log_error("split_data_lines(%s) truncated over limit lines\n", data_file);
	}

	if (write(fd, &cache, sizeof(cache)) == -1)
	{
		log_error("write(%s, cache) error (%d)\n", data_file, errno);
		close(fd);
		return -3;
	}
	if (write(fd, cache.p_data, cache.data_len) == -1)
	{
		log_error("write(%s, data) error (%d)\n", data_file, errno);
		close(fd);
		return -3;
	}

	if (close(fd) == -1)
	{
		log_error("close(%s) error (%d)\n", data_file, errno);
		return -2;
	}

	free(cache.p_data);

	return 0;
}

int article_cache_load(ARTICLE_CACHE *p_cache, const char *cache_dir, const ARTICLE *p_article)
{
	char data_file[FILE_PATH_LEN];
	int fd;

	if (p_cache == NULL || cache_dir == NULL || p_article == NULL)
	{
		log_error("article_cache_load() NULL pointer error\n");
		return -1;
	}

	if (article_cache_path(data_file, sizeof(data_file), cache_dir, p_article) < 0)
	{
		log_error("article_cache_path(dir=%s, cid=%d) error\n", cache_dir, p_article->cid);
		return -1;
	}

	if ((fd = open(data_file, O_RDONLY)) == -1)
	{
		log_error("open(%s) error (%d)\n", data_file, errno);
		return -2;
	}

	if (read(fd, (void *)p_cache, sizeof(ARTICLE_CACHE)) == -1)
	{
		log_error("read(%s, cache) error (%d)\n", data_file, errno);
		close(fd);
		return -3;
	}

	p_cache->p_data = malloc(p_cache->data_len + 1);
	if (p_cache->p_data == NULL)
	{
		log_error("malloc(size=%ld) error OOM\n", p_cache->data_len + 1);
		close(fd);
		return -1;
	}

	if (read(fd, p_cache->p_data, p_cache->data_len) == -1)
	{
		log_error("read(%s, data) error (%d)\n", data_file, errno);
		close(fd);
		return -3;
	}
	p_cache->p_data[p_cache->data_len] = '\0';

	if (close(fd) == -1)
	{
		log_error("close(%s) error (%d)\n", data_file, errno);
		return -2;
	}

	return 0;
}

int article_cache_unload(ARTICLE_CACHE *p_cache)
{
	if (p_cache == NULL || p_cache->p_data == NULL)
	{
		log_error("article_cache_unload() NULL pointer error\n");
		return -1;
	}

	free(p_cache->p_data);
	p_cache->p_data = NULL;

	return 0;
}
