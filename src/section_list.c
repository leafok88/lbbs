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

#define _GNU_SOURCE

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
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#ifdef _SEM_SEMUN_UNDEFINED
union semun
{
	int val;			   /* Value for SETVAL */
	struct semid_ds *buf;  /* Buffer for IPC_STAT, IPC_SET */
	unsigned short *array; /* Array for GETALL, SETALL */
	struct seminfo *__buf; /* Buffer for IPC_INFO
							  (Linux-specific) */
};
#endif // #ifdef _SEM_SEMUN_UNDEFINED

#define SECTION_TRY_LOCK_WAIT_TIME 1 // second
#define SECTION_TRY_LOCK_TIMES 10

#define ARTICLE_BLOCK_PER_SHM 400		  // sizeof(ARTICLE_BLOCK) * ARTICLE_BLOCK_PER_SHM is the size of each shm segment to allocate
#define ARTICLE_BLOCK_SHM_COUNT_LIMIT 200 // limited by length (8-bit) of proj_id in ftok(path, proj_id)
#define ARTICLE_BLOCK_PER_POOL (ARTICLE_BLOCK_PER_SHM * ARTICLE_BLOCK_SHM_COUNT_LIMIT)

#define CALCULATE_PAGE_THRESHOLD 100 // Adjust to tune performance of moving topic between sections

#define SID_STR_LEN 5 // 32-bit + NULL

struct article_block_t
{
	ARTICLE articles[ARTICLE_PER_BLOCK];
	int article_count;
	struct article_block_t *p_next_block;
};
typedef struct article_block_t ARTICLE_BLOCK;

struct article_block_pool_t
{
	int shmid;
	struct
	{
		int shmid;
		void *p_shm;
	} shm_pool[ARTICLE_BLOCK_SHM_COUNT_LIMIT];
	int shm_count;
	ARTICLE_BLOCK *p_block_free_list;
	ARTICLE_BLOCK *p_block[ARTICLE_BLOCK_PER_POOL];
	int block_count;
};
typedef struct article_block_pool_t ARTICLE_BLOCK_POOL;

static ARTICLE_BLOCK_POOL *p_article_block_pool = NULL;

struct section_list_pool_t
{
	int shmid;
	SECTION_LIST sections[BBS_max_section];
	int section_count;
	int semid;
	TRIE_NODE *p_trie_dict_section_by_name;
	TRIE_NODE *p_trie_dict_section_by_sid;
};
typedef struct section_list_pool_t SECTION_LIST_POOL;

static SECTION_LIST_POOL *p_section_list_pool = NULL;

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

	if (block_count <= 0 || block_count > ARTICLE_BLOCK_PER_POOL)
	{
		log_error("article_block_count exceed limit %d\n", ARTICLE_BLOCK_PER_POOL);
		return -2;
	}

	// Allocate shared memory
	proj_id = ARTICLE_BLOCK_SHM_COUNT_LIMIT; // keep different from proj_id used to create block shm
	key = ftok(filename, proj_id);
	if (key == -1)
	{
		log_error("ftok(%s, %d) error (%d)\n", filename, proj_id, errno);
		return -3;
	}

	size = sizeof(ARTICLE_BLOCK_POOL);
	shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
	if (shmid == -1)
	{
		log_error("shmget(article_block_pool_shm, size = %d) error (%d)\n", size, errno);
		return -3;
	}
	p_shm = shmat(shmid, NULL, 0);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(shmid = %d) error (%d)\n", shmid, errno);
		return -3;
	}

	p_article_block_pool = p_shm;
	p_article_block_pool->shmid = shmid;

	p_article_block_pool->shm_count = 0;
	pp_block_next = &(p_article_block_pool->p_block_free_list);

	while (block_count > 0)
	{
		block_count_in_shm = MIN(block_count, ARTICLE_BLOCK_PER_SHM);
		block_count -= block_count_in_shm;

		proj_id = p_article_block_pool->shm_count;
		key = ftok(filename, proj_id);
		if (key == -1)
		{
			log_error("ftok(%s, %d) error (%d)\n", filename, proj_id, errno);
			article_block_cleanup();
			return -3;
		}

		size = sizeof(ARTICLE_BLOCK) * (size_t)block_count_in_shm;
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
	int shmid;

	if (p_article_block_pool == NULL)
	{
		return;
	}

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

	shmid = p_article_block_pool->shmid;

	if (shmdt(p_article_block_pool) == -1)
	{
		log_error("shmdt(shmid = %d) error (%d)\n", shmid, errno);
	}

	if (shmctl(shmid, IPC_RMID, NULL) == -1)
	{
		log_error("shmctl(shmid = %d, IPC_RMID) error (%d)\n", shmid, errno);
	}

	p_article_block_pool = NULL;
}

int set_article_block_shm_readonly(void)
{
	int shmid;
	void *p_shm;
	int i;

	if (p_article_block_pool == NULL)
	{
		log_error("article_block_pool not initialized\n");
		return -1;
	}

	for (i = 0; i < p_article_block_pool->shm_count; i++)
	{
		shmid = (p_article_block_pool->shm_pool + i)->shmid;

		// Remap shared memory in read-only mode
		p_shm = shmat(shmid, (p_article_block_pool->shm_pool + i)->p_shm, SHM_RDONLY | SHM_REMAP);
		if (p_shm == (void *)-1)
		{
			log_error("shmat(article_block_pool shmid = %d) error (%d)\n", shmid, errno);
			return -2;
		}
	}

	return 0;
}

int detach_article_block_shm(void)
{
	int shmid;

	if (p_article_block_pool == NULL)
	{
		return -1;
	}

	for (int i = 0; i < p_article_block_pool->shm_count; i++)
	{
		if ((p_article_block_pool->shm_pool + i)->p_shm != NULL && shmdt((p_article_block_pool->shm_pool + i)->p_shm) == -1)
		{
			log_error("shmdt(shmid = %d) error (%d)\n", (p_article_block_pool->shm_pool + i)->shmid, errno);
			return -2;
		}
	}

	shmid = p_article_block_pool->shmid;

	if (shmdt(p_article_block_pool) == -1)
	{
		log_error("shmdt(shmid = %d) error (%d)\n", shmid, errno);
		return -3;
	}

	p_article_block_pool = NULL;

	return 0;
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

	if (aid != p_block->articles[left].aid) // not found
	{
		return NULL;
	}

	return (p_block->articles + left); // found
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

extern int section_list_init(const char *filename)
{
	int semid;
	int shmid;
	int proj_id;
	key_t key;
	size_t size;
	void *p_shm;
	union semun arg;
	int i;

	if (p_section_list_pool != NULL)
	{
		section_list_cleanup();
	}

	proj_id = (int)(time(NULL) % getpid());
	key = ftok(filename, proj_id);
	if (key == -1)
	{
		log_error("ftok(%s, %d) error (%d)\n", filename, proj_id, errno);
		return -3;
	}

	// Allocate shared memory
	size = sizeof(SECTION_LIST_POOL);
	shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
	if (shmid == -1)
	{
		log_error("shmget(section_list_pool_shm, size = %d) error (%d)\n", size, errno);
		return -3;
	}
	p_shm = shmat(shmid, NULL, 0);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(shmid = %d) error (%d)\n", shmid, errno);
		return -3;
	}

	p_section_list_pool = p_shm;
	p_section_list_pool->shmid = shmid;
	p_section_list_pool->section_count = 0;

	// Allocate semaphore as section locks
	size = 2 * (BBS_max_section + 1); // r_sem and w_sem per section, the last pair for all sections
	semid = semget(key, (int)size, IPC_CREAT | IPC_EXCL | 0600);
	if (semid == -1)
	{
		log_error("semget(section_list_pool_sem, size = %d) error (%d)\n", size, errno);
		return -3;
	}

	// Initialize sem value to 0
	arg.val = 0;
	for (i = 0; i < size; i++)
	{
		if (semctl(semid, i, SETVAL, arg) == -1)
		{
			log_error("semctl(section_list_pool_sem, SETVAL) error (%d)\n", errno);
			return -3;
		}
	}

	p_section_list_pool->semid = semid;

	p_section_list_pool->p_trie_dict_section_by_name = trie_dict_create();
	if (p_section_list_pool->p_trie_dict_section_by_name == NULL)
	{
		log_error("trie_dict_create() OOM\n", BBS_max_section);
		return -2;
	}

	p_section_list_pool->p_trie_dict_section_by_sid = trie_dict_create();
	if (p_section_list_pool->p_trie_dict_section_by_sid == NULL)
	{
		log_error("trie_dict_create() OOM\n", BBS_max_section);
		return -2;
	}

	return 0;
}

void section_list_cleanup(void)
{
	int shmid;

	if (p_section_list_pool == NULL)
	{
		return;
	}

	if (p_section_list_pool->p_trie_dict_section_by_name != NULL)
	{
		trie_dict_destroy(p_section_list_pool->p_trie_dict_section_by_name);
		p_section_list_pool->p_trie_dict_section_by_name = NULL;
	}

	if (p_section_list_pool->p_trie_dict_section_by_sid != NULL)
	{
		trie_dict_destroy(p_section_list_pool->p_trie_dict_section_by_sid);
		p_section_list_pool->p_trie_dict_section_by_sid = NULL;
	}

	shmid = p_section_list_pool->shmid;

	if (semctl(p_section_list_pool->semid, 0, IPC_RMID) == -1)
	{
		log_error("semctl(semid = %d, IPC_RMID) error (%d)\n", p_section_list_pool->semid, errno);
	}

	if (shmdt(p_section_list_pool) == -1)
	{
		log_error("shmdt(shmid = %d) error (%d)\n", shmid, errno);
	}

	if (shmctl(shmid, IPC_RMID, NULL) == -1)
	{
		log_error("shmctl(shmid = %d, IPC_RMID) error (%d)\n", shmid, errno);
	}

	p_section_list_pool = NULL;
}

int set_section_list_shm_readonly(void)
{
	int shmid;
	void *p_shm;

	if (p_section_list_pool == NULL)
	{
		log_error("p_section_list_pool not initialized\n");
		return -1;
	}

	shmid = p_section_list_pool->shmid;

	// Remap shared memory in read-only mode
	p_shm = shmat(shmid, p_section_list_pool, SHM_RDONLY | SHM_REMAP);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(section_list_pool shmid = %d) error (%d)\n", shmid, errno);
		return -3;
	}

	p_section_list_pool = p_shm;

	return 0;
}

int detach_section_list_shm(void)
{
	if (p_section_list_pool != NULL && shmdt(p_section_list_pool) == -1)
	{
		log_error("shmdt(section_list_pool) error (%d)\n", errno);
		return -1;
	}

	p_section_list_pool = NULL;

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

SECTION_LIST *section_list_create(int32_t sid, const char *sname, const char *stitle, const char *master_list)
{
	SECTION_LIST *p_section;
	char sid_str[SID_STR_LEN];

	if (p_section_list_pool == NULL)
	{
		log_error("session_list_pool not initialized\n");
		return NULL;
	}

	if (p_section_list_pool->section_count >= BBS_max_section)
	{
		log_error("section_count reach limit %d >= %d\n", p_section_list_pool->section_count, BBS_max_section);
		return NULL;
	}

	sid_to_str(sid, sid_str);

	p_section = p_section_list_pool->sections + p_section_list_pool->section_count;

	p_section->sid = sid;

	strncpy(p_section->sname, sname, sizeof(p_section->sname) - 1);
	p_section->sname[sizeof(p_section->sname) - 1] = '\0';

	strncpy(p_section->stitle, stitle, sizeof(p_section->stitle) - 1);
	p_section->stitle[sizeof(p_section->stitle) - 1] = '\0';

	strncpy(p_section->master_list, master_list, sizeof(p_section->master_list) - 1);
	p_section->master_list[sizeof(p_section->master_list) - 1] = '\0';

	if (trie_dict_set(p_section_list_pool->p_trie_dict_section_by_name, sname, p_section_list_pool->section_count) != 1)
	{
		log_error("trie_dict_set(section, %s, %d) error\n", sname, p_section_list_pool->section_count);
		return NULL;
	}

	if (trie_dict_set(p_section_list_pool->p_trie_dict_section_by_sid, sid_str, p_section_list_pool->section_count) != 1)
	{
		log_error("trie_dict_set(section, %d, %d) error\n", sid, p_section_list_pool->section_count);
		return NULL;
	}

	section_list_reset_articles(p_section);

	p_section_list_pool->section_count++;

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

SECTION_LIST *section_list_find_by_name(const char *sname)
{
	int64_t index;
	int ret;

	if (p_section_list_pool == NULL)
	{
		log_error("section_list not initialized\n");
		return NULL;
	}

	ret = trie_dict_get(p_section_list_pool->p_trie_dict_section_by_name, sname, &index);
	if (ret < 0)
	{
		log_error("trie_dict_get(section, %s) error\n", sname);
		return NULL;
	}
	else if (ret == 0)
	{
		return NULL;
	}

	return (p_section_list_pool->sections + index);
}

SECTION_LIST *section_list_find_by_sid(int32_t sid)
{
	int64_t index;
	int ret;
	char sid_str[SID_STR_LEN];

	if (p_section_list_pool == NULL)
	{
		log_error("section_list not initialized\n");
		return NULL;
	}

	sid_to_str(sid, sid_str);

	ret = trie_dict_get(p_section_list_pool->p_trie_dict_section_by_sid, sid_str, &index);
	if (ret < 0)
	{
		log_error("trie_dict_get(section, %d) error\n", sid);
		return NULL;
	}
	else if (ret == 0)
	{
		return NULL;
	}

	return (p_section_list_pool->sections + index);
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

int32_t article_block_last_aid(void)
{
	ARTICLE_BLOCK *p_block = p_article_block_pool->p_block[p_article_block_pool->block_count - 1];
	int32_t last_aid = p_block->articles[p_block->article_count - 1].aid;

	return last_aid;
}

int article_block_article_count(void)
{
	int ret;

	if (p_article_block_pool == NULL || p_article_block_pool->block_count <= 0)
	{
		return -1;
	}

	ret = (p_article_block_pool->block_count - 1) * ARTICLE_PER_BLOCK +
		  p_article_block_pool->p_block[p_article_block_pool->block_count - 1]->article_count;

	return ret;
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
		if (p_article->tid != 0 && p_article->tid != aid)
		{
			log_error("article_count_of_topic(%d) error: article %d not linked to the topic\n", aid, p_article->aid);
			break;
		}

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

	if (p_section_src == NULL || p_section_dest == NULL)
	{
		log_error("section_list_move_topic() NULL pointer error\n");
		return -1;
	}

	if (p_section_src->sid == p_section_dest->sid)
	{
		log_error("section_list_move_topic() src and dest section are the same\n");
		return -1;
	}

	if ((p_article = article_block_find_by_aid(aid)) == NULL)
	{
		log_error("article_block_find_by_aid(aid = %d) error: article not found\n", aid);
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
		log_error("article_count_of_topic(aid = %d) <= 0\n", aid);
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
			log_error("section_list_move_topic() warning: src section sid %d != article %d sid %d\n",
					  p_section_src->sid, p_article->aid, p_article->sid);
			p_article = p_article->p_topic_next;
			continue;
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
			log_error("section_list_move_topic() warning: article %d already in section %d\n", p_article->aid, p_section_dest->sid);
			p_article = p_article->p_topic_next;
			continue;
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
		log_error("section_list_move_topic() warning: count of moved articles %d != %d\n",
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

int get_section_index(SECTION_LIST *p_section)
{
	int index;

	if (p_section_list_pool == NULL)
	{
		log_error("get_section_index() error: uninitialized\n");
		return -1;
	}

	if (p_section == NULL)
	{
		index = BBS_max_section;
	}
	else
	{
		index = (int)(p_section - p_section_list_pool->sections);
		if (index < 0 || index >= BBS_max_section)
		{
			log_error("get_section_index(%d) error: index out of range\n", index);
			return -2;
		}
	}

	return index;
}

int section_list_try_rd_lock(SECTION_LIST *p_section, int wait_sec)
{
	int index;
	struct sembuf sops[4];
	struct timespec timeout;
	int ret;

	index = get_section_index(p_section);
	if (index < 0)
	{
		return -2;
	}

	sops[0].sem_num = (unsigned short)(index * 2 + 1); // w_sem of section index
	sops[0].sem_op = 0;								   // wait until unlocked
	sops[0].sem_flg = 0;

	sops[1].sem_num = (unsigned short)(index * 2); // r_sem of section index
	sops[1].sem_op = 1;							   // lock
	sops[1].sem_flg = SEM_UNDO;					   // undo on terminate

	// Read lock on any specific section will also acquire single read lock on "all section"
	// so that write lock on all section only need to acquire single write on on "all section"
	// rather than to acquire multiple write locks on all the available sections.
	if (index != BBS_max_section)
	{
		sops[2].sem_num = BBS_max_section * 2 + 1; // w_sem of all section
		sops[2].sem_op = 0;						   // wait until unlocked
		sops[2].sem_flg = 0;

		sops[3].sem_num = BBS_max_section * 2; // r_sem of all section
		sops[3].sem_op = 1;					   // lock
		sops[3].sem_flg = SEM_UNDO;			   // undo on terminate
	}

	timeout.tv_sec = wait_sec;
	timeout.tv_nsec = 0;

	ret = semtimedop(p_section_list_pool->semid, sops, (index == BBS_max_section ? 2 : 4), &timeout);
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semtimedop(index = %d, lock read) error %d\n", index, errno);
	}

	return ret;
}

int section_list_try_rw_lock(SECTION_LIST *p_section, int wait_sec)
{
	int index;
	struct sembuf sops[3];
	struct timespec timeout;
	int ret;

	index = get_section_index(p_section);
	if (index < 0)
	{
		return -2;
	}

	sops[0].sem_num = (unsigned short)(index * 2 + 1); // w_sem of section index
	sops[0].sem_op = 0;								   // wait until unlocked
	sops[0].sem_flg = 0;

	sops[1].sem_num = (unsigned short)(index * 2 + 1); // w_sem of section index
	sops[1].sem_op = 1;								   // lock
	sops[1].sem_flg = SEM_UNDO;						   // undo on terminate

	sops[2].sem_num = (unsigned short)(index * 2); // r_sem of section index
	sops[2].sem_op = 0;							   // wait until unlocked
	sops[2].sem_flg = 0;

	timeout.tv_sec = wait_sec;
	timeout.tv_nsec = 0;

	ret = semtimedop(p_section_list_pool->semid, sops, 3, &timeout);
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semtimedop(index = %d, lock write) error %d\n", index, errno);
	}

	return ret;
}

int section_list_rd_unlock(SECTION_LIST *p_section)
{
	int index;
	struct sembuf sops[2];
	int ret;

	index = get_section_index(p_section);
	if (index < 0)
	{
		return -2;
	}

	sops[0].sem_num = (unsigned short)(index * 2); // r_sem of section index
	sops[0].sem_op = -1;						   // unlock
	sops[0].sem_flg = IPC_NOWAIT | SEM_UNDO;	   // no wait

	if (index != BBS_max_section)
	{
		sops[1].sem_num = BBS_max_section * 2;	 // r_sem of all section
		sops[1].sem_op = -1;					 // unlock
		sops[1].sem_flg = IPC_NOWAIT | SEM_UNDO; // no wait
	}

	ret = semop(p_section_list_pool->semid, sops, (index == BBS_max_section ? 1 : 2));
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semop(index = %d, unlock read) error %d\n", index, errno);
	}

	return ret;
}

int section_list_rw_unlock(SECTION_LIST *p_section)
{
	int index;
	struct sembuf sops[1];
	int ret;

	index = get_section_index(p_section);
	if (index < 0)
	{
		return -2;
	}

	sops[0].sem_num = (unsigned short)(index * 2 + 1); // w_sem of section index
	sops[0].sem_op = -1;							   // unlock
	sops[0].sem_flg = IPC_NOWAIT | SEM_UNDO;		   // no wait

	ret = semop(p_section_list_pool->semid, sops, 1);
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semop(index = %d, unlock write) error %d\n", index, errno);
	}

	return ret;
}

int section_list_rd_lock(SECTION_LIST *p_section)
{
	int timer = 0;
	int sid = (p_section == NULL ? 0 : p_section->sid);
	int ret = -1;

	while (!SYS_server_exit)
	{
		ret = section_list_try_rd_lock(p_section, SECTION_TRY_LOCK_WAIT_TIME);
		if (ret == 0) // success
		{
			break;
		}
		else if (errno == EAGAIN || errno == EINTR) // retry
		{
			timer++;
			if (timer % SECTION_TRY_LOCK_TIMES == 0)
			{
				log_error("section_list_rd_lock() tried %d times on section %d\n", sid, timer);
			}
		}
		else // failed
		{
			log_error("section_list_rd_lock() failed on section %d\n", sid);
			break;
		}
	}

	return ret;
}

int section_list_rw_lock(SECTION_LIST *p_section)
{
	int timer = 0;
	int sid = (p_section == NULL ? 0 : p_section->sid);
	int ret = -1;

	while (!SYS_server_exit)
	{
		ret = section_list_try_rw_lock(p_section, SECTION_TRY_LOCK_WAIT_TIME);
		if (ret == 0) // success
		{
			break;
		}
		else if (errno == EAGAIN || errno == EINTR) // retry
		{
			timer++;
			if (timer % SECTION_TRY_LOCK_TIMES == 0)
			{
				log_error("acquire_section_rw_lock() tried %d times on section %d\n", sid, timer);
			}
		}
		else // failed
		{
			log_error("acquire_section_rw_lock() failed on section %d\n", sid);
			break;
		}
	}

	return ret;
}
