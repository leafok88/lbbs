/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * article_cache
 *   - convert article content from DB to local cache with LML conversion and line offset index
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "article_cache.h"
#include "lml.h"
#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

enum _article_cache_constant_t
{
	ARTICLE_HEADER_MAX_LEN = 4096,
	ARTICLE_FOOTER_MAX_LEN = 4096,
	SUB_DT_MAX_LEN = 50,
};

static const char *BBS_article_footer_color[] = {
	"\033[31m",
	"\033[32m",
	"\033[33m",
	"\033[34m",
	"\033[35m",
	"\033[36m",
	"\033[37m",
};
static const int BBS_article_footer_color_count = 7;

static char *content_f; // static buffer in large size

inline static int article_cache_path(char *file_path, size_t buf_len, const char *cache_dir, const ARTICLE *p_article)
{
	if (file_path == NULL || cache_dir == NULL || p_article == NULL)
	{
		log_error("NULL pointer error\n");
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
	size_t body_len;
	long body_line_cnt;
	char footer[ARTICLE_FOOTER_MAX_LEN];
	size_t footer_len;
	long footer_line_cnt;
	long i;

	if (cache_dir == NULL || p_article == NULL || content == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (content_f == NULL && (content_f = malloc(ARTICLE_CONTENT_MAX_LEN)) == NULL)
	{
		log_error("malloc(content_f) error: OOM\n");
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

	memset(&cache, 0, sizeof(cache));

	// Generate article header / footer
	localtime_r(&(p_article->sub_dt), &tm_sub_dt);
	strftime(str_sub_dt, sizeof(str_sub_dt), "%c %Z", &tm_sub_dt);

	snprintf(header, sizeof(header), "发布者: %s (%s), 版块: %s (%s)\n标  题: %s\n发布于: %s (%s)\n\n",
			 p_article->username, p_article->nickname, p_section->sname, p_section->stitle, p_article->title, BBS_name, str_sub_dt);

	snprintf(footer, sizeof(footer),
			 "\n--\n%s※ 来源: %s https://%s [FROM: %s]\033[m\n\n",
			 BBS_article_footer_color[p_article->aid % BBS_article_footer_color_count],
			 BBS_name, BBS_server, sub_ip);

	header_len = strnlen(header, sizeof(header));
	footer_len = strnlen(footer, sizeof(footer));

	header_line_cnt = split_data_lines(header, SCREEN_COLS, cache.line_offsets,
									   MAX_SPLIT_FILE_LINES, 1, NULL);

	if (header_len != cache.line_offsets[header_line_cnt])
	{
#ifdef _DEBUG
		log_error("Header of article(aid=%d) is truncated from %ld to %ld\n, body_line=%ld, body_line_limit=%ld",
				  p_article->aid, header_len, cache.line_offsets[header_line_cnt],
				  header_line_cnt, MAX_SPLIT_FILE_LINES);
#endif
		header_len = (size_t)cache.line_offsets[header_line_cnt];
	}

	// Apply LML render to content body
	body_len = (size_t)lml_render(content, content_f, ARTICLE_CONTENT_MAX_LEN, SCREEN_COLS, 0);
	cache.data_len = header_len + body_len;

	body_line_cnt = split_data_lines(content_f, SCREEN_COLS + 1, &(cache.line_offsets[header_line_cnt]),
									 MAX_SPLIT_FILE_LINES - header_line_cnt, 1, NULL);
	cache.line_total = header_line_cnt + body_line_cnt;

	if (body_len != (size_t)cache.line_offsets[cache.line_total])
	{
#ifdef _DEBUG
		log_error("Body of article(aid=%d) is truncated from %ld to %ld, body_line=%ld, body_line_limit=%ld\n",
				  p_article->aid, body_len, cache.line_offsets[cache.line_total],
				  body_line_cnt, MAX_SPLIT_FILE_LINES - header_line_cnt);
#endif
		cache.data_len = header_len + (size_t)(cache.line_offsets[cache.line_total]);
	}

	for (i = header_line_cnt; i <= cache.line_total; i++)
	{
		cache.line_offsets[i] += (long)header_len;
	}

	footer_line_cnt = split_data_lines(footer, SCREEN_COLS, &(cache.line_offsets[cache.line_total]),
									   MAX_SPLIT_FILE_LINES - cache.line_total, 1, NULL);

	if (footer_len != cache.line_offsets[cache.line_total + footer_line_cnt])
	{
#ifdef _DEBUG
		log_error("Footer of article(aid=%d) is truncated from %ld to %ld, footer_line=%ld, footer_line_limit=%ld\n",
				  p_article->aid, footer_len, cache.line_offsets[cache.line_total + footer_line_cnt],
				  footer_line_cnt, MAX_SPLIT_FILE_LINES - cache.line_total);
#endif
		footer_len = (size_t)(cache.line_offsets[cache.line_total + footer_line_cnt]);
	}

	for (i = 0; i <= footer_line_cnt; i++)
	{
		cache.line_offsets[cache.line_total + i] += (long)cache.data_len;
	}

	cache.data_len += footer_len;
	cache.line_total += footer_line_cnt;
	cache.mmap_len = sizeof(cache) - sizeof(long) * (MAX_SPLIT_FILE_LINES - (size_t)(cache.line_total) - 1) + cache.data_len;

	if (write(fd, &cache, cache.mmap_len - cache.data_len) == -1)
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
	if (write(fd, content_f, cache.data_len - header_len - footer_len) == -1)
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

void article_cache_cleanup(void)
{
	if (content_f)
	{
		free(content_f);
		content_f = NULL;
	}
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
		log_error("NULL pointer error\n");
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

	if (((ARTICLE_CACHE *)p_mmap)->mmap_len != mmap_len)
	{
		log_error("Inconsistent mmap len of article (cid=%d), %ld != %ld\n", p_article->cid, p_cache->mmap_len, mmap_len);
		return -3;
	}

	memset(p_cache, 0, sizeof(*p_cache));
	memcpy((void *)p_cache, p_mmap, (size_t)(((ARTICLE_CACHE *)p_mmap)->mmap_len - ((ARTICLE_CACHE *)p_mmap)->data_len));

	p_cache->p_mmap = p_mmap;
	p_cache->p_data = (char *)p_mmap + (p_cache->mmap_len - p_cache->data_len);

	return 0;
}

int article_cache_unload(ARTICLE_CACHE *p_cache)
{
	if (p_cache == NULL || p_cache->p_mmap == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (munmap(p_cache->p_mmap, p_cache->mmap_len) < 0)
	{
		log_error("munmap() error (%d)\n", errno);
		return -2;
	}

	p_cache->p_mmap = NULL;
	p_cache->p_data = NULL;

	return 0;
}
