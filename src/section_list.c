/***************************************************************************
					   section_list.c  -  description
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

#include "section_list.h"
#include "log.h"
#include "trie_dict.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#define ARTICLE_BLOCK_PER_SHM 400		  // sizeof(ARTICLE_BLOCK) * ARTICLE_BLOCK_PER_SHM is the size of each shm segment to allocate
#define ARTICLE_BLOCK_SHM_COUNT_LIMIT 256 // limited by length (8-bit) of proj_id in ftok(path, proj_id)
#define ARTICLE_BLOCK_PER_POOL (ARTICLE_BLOCK_PER_SHM * ARTICLE_BLOCK_SHM_COUNT_LIMIT)

#define CALCULATE_PAGE_THRESHOLD 100 // Adjust to tune performance of move topic

#define SID_STR_LEN 5 // 32-bit + NULL

struct article_block_t
{
	ARTICLE articles[ARTICLE_PER_BLOCK];
	int32_t article_count;
	struct article_block_t *p_next_block;
};
typedef struct article_block_t ARTICLE_BLOCK;

struct article_block_shm_t
{
	int shmid;
	void *p_shm;
};
typedef struct article_block_shm_t ARTICLE_BLOCK_SHM;

struct article_block_pool_t
{
	ARTICLE_BLOCK_SHM shm_pool[ARTICLE_BLOCK_SHM_COUNT_LIMIT];
	int shm_count;
	ARTICLE_BLOCK *p_block_free_list;
	ARTICLE_BLOCK *p_block[ARTICLE_BLOCK_PER_POOL];
	int32_t block_count;
};
typedef struct article_block_pool_t ARTICLE_BLOCK_POOL;

static ARTICLE_BLOCK_POOL *p_article_block_pool = NULL;

static int section_list_pool_shmid;
static SECTION_LIST *p_section_list_pool = NULL;
static int section_list_count = 0;
static TRIE_NODE *p_trie_dict_section_by_name = NULL;
static TRIE_NODE *p_trie_dict_section_by_sid = NULL;

int article_block_init(const char *filename, int block_count)
{
	int shmid;
	int proj_id;
	key_t key;
	size_t size;
	void *p_shm;
	int i;
	int block_count_in_shm;
	ARTICLE_BLOCK *p_block_in_shm;
	ARTICLE_BLOCK **pp_block_next;

	if (p_article_block_pool != NULL)
	{
		log_error("article_block_pool already initialized\n");
		return -1;
	}

	if (block_count > ARTICLE_BLOCK_PER_POOL)
	{
		log_error("article_block_count exceed limit %d\n", ARTICLE_BLOCK_PER_POOL);
		return -2;
	}

	p_article_block_pool = calloc(1, sizeof(ARTICLE_BLOCK_POOL));
	if (p_article_block_pool == NULL)
	{
		log_error("calloc(ARTICLE_BLOCK_POOL) OOM\n");
		return -2;
	}

	// Allocate shared memory
	p_article_block_pool->shm_count = 0;
	pp_block_next = &(p_article_block_pool->p_block_free_list);

	while (block_count > 0)
	{
		block_count_in_shm = MIN(block_count, ARTICLE_BLOCK_PER_SHM);
		block_count -= block_count_in_shm;

		proj_id = getpid() + p_article_block_pool->shm_count;
		key = ftok(filename, proj_id);
		if (key == -1)
		{
			log_error("ftok(%s, %d) error (%d)\n", filename, proj_id, errno);
			article_block_cleanup();
			return -3;
		}

		size = sizeof(shmid) + sizeof(ARTICLE_BLOCK) * (size_t)block_count_in_shm;
		shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
		if (shmid == -1)
		{
			log_error("shmget(shm_index = %d, size = %d) error (%d)\n", p_article_block_pool->shm_count, size, errno);
			article_block_cleanup();
			return -3;
		}
		p_shm = shmat(shmid, NULL, 0);
		if (p_shm == (void *)-1)
		{
			log_error("shmat(shmid = %d) error (%d)\n", shmid, errno);
			article_block_cleanup();
			return -3;
		}

		(p_article_block_pool->shm_pool + p_article_block_pool->shm_count)->shmid = shmid;
		(p_article_block_pool->shm_pool + p_article_block_pool->shm_count)->p_shm = p_shm;
		p_article_block_pool->shm_count++;

		p_block_in_shm = p_shm;
		*pp_block_next = p_block_in_shm;

		for (i = 0; i < block_count_in_shm; i++)
		{
			if (i < block_count_in_shm - 1)
			{
				(p_block_in_shm + i)->p_next_block = (p_block_in_shm + i + 1);
			}
			else
			{
				(p_block_in_shm + i)->p_next_block = NULL;
				pp_block_next = &((p_block_in_shm + i)->p_next_block);
			}
		}
	}

	p_article_block_pool->block_count = 0;

	return 0;
}

void article_block_cleanup(void)
{
	if (p_article_block_pool != NULL)
	{
		for (int i = 0; i < p_article_block_pool->shm_count; i++)
		{
			if (shmdt((p_article_block_pool->shm_pool + i)->p_shm) == -1)
			{
				log_error("shmdt(shmid = %d) error (%d)\n", (p_article_block_pool->shm_pool + i)->shmid, errno);
			}

			if (shmctl((p_article_block_pool->shm_pool + i)->shmid, IPC_RMID, NULL) == -1)
			{
				log_error("shmctl(shmid = %d, IPC_RMID) error (%d)\n", (p_article_block_pool->shm_pool + i)->shmid, errno);
			}
		}

		free(p_article_block_pool);
		p_article_block_pool = NULL;
	}
}

inline static ARTICLE_BLOCK *pop_free_article_block(void)
{
	ARTICLE_BLOCK *p_block = NULL;

	if (p_article_block_pool->p_block_free_list != NULL)
	{
		p_block = p_article_block_pool->p_block_free_list;
		p_article_block_pool->p_block_free_list = p_block->p_next_block;
		p_block->p_next_block = NULL;
		p_block->article_count = 0;
	}

	return p_block;
}

inline static void push_free_article_block(ARTICLE_BLOCK *p_block)
{
	p_block->p_next_block = p_article_block_pool->p_block_free_list;
	p_article_block_pool->p_block_free_list = p_block;
}

int article_block_reset(void)
{
	ARTICLE_BLOCK *p_block;

	if (p_article_block_pool == NULL)
	{
		log_error("article_block_pool not initialized\n");
		return -1;
	}

	while (p_article_block_pool->block_count > 0)
	{
		p_article_block_pool->block_count--;
		p_block = p_article_block_pool->p_block[p_article_block_pool->block_count];
		push_free_article_block(p_block);
	}

	return 0;
}

ARTICLE *article_block_find_by_aid(int32_t aid)
{
	ARTICLE_BLOCK *p_block;
	int left;
	int right;
	int mid;

	if (p_article_block_pool == NULL)
	{
		log_error("article_block_pool not initialized\n");
		return NULL;
	}

	if (p_article_block_pool->block_count == 0) // empty
	{
		return NULL;
	}

	left = 0;
	right = p_article_block_pool->block_count;

	// aid in the range [ head aid of blocks[left], tail aid of blocks[right - 1] ]
	while (left < right - 1)
	{
		// get block offset no less than mid value of left and right block offsets
		mid = (left + right) / 2 + (right - left) % 2;

		if (mid >= p_article_block_pool->block_count)
		{
			log_error("block(mid = %d) is out of boundary\n", mid);
			return NULL;
		}

		if (aid < p_article_block_pool->p_block[mid]->articles[0].aid)
		{
			right = mid;
		}
		else
		{
			left = mid;
		}
	}

	p_block = p_article_block_pool->p_block[left];

	left = 0;
	right = p_block->article_count - 1;

	// aid in the range [ aid of articles[left], aid of articles[right] ]
	while (left < right)
	{
		mid = (left + right) / 2;

		if (aid <= p_block->articles[mid].aid)
		{
			right = mid;
		}
		else
		{
			left = mid + 1;
		}
	}

	return (p_block->articles + left);
}

ARTICLE *article_block_find_by_index(int index)
{
	ARTICLE_BLOCK *p_block;

	if (p_article_block_pool == NULL)
	{
		log_error("article_block_pool not initialized\n");
		return NULL;
	}

	if (index < 0 || index / ARTICLE_PER_BLOCK >= p_article_block_pool->block_count)
	{
		log_error("article_block_find_by_index(%d) is out of boundary of block [0, %d)\n", index, p_article_block_pool->block_count);
		return NULL;
	}

	p_block = p_article_block_pool->p_block[index / ARTICLE_PER_BLOCK];

	if (index % ARTICLE_PER_BLOCK >= p_block->article_count)
	{
		log_error("article_block_find_by_index(%d) is out of boundary of article [0, %d)\n", index, p_block->article_count);
		return NULL;
	}

	return (p_block->articles + (index % ARTICLE_PER_BLOCK));
}

extern int section_list_pool_init(const char *filename)
{
	int shmid;
	int proj_id;
	key_t key;
	size_t size;
	void *p_shm;

	if (p_section_list_pool == NULL || p_trie_dict_section_by_name == NULL || p_trie_dict_section_by_sid == NULL)
	{
		section_list_pool_cleanup();
	}

	p_section_list_pool = calloc(BBS_max_section, sizeof(SECTION_LIST));
	if (p_section_list_pool == NULL)
	{
		log_error("calloc(%d SECTION_LIST) OOM\n", BBS_max_section);
		return -1;
	}

	proj_id = (int)(time(NULL) % getpid());
	key = ftok(filename, proj_id);
	if (key == -1)
	{
		log_error("ftok(%s, %d) error (%d)\n", filename, proj_id, errno);
		return -3;
	}

	size = sizeof(shmid) + sizeof(SECTION_LIST) * BBS_max_section;
	shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
	if (shmid == -1)
	{
		log_error("shmget(section_list_pool, size = %d) error (%d)\n", size, errno);
		return -3;
	}
	p_shm = shmat(shmid, NULL, 0);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(shmid = %d) error (%d)\n", shmid, errno);
		return -3;
	}

	section_list_pool_shmid = shmid;
	p_section_list_pool = p_shm;
	section_list_count = 0;

	p_trie_dict_section_by_name = trie_dict_create();
	if (p_trie_dict_section_by_name == NULL)
	{
		log_error("trie_dict_create() OOM\n", BBS_max_section);
		return -2;
	}

	p_trie_dict_section_by_sid = trie_dict_create();
	if (p_trie_dict_section_by_sid == NULL)
	{
		log_error("trie_dict_create() OOM\n", BBS_max_section);
		return -2;
	}

	return 0;
}

inline static void sid_to_str(int32_t sid, char *p_sid_str)
{
	uint32_t u_sid;
	int i;

	u_sid = (uint32_t)sid;
	for (i = 0; i < SID_STR_LEN - 1; i++)
	{
		p_sid_str[i] = (char)(u_sid % 255 + 1);
		u_sid /= 255;
	}
	p_sid_str[i] = '\0';
}

SECTION_LIST *section_list_create(int32_t sid, const char *sname, const char *stitle, const char *master_name)
{
	SECTION_LIST *p_section;
	char sid_str[SID_STR_LEN];

	if (p_section_list_pool == NULL || p_trie_dict_section_by_name == NULL || p_trie_dict_section_by_sid == NULL)
	{
		log_error("session_list_pool not initialized\n");
		return NULL;
	}

	if (section_list_count >= BBS_max_section)
	{
		log_error("section_list_count exceed limit %d >= %d\n", section_list_count, BBS_max_section);
		return NULL;
	}

	sid_to_str(sid, sid_str);

	p_section = p_section_list_pool + section_list_count;

	p_section->sid = sid;

	strncpy(p_section->sname, sname, sizeof(p_section->sname - 1));
	p_section->sname[sizeof(p_section->sname - 1)] = '\0';

	strncpy(p_section->stitle, stitle, sizeof(p_section->stitle - 1));
	p_section->stitle[sizeof(p_section->stitle - 1)] = '\0';

	strncpy(p_section->master_name, master_name, sizeof(p_section->master_name - 1));
	p_section->master_name[sizeof(p_section->master_name - 1)] = '\0';

	if (trie_dict_set(p_trie_dict_section_by_name, sname, section_list_count) != 1)
	{
		log_error("trie_dict_set(section, %s, %d) error\n", sname, section_list_count);
		return NULL;
	}

	if (trie_dict_set(p_trie_dict_section_by_sid, sid_str, section_list_count) != 1)
	{
		log_error("trie_dict_set(section, %d, %d) error\n", sid, section_list_count);
		log_std("Debug %x %x %x %x\n", sid_str[0], sid_str[1], sid_str[2], sid_str[3]);
		return NULL;
	}

	section_list_reset_articles(p_section);

	section_list_count++;

	return p_section;
}

void section_list_reset_articles(SECTION_LIST *p_section)
{
	p_section->article_count = 0;
	p_section->topic_count = 0;
	p_section->visible_article_count = 0;
	p_section->visible_topic_count = 0;
	p_section->p_article_head = NULL;
	p_section->p_article_tail = NULL;

	p_section->page_count = 0;
	p_section->last_page_visible_article_count = 0;
}

void section_list_pool_cleanup(void)
{
	if (p_trie_dict_section_by_name != NULL)
	{
		trie_dict_destroy(p_trie_dict_section_by_name);
		p_trie_dict_section_by_name = NULL;
	}

	if (p_trie_dict_section_by_sid != NULL)
	{
		trie_dict_destroy(p_trie_dict_section_by_sid);
		p_trie_dict_section_by_sid = NULL;
	}

	if (p_section_list_pool != NULL)
	{
		if (shmdt(p_section_list_pool) == -1)
		{
			log_error("shmdt(shmid = %d) error (%d)\n", section_list_pool_shmid, errno);
		}
		p_section_list_pool = NULL;

		if (shmctl(section_list_pool_shmid, IPC_RMID, NULL) == -1)
		{
			log_error("shmctl(shmid = %d, IPC_RMID) error (%d)\n", section_list_pool_shmid, errno);
		}
	}

	section_list_count = 0;
}

SECTION_LIST *section_list_find_by_name(const char *sname)
{
	int64_t index;

	if (p_section_list_pool == NULL || p_trie_dict_section_by_name == NULL)
	{
		log_error("section_list not initialized\n");
		return NULL;
	}

	if (trie_dict_get(p_trie_dict_section_by_name, sname, &index) != 1)
	{
		log_error("trie_dict_get(section, %s) error\n", sname);
		return NULL;
	}

	return (p_section_list_pool + index);
}

SECTION_LIST *section_list_find_by_sid(int32_t sid)
{
	int64_t index;
	char sid_str[SID_STR_LEN];

	if (p_section_list_pool == NULL || p_trie_dict_section_by_sid == NULL)
	{
		log_error("section_list not initialized\n");
		return NULL;
	}

	sid_to_str(sid, sid_str);

	if (trie_dict_get(p_trie_dict_section_by_sid, sid_str, &index) != 1)
	{
		log_error("trie_dict_get(section, %d) error\n", sid);
		return NULL;
	}

	return (p_section_list_pool + index);
}

int section_list_append_article(SECTION_LIST *p_section, const ARTICLE *p_article_src)
{
	ARTICLE_BLOCK *p_block;
	int32_t last_aid = 0;
	ARTICLE *p_article;
	ARTICLE *p_topic_head;
	ARTICLE *p_topic_tail;

	if (p_section == NULL || p_article_src == NULL)
	{
		log_error("section_list_append_article() NULL pointer error\n");
		return -1;
	}

	if (p_article_block_pool == NULL)
	{
		log_error("article_block_pool not initialized\n");
		return -1;
	}

	if (p_section->sid != p_article_src->sid)
	{
		log_error("section_list_append_article() error: section sid %d != article sid %d\n", p_section->sid, p_article_src->sid);
		return -2;
	}

	if (p_section->article_count >= BBS_article_limit_per_section)
	{
		log_error("section_list_append_article() error: article_count reach limit in section %d\n", p_section->sid);
		return -2;
	}

	if (p_article_block_pool->block_count == 0 ||
		p_article_block_pool->p_block[p_article_block_pool->block_count - 1]->article_count >= ARTICLE_PER_BLOCK)
	{
		if ((p_block = pop_free_article_block()) == NULL)
		{
			log_error("pop_free_article_block() error\n");
			return -2;
		}

		if (p_article_block_pool->block_count > 0)
		{
			last_aid = p_article_block_pool->p_block[p_article_block_pool->block_count - 1]->articles[ARTICLE_PER_BLOCK - 1].aid;
		}

		p_article_block_pool->p_block[p_article_block_pool->block_count] = p_block;
		p_article_block_pool->block_count++;
	}
	else
	{
		p_block = p_article_block_pool->p_block[p_article_block_pool->block_count - 1];
		last_aid = p_block->articles[p_block->article_count - 1].aid;
	}

	// AID of articles should be strictly ascending
	if (p_article_src->aid <= last_aid)
	{
		log_error("section_list_append_article(aid=%d) error: last_aid=%d\n", p_article_src->aid, last_aid);
		return -3;
	}

	p_article = (p_block->articles + p_block->article_count);
	p_block->article_count++;
	p_section->article_count++;

	// Copy article data
	*p_article = *p_article_src;

	if (p_article->visible)
	{
		p_section->visible_article_count++;
	}

	// Link appended article as tail node of topic bi-directional list
	if (p_article->tid != 0)
	{
		p_topic_head = article_block_find_by_aid(p_article->tid);
		if (p_topic_head == NULL)
		{
			log_error("search head of topic (aid=%d) error\n", p_article->tid);
			return -4;
		}

		p_topic_tail = p_topic_head->p_topic_prior;
		if (p_topic_tail == NULL)
		{
			log_error("tail of topic (aid=%d) is NULL\n", p_article->tid);
			return -4;
		}
	}
	else
	{
		p_section->topic_count++;

		if (p_article->visible)
		{
			p_section->visible_topic_count++;
		}

		p_topic_head = p_article;
		p_topic_tail = p_article;
	}

	p_article->p_topic_prior = p_topic_tail;
	p_article->p_topic_next = p_topic_head;
	p_topic_head->p_topic_prior = p_article;
	p_topic_tail->p_topic_next = p_article;

	// Link appended article as tail node of article bi-directional list
	if (p_section->p_article_head == NULL)
	{
		p_section->p_article_head = p_article;
		p_section->p_article_tail = p_article;
	}
	p_article->p_prior = p_section->p_article_tail;
	p_article->p_next = p_section->p_article_head;
	p_section->p_article_head->p_prior = p_article;
	p_section->p_article_tail->p_next = p_article;
	p_section->p_article_tail = p_article;

	// Update page
	if ((p_article->visible && p_section->last_page_visible_article_count % BBS_article_limit_per_page == 0) ||
		p_section->article_count == 1)
	{
		p_section->p_page_first_article[p_section->page_count] = p_article;
		p_section->page_count++;
		p_section->last_page_visible_article_count = 0;
	}

	if (p_article->visible)
	{
		p_section->last_page_visible_article_count++;
	}

	return 0;
}

int section_list_set_article_visible(SECTION_LIST *p_section, int32_t aid, int8_t visible)
{
	ARTICLE *p_article;
	ARTICLE *p_reply;
	int affected_count = 0;

	if (p_section == NULL)
	{
		log_error("section_list_set_article_visible() NULL pointer error\n");
		return -2;
	}

	p_article = article_block_find_by_aid(aid);
	if (p_article == NULL)
	{
		return -1; // Not found
	}

	if (p_section->sid != p_article->sid)
	{
		log_error("section_list_set_article_visible() error: section sid %d != article sid %d\n", p_section->sid, p_article->sid);
		return -2;
	}

	if (p_article->visible == visible)
	{
		return 0; // Already set
	}

	if (visible == 0) // 1 -> 0
	{
		p_section->visible_article_count--;

		if (p_article->tid == 0)
		{
			p_section->visible_topic_count--;

			// Set related visible replies to invisible
			for (p_reply = p_article->p_topic_next; p_reply->tid != 0; p_reply = p_reply->p_topic_next)
			{
				if (p_reply->tid != aid)
				{
					log_error("Inconsistent tid = %d found in reply %d of topic %d\n", p_reply->tid, p_reply->aid, aid);
					continue;
				}

				if (p_reply->visible == 1)
				{
					p_reply->visible = 0;
					p_section->visible_article_count--;
					affected_count++;
				}
			}
		}
	}
	else // 0 -> 1
	{
		p_section->visible_article_count++;

		if (p_article->tid == 0)
		{
			p_section->visible_topic_count++;
		}
	}

	p_article->visible = visible;
	affected_count++;

	return affected_count;
}

ARTICLE *section_list_find_article_with_offset(SECTION_LIST *p_section, int32_t aid, int32_t *p_page, int32_t *p_offset, ARTICLE **pp_next)
{
	ARTICLE *p_article;
	int left;
	int right;
	int mid;

	*p_page = -1;
	*p_offset = -1;
	*pp_next = NULL;

	if (p_section == NULL)
	{
		log_error("section_list_find_article_with_offset() NULL pointer error\n");
		return NULL;
	}

	if (p_section->article_count == 0) // empty
	{
		*p_page = 0;
		*p_offset = 0;
		return NULL;
	}

	left = 0;
	right = p_section->page_count;

	// aid in the range [ head aid of pages[left], tail aid of pages[right - 1] ]
	while (left < right - 1)
	{
		// get page id no less than mid value of left page id and right page id
		mid = (left + right) / 2 + (right - left) % 2;

		if (mid >= p_section->page_count)
		{
			log_error("page id (mid = %d) is out of boundary\n", mid);
			return NULL;
		}

		if (aid < p_section->p_page_first_article[mid]->aid)
		{
			right = mid;
		}
		else
		{
			left = mid;
		}
	}

	*p_page = left;

	p_article = p_section->p_page_first_article[*p_page];

	// p_section->p_page_first_article[*p_page]->aid <= aid < p_section->p_page_first_article[*p_page + 1]->aid
	right = (*p_page == MAX(0, p_section->page_count - 1) ? INT32_MAX : p_section->p_page_first_article[*p_page + 1]->aid);

	// left will be the offset of article found or offset to insert
	left = 0;

	while (aid > p_article->aid)
	{
		p_article = p_article->p_next;
		left++;

		if (aid == p_article->aid)
		{
			*pp_next = p_article->p_next;
			break;
		}

		// over last article in the page
		if (p_article == p_section->p_article_head || p_article->aid >= right)
		{
			*pp_next = (p_article == p_section->p_article_head ? p_section->p_article_head : p_section->p_page_first_article[*p_page + 1]);
			*p_offset = left;
			return NULL; // not found
		}
	}

	if (aid < p_article->aid)
	{
		*pp_next = p_article;
		p_article = NULL; // not found
	}
	else // aid == p_article->aid
	{
		*pp_next = p_article->p_next;
	}

	*p_offset = left;

	return p_article;
}

int section_list_calculate_page(SECTION_LIST *p_section, int32_t start_aid)
{
	ARTICLE *p_article;
	ARTICLE *p_next;
	int32_t page;
	int32_t offset;
	int visible_article_count;
	int page_head_set;

	if (p_section == NULL)
	{
		log_error("section_list_calculate_page() NULL pointer error\n");
		return -1;
	}

	if (p_section->article_count == 0) // empty
	{
		p_section->page_count = 0;
		p_section->last_page_visible_article_count = 0;

		return 0;
	}

	if (start_aid > 0)
	{
		p_article = article_block_find_by_aid(start_aid);
		if (p_article == NULL)
		{
			return -1; // Not found
		}

		if (p_section->sid != p_article->sid)
		{
			log_error("section_list_calculate_page() error: section sid %d != start article sid %d\n", p_section->sid, p_article->sid);
			return -2;
		}

		p_article = section_list_find_article_with_offset(p_section, start_aid, &page, &offset, &p_next);
		if (p_article == NULL)
		{
			if (page < 0)
			{
				return -1;
			}
			log_error("section_list_calculate_page() aid = %d not found in section sid = %d\n",
					  start_aid, p_section->sid);
			return -2;
		}

		if (offset > 0)
		{
			p_article = p_section->p_page_first_article[page];
		}
	}
	else
	{
		p_article = p_section->p_article_head;
		page = 0;
		offset = 0;
	}

	visible_article_count = 0;
	page_head_set = 0;

	do
	{
		if (!page_head_set && visible_article_count == 0)
		{
			p_section->p_page_first_article[page] = p_article;
			page_head_set = 1;
		}

		if (p_article->visible)
		{
			visible_article_count++;
		}

		p_article = p_article->p_next;

		// skip remaining invisible articles
		while (p_article->visible == 0 && p_article != p_section->p_article_head)
		{
			p_article = p_article->p_next;
		}

		if (visible_article_count >= BBS_article_limit_per_page && p_article != p_section->p_article_head)
		{
			page++;
			visible_article_count = 0;
			page_head_set = 0;

			if (page >= BBS_article_limit_per_section / BBS_article_limit_per_page && p_article != p_section->p_article_head)
			{
				log_error("Count of page exceed limit in section %d\n", p_section->sid);
				break;
			}
		}
	} while (p_article != p_section->p_article_head);

	p_section->page_count = page + (visible_article_count > 0 ? 1 : 0);
	p_section->last_page_visible_article_count = visible_article_count;

	return 0;
}

int article_count_of_topic(int32_t aid)
{
	ARTICLE *p_article;
	int article_count;

	p_article = article_block_find_by_aid(aid);
	if (p_article == NULL)
	{
		return 0; // Not found
	}

	article_count = 0;

	do
	{
		article_count++;
		p_article = p_article->p_topic_next;
	} while (p_article->aid != aid);

	return article_count;
}

int section_list_move_topic(SECTION_LIST *p_section_src, SECTION_LIST *p_section_dest, int32_t aid)
{
	ARTICLE *p_article;
	ARTICLE *p_next;
	int32_t page;
	int32_t offset;
	int32_t move_article_count;
	int32_t dest_article_count_old;
	int32_t last_unaffected_aid_src;
	int32_t first_inserted_aid_dest;
	int move_counter;

	if (p_section_dest == NULL)
	{
		log_error("section_list_move_topic() NULL pointer error\n");
		return -1;
	}

	if ((p_article = article_block_find_by_aid(aid)) == NULL)
	{
		log_error("section_list_move_topic() error: article %d not found in block\n", aid);
		return -2;
	}

	if (p_section_src->sid != p_article->sid)
	{
		log_error("section_list_move_topic() error: src section sid %d != article %d sid %d\n",
				  p_section_src->sid, p_article->aid, p_article->sid);
		return -2;
	}

	if (p_article->tid != 0)
	{
		log_error("section_list_move_topic(aid = %d) error: article is not head of topic, tid = %d\n", aid, p_article->tid);
		return -2;
	}

	last_unaffected_aid_src = (p_article == p_section_src->p_article_head ? 0 : p_article->p_prior->aid);

	move_article_count = article_count_of_topic(aid);
	if (move_article_count <= 0)
	{
		log_error("section_list_count_of_topic_articles(aid = %d) <= 0\n", aid);
		return -2;
	}

	if (p_section_dest->article_count + move_article_count > BBS_article_limit_per_section)
	{
		log_error("section_list_move_topic() error: article_count %d reach limit in section %d\n",
				  p_section_dest->article_count + move_article_count, p_section_dest->sid);
		return -3;
	}

	dest_article_count_old = p_section_dest->article_count;
	move_counter = 0;
	first_inserted_aid_dest = p_article->aid;

	do
	{
		if (p_section_src->sid != p_article->sid)
		{
			log_error("section_list_move_topic() error: src section sid %d != article %d sid %d\n",
					  p_section_src->sid, p_article->aid, p_article->sid);
			return -2;
		}

		// Remove from bi-directional article list of src section
		if (p_section_src->p_article_head == p_article)
		{
			p_section_src->p_article_head = p_article->p_next;
		}
		if (p_section_src->p_article_tail == p_article)
		{
			p_section_src->p_article_tail = p_article->p_prior;
		}
		if (p_section_src->p_article_head == p_article) // || p_section_src->p_article_tail == p_article
		{
			p_section_src->p_article_head = NULL;
			p_section_src->p_article_tail = NULL;
		}

		p_article->p_prior->p_next = p_article->p_next;
		p_article->p_next->p_prior = p_article->p_prior;

		// Update sid of article
		p_article->sid = p_section_dest->sid;

		if (section_list_find_article_with_offset(p_section_dest, p_article->aid, &page, &offset, &p_next) != NULL)
		{
			log_error("section_list_move_topic() error: article %d already in section %d\n", p_article->aid, p_section_dest->sid);
			return -4;
		}

		// Insert into bi-directional article list of dest section
		if (p_next == NULL) // empty section
		{
			p_section_dest->p_article_head = p_article;
			p_section_dest->p_article_tail = p_article;
			p_article->p_prior = p_article;
			p_article->p_next = p_article;
		}
		else
		{
			if (p_section_dest->p_article_head == p_next)
			{
				if (p_article->aid < p_next->aid)
				{
					p_section_dest->p_article_head = p_article;
				}
				else // p_article->aid > p_next->aid
				{
					p_section_dest->p_article_tail = p_article;
				}
			}

			p_article->p_prior = p_next->p_prior;
			p_article->p_next = p_next;
			p_next->p_prior->p_next = p_article;
			p_next->p_prior = p_article;
		}

		// Update article / topic counter of src / desc section
		p_section_src->article_count--;
		p_section_dest->article_count++;
		if (p_article->tid == 0)
		{
			p_section_src->topic_count--;
			p_section_dest->topic_count++;
		}

		// Update visible article / topic counter of src / desc section
		if (p_article->visible)
		{
			p_section_src->visible_article_count--;
			p_section_dest->visible_article_count++;
			if (p_article->tid == 0)
			{
				p_section_src->visible_topic_count--;
				p_section_dest->visible_topic_count++;
			}
		}

		// Update page for empty dest section
		if (p_section_dest->article_count == 1)
		{
			p_section_dest->p_page_first_article[0] = p_article;
			p_section_dest->page_count = 1;
			p_section_dest->last_page_visible_article_count = (p_article->visible ? 1 : 0);
		}

		p_article = p_article->p_topic_next;

		move_counter++;
		if (move_counter % CALCULATE_PAGE_THRESHOLD == 0)
		{
			// Re-calculate pages of desc section
			if (section_list_calculate_page(p_section_dest, first_inserted_aid_dest) < 0)
			{
				log_error("section_list_calculate_page(dest section = %d, aid = %d) error\n",
						  p_section_dest->sid, first_inserted_aid_dest);
			}

			first_inserted_aid_dest = p_article->aid;
		}
	} while (p_article->aid != aid);

	if (p_section_dest->article_count - dest_article_count_old != move_article_count)
	{
		log_error("section_list_move_topic() error: count of moved articles %d != %d\n",
				  p_section_dest->article_count - dest_article_count_old, move_article_count);
	}

	// Re-calculate pages of src section
	if (section_list_calculate_page(p_section_src, last_unaffected_aid_src) < 0)
	{
		log_error("section_list_calculate_page(src section = %d, aid = %d) error at aid = %d\n",
				  p_section_src->sid, last_unaffected_aid_src, aid);
	}

	if (move_counter % CALCULATE_PAGE_THRESHOLD != 0)
	{
		// Re-calculate pages of desc section
		if (section_list_calculate_page(p_section_dest, first_inserted_aid_dest) < 0)
		{
			log_error("section_list_calculate_page(dest section = %d, aid = %d) error\n",
					  p_section_dest->sid, first_inserted_aid_dest);
		}
	}

	return move_article_count;
}
