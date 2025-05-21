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
#include "io.h"
#include "trie_dict.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/ipc.h>

// ARTICLE_BLOCK_PER_SHM * ARTICLE_BLOCK_SHM_COUNT_LIMIT should be
// no less than BBS_article_block_limit_per_section * BBS_max_section,
// in order to allocate enough memory for blocks
#define ARTICLE_BLOCK_PER_SHM 50 // sizeof(ARTICLE_BLOCK) * ARTICLE_BLOCK_PER_SHM is the size of each shm segment to allocate
#define ARTICLE_BLOCK_SHM_COUNT_LIMIT 256 // limited by length (8-bit) of proj_id in ftok(path, proj_id)

struct article_block_shm_t
{
	int shmid;
	void *p_shm;
};
typedef struct article_block_shm_t ARTICLE_BLOCK_SHM;

static ARTICLE_BLOCK_SHM *p_article_block_shm_pool;
static int article_block_shm_count;
static ARTICLE_BLOCK *p_article_block_free_list;

static SECTION_DATA *p_section_data_pool;
static int section_data_count;
static TRIE_NODE *p_trie_dict_section_data;

int section_data_pool_init(const char *filename, int article_block_count)
{
	int shmid;
	int proj_id;
	key_t key;
	size_t size;
	void *p_shm;
	int i;
	int article_block_count_in_shm;
	ARTICLE_BLOCK *p_article_block_in_shm;
	ARTICLE_BLOCK **pp_article_block_next;

	if (p_article_block_shm_pool != NULL ||
		p_article_block_free_list != NULL ||
		p_section_data_pool != NULL ||
		p_trie_dict_section_data != NULL)
	{
		log_error("section_data_pool already initialized\n");
		return -1;
	}

	if (article_block_count > ARTICLE_BLOCK_PER_SHM * ARTICLE_BLOCK_SHM_COUNT_LIMIT)
	{
		log_error("article_block_count exceed limit %d\n", ARTICLE_BLOCK_PER_SHM * ARTICLE_BLOCK_SHM_COUNT_LIMIT);
		return -2;
	}

	p_article_block_shm_pool = calloc((size_t)article_block_count / ARTICLE_BLOCK_PER_SHM + 1, sizeof(ARTICLE_BLOCK_SHM));
	if (p_article_block_shm_pool == NULL)
	{
		log_error("calloc(%d ARTICLE_BLOCK_SHM) OOM\n", article_block_count / ARTICLE_BLOCK_PER_SHM + 1);
		return -2;
	}

	p_section_data_pool = calloc(BBS_max_section, sizeof(SECTION_DATA));
	if (p_section_data_pool == NULL)
	{
		log_error("calloc(%d SECTION_DATA) OOM\n", BBS_max_section);
		return -2;
	}
	section_data_count = 0;

	p_trie_dict_section_data = trie_dict_create();
	if (p_trie_dict_section_data == NULL)
	{
		log_error("trie_dict_create() OOM\n", BBS_max_section);
		return -2;
	}

	// Allocate shared memory
	article_block_shm_count = 0;
	pp_article_block_next = &p_article_block_free_list;

	while (article_block_count > 0)
	{
		article_block_count_in_shm =
			(article_block_count < ARTICLE_BLOCK_PER_SHM ? article_block_count : ARTICLE_BLOCK_PER_SHM);
		article_block_count -= article_block_count_in_shm;

		proj_id = getpid() + article_block_shm_count;
		key = ftok(filename, proj_id);
		if (key == -1)
		{
			log_error("ftok(%s, %d) error (%d)\n", filename, proj_id, errno);
			section_data_pool_cleanup();
			return -3;
		}

		size = sizeof(shmid) + sizeof(ARTICLE_BLOCK) * (size_t)article_block_count_in_shm;
		shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
		if (shmid == -1)
		{
			log_error("shmget(shm_index = %d, size = %d) error (%d)\n", article_block_shm_count, size, errno);
			section_data_pool_cleanup();
			return -3;
		}
		p_shm = shmat(shmid, NULL, 0);
		if (p_shm == (void *)-1)
		{
			log_error("shmat(shmid = %d) error (%d)\n", shmid, errno);
			section_data_pool_cleanup();
			return -3;
		}

		(p_article_block_shm_pool + article_block_shm_count)->shmid = shmid;
		(p_article_block_shm_pool + article_block_shm_count)->p_shm = p_shm;
		article_block_shm_count++;

		p_article_block_in_shm = p_shm;
		*pp_article_block_next = p_article_block_in_shm;

		for (i = 0; i < article_block_count_in_shm; i++)
		{
			if (i < article_block_count_in_shm - 1)
			{
				(p_article_block_in_shm + i)->p_next_block = (p_article_block_in_shm + i + 1);
			}
			else
			{
				(p_article_block_in_shm + i)->p_next_block = NULL;
				pp_article_block_next = &((p_article_block_in_shm + i)->p_next_block);
			}
		}
	}

	return 0;
}

void section_data_pool_cleanup(void)
{
	int i;

	if (p_trie_dict_section_data != NULL)
	{
		trie_dict_destroy(p_trie_dict_section_data);
		p_trie_dict_section_data = NULL;
		section_data_count = 0;
	}

	if (p_section_data_pool != NULL)
	{
		free(p_section_data_pool);
		p_section_data_pool = NULL;
	}

	if (p_article_block_free_list != NULL)
	{
		p_article_block_free_list = NULL;
	}

	if (p_article_block_shm_pool != NULL)
	{
		for (i = 0; i < article_block_shm_count; i++)
		{
			if (shmdt((p_article_block_shm_pool + i)->p_shm) == -1)
			{
				log_error("shmdt(shmid = %d) error (%d)\n", (p_article_block_shm_pool + i)->shmid, errno);
			}

			if (shmctl((p_article_block_shm_pool + i)->shmid, IPC_RMID, NULL) == -1)
			{
				log_error("shmctl(shmid = %d, IPC_RMID) error (%d)\n", (p_article_block_shm_pool + i)->shmid, errno);
			}
		}

		p_article_block_shm_pool = NULL;
		article_block_shm_count = 0;
	}
}

inline static ARTICLE_BLOCK *pop_free_article_block(void)
{
	ARTICLE_BLOCK *p_article_block = NULL;

	if (p_article_block_free_list != NULL)
	{
		p_article_block = p_article_block_free_list;
		p_article_block_free_list = p_article_block_free_list->p_next_block;
	}

	return p_article_block;
}

inline static void push_free_article_block(ARTICLE_BLOCK *p_article_block)
{
	p_article_block->p_next_block = p_article_block_free_list;
	p_article_block_free_list = p_article_block;
}

SECTION_DATA *section_data_create(const char *sname, const char *stitle, const char *master_name)
{
	SECTION_DATA *p_section;
	int index;

	if (p_section_data_pool == NULL || p_trie_dict_section_data == NULL)
	{
		log_error("section_data not initialized\n");
		return NULL;
	}

	if (section_data_count >= BBS_max_section)
	{
		log_error("section_data_count exceed limit %d\n", BBS_max_section);
		return NULL;
	}

	index = section_data_count;
	p_section = p_section_data_pool + index;

	strncpy(p_section->sname, sname, sizeof(p_section->sname - 1));
	p_section->sname[sizeof(p_section->sname - 1)] = '\0';

	strncpy(p_section->stitle, stitle, sizeof(p_section->stitle - 1));
	p_section->stitle[sizeof(p_section->stitle - 1)] = '\0';

	strncpy(p_section->master_name, master_name, sizeof(p_section->master_name - 1));
	p_section->master_name[sizeof(p_section->master_name - 1)] = '\0';

	p_section->p_head_block = NULL;
	p_section->p_tail_block = NULL;
	p_section->block_count = 0;
	p_section->article_count = 0;
	p_section->delete_count = 0;

	if (trie_dict_set(p_trie_dict_section_data, sname, index) != 1)
	{
		log_error("trie_dict_set(section_data, %s, %d) error\n", sname, index);
		return NULL;
	}

	section_data_count++;

	return p_section;
}

int section_data_free_block(SECTION_DATA *p_section)
{
	ARTICLE_BLOCK *p_block;

	if (p_section == NULL)
	{
		log_error("section_data_free_block() NULL pointer error\n");
		return -1;
	}

	if (p_section_data_pool == NULL)
	{
		log_error("section_data not initialized\n");
		return -1;
	}

	while (p_section->p_head_block != NULL)
	{
		p_block = p_section->p_head_block;
		p_section->p_head_block = p_block->p_next_block;
		push_free_article_block(p_block);
	}

	p_section->p_tail_block = NULL;
	p_section->block_count = 0;
	p_section->article_count = 0;
	p_section->delete_count = 0;

	return 0;
}

SECTION_DATA *section_data_find_by_name(const char *sname)
{
	int64_t index;

	if (p_section_data_pool == NULL || p_trie_dict_section_data == NULL)
	{
		log_error("section_data not initialized\n");
		return NULL;
	}

	if (trie_dict_get(p_trie_dict_section_data, sname, &index) != 1)
	{
		log_error("trie_dict_get(section_data, %s) error\n", sname);
		return NULL;
	}

	return (p_section_data_pool + index);
}

int section_data_append_article(SECTION_DATA *p_section, const ARTICLE *p_article)
{
	ARTICLE_BLOCK *p_block;
	int32_t last_aid = 0;
	ARTICLE *p_topic_head;
	ARTICLE *p_topic_tail;

	if (p_section == NULL || p_article == NULL)
	{
		log_error("section_data_append_article() NULL pointer error\n");
		return -1;
	}

	if (p_section_data_pool == NULL)
	{
		log_error("section_data not initialized\n");
		return -1;
	}

	if (p_section->p_tail_block == NULL || p_section->p_tail_block->article_count >= BBS_article_limit_per_block)
	{
		if (p_section->block_count >= BBS_article_block_limit_per_section)
		{
			log_error("section block count %d reach limit\n", p_section->block_count);
			return -2;
		}

		if ((p_block = pop_free_article_block()) == NULL)
		{
			log_error("pop_free_article_block() error\n");
			return -2;
		}

		p_block->article_count = 0;
		p_block->p_next_block = NULL;

		if (p_section->p_tail_block == NULL)
		{
			p_section->p_head_block = p_block;
			last_aid = 0;
		}
		else
		{
			p_section->p_tail_block->p_next_block = p_block;
			last_aid = p_section->p_tail_block->articles[BBS_article_limit_per_block - 1].aid;
		}
		p_section->p_tail_block = p_block;
		p_section->p_block[p_section->block_count] = p_block;
		p_section->block_count++;
	}
	else
	{
		p_block = p_section->p_tail_block;
		last_aid = p_block->articles[p_block->article_count - 1].aid;
	}

	// AID of articles should be strictly ascending
	if (p_article->aid <= last_aid)
	{
		log_error("section_data_append_article(aid=%d) error: last_aid=%d\n", p_article->aid, last_aid);
		return -3;
	}

	if (p_block->article_count == 0)
	{
		p_section->block_head_aid[p_section->block_count - 1] = p_article->aid;
	}

	if (p_article->tid != 0)
	{
		p_topic_head = section_data_find_article_by_aid(p_section, p_article->tid);
		if (p_topic_head == NULL)
		{
			log_error("search head of topic (aid=%d) error\n", p_article->tid);
			return -4;
		}

		p_topic_tail = section_data_find_article_by_aid(p_section, p_topic_head->prior_aid);
		if (p_topic_tail == NULL)
		{
			log_error("search tail of topic (aid=%d) error\n", p_topic_head->prior_aid);
			return -4;
		}
	}
	else
	{
		p_topic_head = &(p_block->articles[p_block->article_count]);
		p_topic_tail = p_topic_head;
	}

	// Copy article data
	p_block->articles[p_block->article_count] = *p_article;

	// Link appended article as tail node of topic bi-directional list;
	p_block->articles[p_block->article_count].prior_aid = p_topic_tail->aid;
	p_block->articles[p_block->article_count].next_aid = p_topic_head->aid;
	p_topic_head->prior_aid = p_article->aid;
	p_topic_tail->next_aid = p_article->aid;

	p_block->article_count++;
	p_section->article_count++;

	return 0;
}

ARTICLE *section_data_find_article_by_aid(SECTION_DATA *p_section, int32_t aid)
{
	ARTICLE *p_article;
	ARTICLE_BLOCK *p_block;
	int left;
	int right;
	int mid;

	if (p_section == NULL)
	{
		log_error("section_data_find_article_by_aid() NULL pointer error\n");
		return NULL;
	}

	if (p_section->block_count == 0) // empty section
	{
		return NULL;
	}

	left = 0;
	right = p_section->block_count;

	// aid in the range [ head aid of blocks[left], tail aid of blocks[right - 1] ]
	while (left < right - 1)
	{
		// get block offset no less than mid value of left and right block offsets
		mid = (left + right) / 2 + (right - left) % 2;
		
		if (mid >= BBS_article_block_limit_per_section)
		{
			log_error("block_m(%d) is out of boundary\n", mid);
			return NULL;
		}

		if (aid < p_section->block_head_aid[mid])
		{
			right = mid;
		}
		else
		{
			left = mid;
		}
	}

	p_block = p_section->p_block[left];

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

	p_article = &(p_block->articles[left]);

	return p_article;
}

ARTICLE *section_data_find_article_by_index(SECTION_DATA *p_section, int index)
{
	ARTICLE *p_article;
	ARTICLE_BLOCK *p_block;

	if (p_section == NULL)
	{
		log_error("section_data_find_article_by_index() NULL pointer error\n");
		return NULL;
	}

	if (index < 0 || index >= p_section->article_count)
	{
		log_error("section_data_find_article_by_index(%d) is out of boundary [0, %d)\n", index, p_section->article_count);
		return NULL;
	}

	p_block = p_section->p_block[index / BBS_article_limit_per_block];
	p_article = &(p_block->articles[index % BBS_article_limit_per_block]);

	return p_article;
}

int section_data_mark_del_article(SECTION_DATA *p_section, int32_t aid)
{
	ARTICLE *p_article;

	if (p_section == NULL)
	{
		log_error("section_data_mark_del_article() NULL pointer error\n");
		return -2;
	}

	p_article = section_data_find_article_by_aid(p_section, aid);
	if (p_article == NULL)
	{
		return -1; // Not found
	}

	if (p_article->visible == 0)
	{
		return 0; // Already deleted
	}

	p_article->visible = 0;
	p_section->delete_count++;

	return 1;
}
