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
#define ARTICLE_FOOTER_MAX_LEN 4096
#define SUB_DT_MAX_LEN 50

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

int article_cache_generate(const char *cache_dir, const ARTICLE *p_article, const SECTION_LIST *p_section,
						   const char *content, const char *sub_ip, int overwrite)
{
	char data_file[FILE_PATH_LEN];
	int fd;
	ARTICLE_CACHE cache;
	struct tm tm_sub_dt;
	char str_sub_dt[SUB_DT_MAX_LEN + 1];
	char header[ARTICLE_HEADER_MAX_LEN];
	size_t header_len;
	long header_line_cnt;
	char footer[ARTICLE_FOOTER_MAX_LEN];
	size_t footer_len;
	long footer_line_cnt;
	long i;

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

	if ((fd = open(data_file, O_WRONLY | O_CREAT | O_TRUNC, 0640)) == -1)
	{
		log_error("open(%s) error (%d)\n", data_file, errno);
		return -2;
	}

	bzero(&cache, sizeof(cache));

	// Generate article header / footer
	localtime_r(&(p_article->sub_dt), &tm_sub_dt);
	strftime(str_sub_dt, sizeof(str_sub_dt), "%c", &tm_sub_dt);

	snprintf(header, sizeof(header), "发布者: %s (%s), 版块: %s (%s)\n标  题: %s\n发布于: %s (%s)\n\n",
			 p_article->username, p_article->nickname, p_section->sname, p_section->stitle, p_article->title, BBS_name, str_sub_dt);

	snprintf(footer, sizeof(footer),
			 "--\n※ 来源: %s https://%s [FROM: %s]\n",
			 BBS_name, BBS_server, sub_ip);

	header_len = strnlen(header, sizeof(header));
	footer_len = strnlen(footer, sizeof(footer));

	cache.data_len = header_len + strlen(content);

	header_line_cnt = split_data_lines(header, SCREEN_COLS, cache.line_offsets, MAX_SPLIT_FILE_LINES);
	cache.line_total = header_line_cnt +
					   split_data_lines(content, SCREEN_COLS, cache.line_offsets + header_line_cnt, MAX_SPLIT_FILE_LINES - header_line_cnt);
	if (cache.line_total >= MAX_SPLIT_FILE_LINES)
	{
		log_error("split_data_lines(%s) truncated over limit lines %d >= %d\n", data_file, cache.line_total, MAX_SPLIT_FILE_LINES);
		return -3;
	}

	for (i = header_line_cnt; i <= cache.line_total; i++)
	{
		cache.line_offsets[i] += (long)header_len;
	}

	footer_line_cnt = split_data_lines(footer, SCREEN_COLS, cache.line_offsets + cache.line_total, MAX_SPLIT_FILE_LINES - cache.line_total);
	if (cache.line_total + footer_line_cnt >= MAX_SPLIT_FILE_LINES)
	{
		log_error("split_data_lines(%s) truncated over limit lines %d >= %d\n", data_file, cache.line_total + footer_line_cnt, MAX_SPLIT_FILE_LINES);
		return -3;
	}

	for (i = 0; i <= footer_line_cnt; i++)
	{
		cache.line_offsets[cache.line_total + i] += (long)cache.data_len;
	}

	cache.data_len += footer_len;
	cache.line_total += footer_line_cnt;

	if (write(fd, &cache, sizeof(cache)) == -1)
	{
		log_error("write(%s, cache) error (%d)\n", data_file, errno);
		close(fd);
		return -3;
	}
	if (write(fd, header, header_len) == -1)
	{
		log_error("write(%s, header) error (%d)\n", data_file, errno);
		close(fd);
		return -3;
	}
	if (write(fd, content, cache.data_len - header_len - footer_len) == -1)
	{
		log_error("write(%s, content) error (%d)\n", data_file, errno);
		close(fd);
		return -3;
	}
	if (write(fd, footer, footer_len) == -1)
	{
		log_error("write(%s, footer) error (%d)\n", data_file, errno);
		close(fd);
		return -3;
	}

	if (close(fd) == -1)
	{
		log_error("close(%s) error (%d)\n", data_file, errno);
		return -2;
	}

	return 0;
}

int article_cache_load(ARTICLE_CACHE *p_cache, const char *cache_dir, const ARTICLE *p_article)
{
	char data_file[FILE_PATH_LEN];
	int fd;
	struct stat sb;
	void *p_mmap;
	size_t mmap_len;

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

	if (fstat(fd, &sb) < 0)
	{
		log_error("fstat(fd) error (%d)\n", errno);
		return -2;
	}

	mmap_len = (size_t)sb.st_size;

	p_mmap = mmap(NULL, mmap_len, PROT_READ, MAP_SHARED, fd, 0L);
	if (p_mmap == MAP_FAILED)
	{
		log_error("mmap(%s) error (%d)\n", data_file, errno);
		return -2;
	}

	if (close(fd) == -1)
	{
		log_error("close(%s) error (%d)\n", data_file, errno);
		return -2;
	}

	memcpy((void *)p_cache, p_mmap, sizeof(ARTICLE_CACHE));

	if (sizeof(ARTICLE_CACHE) + p_cache->data_len != mmap_len)
	{
		log_error("Inconsistent length, cache_len(%ld) + data_len(%ld) != mmap_len(%ld)\n",
				  sizeof(ARTICLE_CACHE), p_cache->data_len, mmap_len);

		if (munmap(p_mmap, mmap_len) < 0)
		{
			log_error("munmap() error (%d)\n", errno);
		}

		return -3;
	}

	p_cache->p_mmap = p_mmap;
	p_cache->p_data = p_mmap + sizeof(ARTICLE_CACHE);

	return 0;
}

int article_cache_unload(ARTICLE_CACHE *p_cache)
{
	if (p_cache == NULL || p_cache->p_mmap == NULL)
	{
		log_error("article_cache_unload() NULL pointer error\n");
		return -1;
	}

	if (munmap(p_cache->p_mmap, sizeof(ARTICLE_CACHE) + p_cache->data_len) < 0)
	{
		log_error("munmap() error (%d)\n", errno);
		return -2;
	}

	p_cache->p_mmap = NULL;
	p_cache->p_data = NULL;

	return 0;
}
