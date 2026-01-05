/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * section_list
 *   - data models and basic operations of section and article
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "log.h"
#include "section_list.h"
#include "trie_dict.h"
#include "user_list.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/sem.h>
#include <sys/stat.h>

#if defined(_SEM_SEMUN_UNDEFINED) || defined(__CYGWIN__)
union semun
{
	int val;			   /* Value for SETVAL */
	struct semid_ds *buf;  /* Buffer for IPC_STAT, IPC_SET */
	unsigned short *array; /* Array for GETALL, SETALL */
	struct seminfo *__buf; /* Buffer for IPC_INFO
							  (Linux-specific) */
};
#endif // #if defined(_SEM_SEMUN_UNDEFINED)

enum _section_list_constant_t
{
	SECTION_TRY_LOCK_WAIT_TIME = 1, // second
	SECTION_TRY_LOCK_TIMES = 10,
	SECTION_DEAD_LOCK_TIMEOUT = 15, // second

	ARTICLE_BLOCK_PER_SHM = 1000,		// sizeof(ARTICLE_BLOCK) * ARTICLE_BLOCK_PER_SHM is the size of each shm segment to allocate
	ARTICLE_BLOCK_SHM_COUNT_LIMIT = 80, // limited by length (8-bit) of proj_id in ftok(path, proj_id)
	ARTICLE_BLOCK_PER_POOL = (ARTICLE_BLOCK_PER_SHM * ARTICLE_BLOCK_SHM_COUNT_LIMIT),

	CALCULATE_PAGE_THRESHOLD = 100, // Adjust to tune performance of moving topic between sections

	SID_STR_LEN = 5, // 32-bit + NULL
};

struct article_block_t
{
	ARTICLE articles[BBS_article_count_per_block];
	int article_count;
	struct article_block_t *p_next_block;
};
typedef struct article_block_t ARTICLE_BLOCK;

struct article_block_pool_t
{
	size_t shm_size;
	struct
	{
		size_t shm_size;
		void *p_shm;
	} shm_pool[ARTICLE_BLOCK_SHM_COUNT_LIMIT];
	int shm_count;
	ARTICLE_BLOCK *p_block_free_list;
	ARTICLE_BLOCK *p_block[ARTICLE_BLOCK_PER_POOL];
	int block_count;
};
typedef struct article_block_pool_t ARTICLE_BLOCK_POOL;

static char article_block_shm_name[FILE_NAME_LEN];
static ARTICLE_BLOCK_POOL *p_article_block_pool = NULL;

static char section_list_shm_name[FILE_NAME_LEN];
SECTION_LIST_POOL *p_section_list_pool = NULL;

#ifndef HAVE_SYSTEM_V
static int section_list_reset_lock(SECTION_LIST *p_section);
#endif

int article_block_init(const char *filename, int block_count)
{
	char filepath[FILE_PATH_LEN];
	int fd;
	size_t size;
	void *p_shm;
	int i;
	int block_count_in_shm;
	ARTICLE_BLOCK *p_block_in_shm;
	ARTICLE_BLOCK **pp_block_next;

	if (p_article_block_pool != NULL)
	{
		log_error("article_block_pool already initialized");
		return -1;
	}

	if (block_count <= 0 || block_count > ARTICLE_BLOCK_PER_POOL)
	{
		log_error("article_block_count exceed limit %d", ARTICLE_BLOCK_PER_POOL);
		return -2;
	}

	strncpy(filepath, filename, sizeof(filepath) - 1);
	filepath[sizeof(filepath) - 1] = '\0';
	snprintf(article_block_shm_name, sizeof(article_block_shm_name), "/ARTICLE_BLOCK_SHM_%s", basename(filepath));

	// Allocate shared memory
	size = sizeof(ARTICLE_BLOCK_POOL);
	snprintf(filepath, sizeof(filepath), "%s_%d", article_block_shm_name, ARTICLE_BLOCK_SHM_COUNT_LIMIT);

	if (shm_unlink(filepath) == -1 && errno != ENOENT)
	{
		log_error("shm_unlink(%s) error (%d)", filepath, errno);
		return -2;
	}

	if ((fd = shm_open(filepath, O_CREAT | O_EXCL | O_RDWR, 0600)) == -1)
	{
		log_error("shm_open(%s) error (%d)", filepath, errno);
		return -2;
	}
	if (ftruncate(fd, (off_t)size) == -1)
	{
		log_error("ftruncate(size=%d) error (%d)", size, errno);
		close(fd);
		return -2;
	}

	p_shm = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0L);
	if (p_shm == MAP_FAILED)
	{
		log_error("mmap() error (%d)", errno);
		close(fd);
		return -2;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)", errno);
		return -1;
	}

	p_article_block_pool = p_shm;
	p_article_block_pool->shm_size = size;

	p_article_block_pool->shm_count = 0;
	pp_block_next = &(p_article_block_pool->p_block_free_list);

	while (block_count > 0)
	{
		block_count_in_shm = MIN(block_count, ARTICLE_BLOCK_PER_SHM);
		block_count -= block_count_in_shm;

		size = sizeof(ARTICLE_BLOCK) * (size_t)block_count_in_shm;
		snprintf(filepath, sizeof(filepath), "%s_%d", article_block_shm_name, p_article_block_pool->shm_count);

		if (shm_unlink(filepath) == -1 && errno != ENOENT)
		{
			log_error("shm_unlink(%s) error (%d)", filepath, errno);
			return -2;
		}

		if ((fd = shm_open(filepath, O_CREAT | O_EXCL | O_RDWR, 0600)) == -1)
		{
			log_error("shm_open(%s) error (%d)", filepath, errno);
			return -2;
		}
		if (ftruncate(fd, (off_t)size) == -1)
		{
			log_error("ftruncate(size=%d) error (%d)", size, errno);
			close(fd);
			return -2;
		}

		p_shm = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0L);
		if (p_shm == MAP_FAILED)
		{
			log_error("mmap() error (%d)", errno);
			close(fd);
			return -2;
		}

		if (close(fd) < 0)
		{
			log_error("close(fd) error (%d)", errno);
			return -1;
		}

		(p_article_block_pool->shm_pool + p_article_block_pool->shm_count)->shm_size = size;
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
	char filepath[FILE_PATH_LEN];

	if (p_article_block_pool == NULL)
	{
		return;
	}

	for (int i = 0; i < p_article_block_pool->shm_count; i++)
	{
		snprintf(filepath, sizeof(filepath), "%s_%d", article_block_shm_name, i);

		if (shm_unlink(filepath) == -1 && errno != ENOENT)
		{
			log_error("shm_unlink(%s) error (%d)", filepath, errno);
		}
	}

	snprintf(filepath, sizeof(filepath), "%s_%d", article_block_shm_name, ARTICLE_BLOCK_SHM_COUNT_LIMIT);

	if (shm_unlink(filepath) == -1 && errno != ENOENT)
	{
		log_error("shm_unlink(%s) error (%d)", filepath, errno);
	}

	detach_article_block_shm();
}

int set_article_block_shm_readonly(void)
{
	// int i;

	if (p_article_block_pool == NULL)
	{
		log_error("article_block_pool not initialized");
		return -1;
	}

	// for (i = 0; i < p_article_block_pool->shm_count; i++)
	// {
	// 	if ((p_article_block_pool->shm_pool + i)->p_shm != NULL &&
	// 		mprotect((p_article_block_pool->shm_pool + i)->p_shm, (p_article_block_pool->shm_pool + i)->shm_size, PROT_READ) < 0)
	// 	{
	// 		log_error("mprotect() error (%d)", errno);
	// 		return -2;
	// 	}
	// }

	if (p_article_block_pool != NULL &&
		mprotect(p_article_block_pool, p_article_block_pool->shm_size, PROT_READ) < 0)
	{
		log_error("mprotect() error (%d)", errno);
		return -3;
	}

	return 0;
}

int detach_article_block_shm(void)
{
	if (p_article_block_pool == NULL)
	{
		return -1;
	}

	for (int i = 0; i < p_article_block_pool->shm_count; i++)
	{
		if ((p_article_block_pool->shm_pool + i)->p_shm != NULL &&
			munmap((p_article_block_pool->shm_pool + i)->p_shm, (p_article_block_pool->shm_pool + i)->shm_size) < 0)
		{
			log_error("munmap() error (%d)", errno);
			return -2;
		}
	}

	if (p_article_block_pool != NULL && munmap(p_article_block_pool, p_article_block_pool->shm_size) < 0)
	{
		log_error("munmap() error (%d)", errno);
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
		log_error("article_block_pool not initialized");
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
		log_error("article_block_pool not initialized");
		return NULL;
	}

	if (p_article_block_pool->block_count == 0) // empty
	{
		return NULL;
	}

	left = 0;
	right = p_article_block_pool->block_count - 1;

	// aid in the range [ head aid of blocks[left], tail aid of blocks[right] ]
	while (left < right)
	{
		// get block offset no less than mid value of left and right block offsets
		mid = (left + right) / 2 + (left + right) % 2;

		if (aid < p_article_block_pool->p_block[mid]->articles[0].aid)
		{
			right = mid - 1;
		}
		else // if (aid >= p_article_block_pool->p_block[mid]->articles[0].aid)
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
		log_error("article_block_pool not initialized");
		return NULL;
	}

	if (index < 0 || index / BBS_article_count_per_block >= p_article_block_pool->block_count)
	{
		log_error("article_block_find_by_index(%d) is out of boundary of block [0, %d)", index, p_article_block_pool->block_count);
		return NULL;
	}

	p_block = p_article_block_pool->p_block[index / BBS_article_count_per_block];

	if (index % BBS_article_count_per_block >= p_block->article_count)
	{
		log_error("article_block_find_by_index(%d) is out of boundary of article [0, %d)", index, p_block->article_count);
		return NULL;
	}

	return (p_block->articles + (index % BBS_article_count_per_block));
}

extern int section_list_init(const char *filename)
{
	char filepath[FILE_PATH_LEN];
	int fd;
	size_t size;
	void *p_shm;
#ifdef HAVE_SYSTEM_V
	int proj_id;
	key_t key;
	int semid;
	union semun arg;
#endif
	int i;

	if (p_section_list_pool != NULL)
	{
		section_list_cleanup();
	}

	// Allocate shared memory
	size = sizeof(SECTION_LIST_POOL);

	strncpy(filepath, filename, sizeof(filepath) - 1);
	filepath[sizeof(filepath) - 1] = '\0';
	snprintf(section_list_shm_name, sizeof(section_list_shm_name), "/SECTION_LIST_SHM_%s", basename(filepath));

	if (shm_unlink(section_list_shm_name) == -1 && errno != ENOENT)
	{
		log_error("shm_unlink(%s) error (%d)", section_list_shm_name, errno);
		return -2;
	}

	if ((fd = shm_open(section_list_shm_name, O_CREAT | O_EXCL | O_RDWR, 0600)) == -1)
	{
		log_error("shm_open(%s) error (%d)", section_list_shm_name, errno);
		return -2;
	}
	if (ftruncate(fd, (off_t)size) == -1)
	{
		log_error("ftruncate(size=%d) error (%d)", size, errno);
		close(fd);
		return -2;
	}

	p_shm = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0L);
	if (p_shm == MAP_FAILED)
	{
		log_error("mmap() error (%d)", errno);
		close(fd);
		return -2;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)", errno);
		return -1;
	}

	p_section_list_pool = p_shm;
	p_section_list_pool->shm_size = size;
	p_section_list_pool->section_count = 0;

	// Allocate semaphore as section locks
#ifndef HAVE_SYSTEM_V
	for (i = 0; i <= BBS_max_section; i++)
	{
		if (sem_init(&(p_section_list_pool->sem[i]), 1, 1) == -1)
		{
			log_error("sem_init(sem[%d]) error (%d)", i, errno);
			return -3;
		}

		p_section_list_pool->read_lock_count[i] = 0;
		p_section_list_pool->write_lock_count[i] = 0;
	}
#else
	proj_id = (int)(time(NULL) % getpid());
	key = ftok(filename, proj_id);
	if (key == -1)
	{
		log_error("ftok(%s, %d) error (%d)", filename, proj_id, errno);
		return -3;
	}

	size = 2 * (BBS_max_section + 1); // r_sem and w_sem per section, the last pair for all sections
	semid = semget(key, (int)size, IPC_CREAT | IPC_EXCL | 0600);
	if (semid == -1)
	{
		log_error("semget(section_list_pool_sem, size = %d) error (%d)", size, errno);
		return -3;
	}

	// Initialize sem value to 0
	arg.val = 0;
	for (i = 0; i < size; i++)
	{
		if (semctl(semid, i, SETVAL, arg) == -1)
		{
			log_error("semctl(section_list_pool_sem, SETVAL) error (%d)", errno);
			return -3;
		}
	}

	p_section_list_pool->semid = semid;
#endif

	p_section_list_pool->p_trie_dict_section_by_name = trie_dict_create();
	if (p_section_list_pool->p_trie_dict_section_by_name == NULL)
	{
		log_error("trie_dict_create() OOM", BBS_max_section);
		return -2;
	}

	p_section_list_pool->p_trie_dict_section_by_sid = trie_dict_create();
	if (p_section_list_pool->p_trie_dict_section_by_sid == NULL)
	{
		log_error("trie_dict_create() OOM", BBS_max_section);
		return -2;
	}

	return 0;
}

void section_list_cleanup(void)
{
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

#ifdef HAVE_SYSTEM_V
	if (semctl(p_section_list_pool->semid, 0, IPC_RMID) == -1)
	{
		log_error("semctl(semid = %d, IPC_RMID) error (%d)", p_section_list_pool->semid, errno);
	}
#else
	for (int i = 0; i <= BBS_max_section; i++)
	{
		if (sem_destroy(&(p_section_list_pool->sem[i])) == -1)
		{
			log_error("sem_destroy(sem[%d]) error (%d)", i, errno);
		}
	}
#endif

	detach_section_list_shm();

	if (shm_unlink(section_list_shm_name) == -1 && errno != ENOENT)
	{
		log_error("shm_unlink(%s) error (%d)", section_list_shm_name, errno);
	}
}

int set_section_list_shm_readonly(void)
{
	if (p_section_list_pool == NULL)
	{
		log_error("p_section_list_pool not initialized");
		return -1;
	}

	if (p_section_list_pool != NULL &&
		mprotect(p_section_list_pool, p_section_list_pool->shm_size, PROT_READ) < 0)
	{
		log_error("mprotect() error (%d)", errno);
		return -2;
	}

	return 0;
}

int detach_section_list_shm(void)
{
	if (p_section_list_pool != NULL && munmap(p_section_list_pool, p_section_list_pool->shm_size) < 0)
	{
		log_error("munmap() error (%d)", errno);
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
		log_error("session_list_pool not initialized");
		return NULL;
	}

	if (p_section_list_pool->section_count >= BBS_max_section)
	{
		log_error("section_count reach limit %d >= %d", p_section_list_pool->section_count, BBS_max_section);
		return NULL;
	}

	sid_to_str(sid, sid_str);

	p_section = p_section_list_pool->sections + p_section_list_pool->section_count;

	p_section->sid = sid;
	p_section->ex_menu_tm = 0;

	strncpy(p_section->sname, sname, sizeof(p_section->sname) - 1);
	p_section->sname[sizeof(p_section->sname) - 1] = '\0';

	strncpy(p_section->stitle, stitle, sizeof(p_section->stitle) - 1);
	p_section->stitle[sizeof(p_section->stitle) - 1] = '\0';

	strncpy(p_section->master_list, master_list, sizeof(p_section->master_list) - 1);
	p_section->master_list[sizeof(p_section->master_list) - 1] = '\0';

	if (trie_dict_set(p_section_list_pool->p_trie_dict_section_by_name, sname, p_section_list_pool->section_count) != 1)
	{
		log_error("trie_dict_set(section, %s, %d) error", sname, p_section_list_pool->section_count);
		return NULL;
	}

	if (trie_dict_set(p_section_list_pool->p_trie_dict_section_by_sid, sid_str, p_section_list_pool->section_count) != 1)
	{
		log_error("trie_dict_set(section, %d, %d) error", sid, p_section_list_pool->section_count);
		return NULL;
	}

	section_list_reset_articles(p_section);

	p_section_list_pool->section_count++;

	return p_section;
}

int section_list_update(SECTION_LIST *p_section, const char *sname, const char *stitle, const char *master_list)
{
	int64_t index;

	if (p_section == NULL || sname == NULL || stitle == NULL || master_list == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	index = get_section_index(p_section);

	strncpy(p_section->sname, sname, sizeof(p_section->sname) - 1);
	p_section->sname[sizeof(p_section->sname) - 1] = '\0';

	strncpy(p_section->stitle, stitle, sizeof(p_section->stitle) - 1);
	p_section->stitle[sizeof(p_section->stitle) - 1] = '\0';

	strncpy(p_section->master_list, master_list, sizeof(p_section->master_list) - 1);
	p_section->master_list[sizeof(p_section->master_list) - 1] = '\0';

	if (trie_dict_set(p_section_list_pool->p_trie_dict_section_by_name, sname, index) < 0)
	{
		log_error("trie_dict_set(section, %s, %d) error", sname, index);
		return -2;
	}

	return 0;
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

	p_section->ontop_article_count = 0;
}

SECTION_LIST *section_list_find_by_name(const char *sname)
{
	int64_t index;
	int ret;

	if (p_section_list_pool == NULL)
	{
		log_error("section_list not initialized");
		return NULL;
	}

	ret = trie_dict_get(p_section_list_pool->p_trie_dict_section_by_name, sname, &index);
	if (ret < 0)
	{
		log_error("trie_dict_get(section, %s) error", sname);
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
		log_error("section_list not initialized");
		return NULL;
	}

	sid_to_str(sid, sid_str);

	ret = trie_dict_get(p_section_list_pool->p_trie_dict_section_by_sid, sid_str, &index);
	if (ret < 0)
	{
		log_error("trie_dict_get(section, %d) error", sid);
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
		log_error("NULL pointer error");
		return -1;
	}

	if (p_article_block_pool == NULL)
	{
		log_error("article_block_pool not initialized");
		return -1;
	}

	if (p_section->sid != p_article_src->sid)
	{
		log_error("section_list_append_article() error: section sid %d != article sid %d", p_section->sid, p_article_src->sid);
		return -2;
	}

	if (p_section->article_count >= BBS_article_limit_per_section)
	{
		log_error("section_list_append_article() error: article_count reach limit in section %d", p_section->sid);
		return -2;
	}

	if (p_article_block_pool->block_count == 0 ||
		p_article_block_pool->p_block[p_article_block_pool->block_count - 1]->article_count >= BBS_article_count_per_block)
	{
		if ((p_block = pop_free_article_block()) == NULL)
		{
			log_error("pop_free_article_block() error");
			return -2;
		}

		if (p_article_block_pool->block_count > 0)
		{
			last_aid = p_article_block_pool->p_block[p_article_block_pool->block_count - 1]->articles[BBS_article_count_per_block - 1].aid;
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
		log_error("section_list_append_article(aid=%d) error: last_aid=%d", p_article_src->aid, last_aid);
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
			log_error("search head of topic (aid=%d) error", p_article->tid);
			return -4;
		}

		p_topic_tail = p_topic_head->p_topic_prior;
		if (p_topic_tail == NULL)
		{
			log_error("tail of topic (aid=%d) is NULL", p_article->tid);
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
	if ((p_article->visible && p_section->last_page_visible_article_count == BBS_article_limit_per_page) ||
		p_section->page_count == 0)
	{
		p_section->p_page_first_article[p_section->page_count] = p_article;
		p_section->page_count++;
		p_section->last_page_visible_article_count = 0;
	}

	if (p_article->visible)
	{
		p_section->last_page_visible_article_count++;
	}

	if (p_article->ontop && section_list_update_article_ontop(p_section, p_article) < 0)
	{
		log_error("section_list_update_article_ontop(sid=%d, aid=%d) error",
				  p_section->sid, p_article->aid);
		return -5;
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
		log_error("NULL pointer error");
		return -1;
	}

	p_article = article_block_find_by_aid(aid);
	if (p_article == NULL)
	{
		return -1; // Not found
	}

	if (p_section->sid != p_article->sid)
	{
		log_error("Inconsistent section sid %d != article sid %d", p_section->sid, p_article->sid);
		return -2;
	}

	if (p_article->visible == visible)
	{
		return 0; // Already set
	}

	if (visible == 0) // 1 -> 0
	{
		p_section->visible_article_count--;

		if (user_article_cnt_inc(p_article->uid, -1) < 0)
		{
			log_error("user_article_cnt_inc(uid=%d, -1) error", p_article->uid);
		}

		if (p_article->tid == 0)
		{
			p_section->visible_topic_count--;

			// Set related visible replies to invisible
			for (p_reply = p_article->p_topic_next; p_reply->tid != 0; p_reply = p_reply->p_topic_next)
			{
				if (p_reply->tid != aid)
				{
					log_error("Inconsistent tid = %d found in reply %d of topic %d", p_reply->tid, p_reply->aid, aid);
					continue;
				}

				if (p_reply->visible == 1)
				{
					p_reply->visible = 0;
					p_section->visible_article_count--;
					affected_count++;

					if (user_article_cnt_inc(p_reply->uid, -1) < 0)
					{
						log_error("user_article_cnt_inc(uid=%d, -1) error", p_reply->uid);
					}
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

		if (user_article_cnt_inc(p_article->uid, 1) < 0)
		{
			log_error("user_article_cnt_inc(uid=%d, 1) error", p_article->uid);
		}
	}

	p_article->visible = visible;
	affected_count++;

	return affected_count;
}

int section_list_set_article_excerption(SECTION_LIST *p_section, int32_t aid, int8_t excerption)
{
	ARTICLE *p_article;

	if (p_section == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	p_article = article_block_find_by_aid(aid);
	if (p_article == NULL)
	{
		return -1; // Not found
	}

	if (p_section->sid != p_article->sid)
	{
		log_error("Inconsistent section sid %d != article sid %d", p_section->sid, p_article->sid);
		return -2;
	}

	if (p_article->excerption == excerption)
	{
		return 0; // Already set
	}

	p_article->excerption = excerption;

	return 1;
}

int section_list_update_article_ontop(SECTION_LIST *p_section, ARTICLE *p_article)
{
	int i;

	if (p_section == NULL || p_article == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	if (p_section->sid != p_article->sid)
	{
		log_error("Inconsistent section sid %d != article sid %d", p_section->sid, p_article->sid);
		return -2;
	}

	if (p_article->ontop)
	{
		for (i = 0; i < p_section->ontop_article_count; i++)
		{
			if (p_section->p_ontop_articles[i]->aid == p_article->aid)
			{
				log_error("Inconsistent state found: article %d already ontop in section %d", p_article->aid, p_section->sid);
				return 0;
			}
			else if (p_section->p_ontop_articles[i]->aid > p_article->aid)
			{
				break;
			}
		}

		// Remove the oldest one if the array of ontop articles is full
		if (p_section->ontop_article_count >= BBS_ontop_article_limit_per_section)
		{
			if (i == 0) // p_article is the oldest one
			{
				return 0;
			}
			memmove((void *)(p_section->p_ontop_articles),
					(void *)(p_section->p_ontop_articles + 1),
					sizeof(ARTICLE *) * (size_t)(i - 1));
			p_section->ontop_article_count--;
			i--;
		}
		else
		{
			memmove((void *)(p_section->p_ontop_articles + i + 1),
					(void *)(p_section->p_ontop_articles + i),
					sizeof(ARTICLE *) * (size_t)(p_section->ontop_article_count - i));
		}

		p_section->p_ontop_articles[i] = p_article;
		p_section->ontop_article_count++;

		// TODO: debug
	}
	else // ontop == 0
	{
		for (i = 0; i < p_section->ontop_article_count; i++)
		{
			if (p_section->p_ontop_articles[i]->aid == p_article->aid)
			{
				break;
			}
		}
		if (i == p_section->ontop_article_count) // not found
		{
			log_error("Inconsistent state found: article %d not ontop in section %d", p_article->aid, p_section->sid);
			return 0;
		}

		memmove((void *)(p_section->p_ontop_articles + i),
				(void *)(p_section->p_ontop_articles + i + 1),
				sizeof(ARTICLE *) * (size_t)(p_section->ontop_article_count - i - 1));
		p_section->ontop_article_count--;
	}

	return 0;
}

int section_list_page_count_with_ontop(SECTION_LIST *p_section)
{
	int page_count;

	if (p_section == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	page_count = p_section->page_count - 1 +
				 (p_section->last_page_visible_article_count + p_section->ontop_article_count + BBS_article_limit_per_page - 1) /
					 BBS_article_limit_per_page;

	if (page_count < 0)
	{
		page_count = 0;
	}

	return page_count;
}

int section_list_page_article_count_with_ontop(SECTION_LIST *p_section, int32_t page_id)
{
	if (p_section == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	if (page_id < p_section->page_count - 1)
	{
		return BBS_article_limit_per_page;
	}
	else // if (page_id >= p_section->page_count - 1)
	{
		return MIN(MAX(0,
					   (p_section->last_page_visible_article_count + p_section->ontop_article_count -
						BBS_article_limit_per_page * (page_id - p_section->page_count + 1))),
				   BBS_article_limit_per_page);
	}
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
		log_error("NULL pointer error");
		return NULL;
	}

	if (p_section->article_count == 0) // empty
	{
		*p_page = 0;
		*p_offset = 0;
		return NULL;
	}

	left = 0;
	right = p_section->page_count - 1;

	// aid in the range [ head aid of pages[left], tail aid of pages[right] ]
	while (left < right)
	{
		// get page id no less than mid value of left page id and right page id
		mid = (left + right) / 2 + (left + right) % 2;

		if (aid < p_section->p_page_first_article[mid]->aid)
		{
			right = mid - 1;
		}
		else // if (aid < p_section->p_page_first_article[mid]->aid)
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
		log_error("NULL pointer error");
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
			log_error("section_list_calculate_page() error: section sid %d != start article sid %d", p_section->sid, p_article->sid);
			return -2;
		}

		p_article = section_list_find_article_with_offset(p_section, start_aid, &page, &offset, &p_next);
		if (p_article == NULL)
		{
			if (page < 0)
			{
				return -1;
			}
			log_error("section_list_calculate_page() aid = %d not found in section sid = %d",
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
				log_error("Count of page exceed limit in section %d", p_section->sid);
				break;
			}
		}
	} while (p_article != p_section->p_article_head);

	p_section->page_count = page + (visible_article_count > 0 ? 1 : 0);
	p_section->last_page_visible_article_count = (visible_article_count > 0
													  ? visible_article_count
													  : (page > 0 ? BBS_article_limit_per_page : 0));

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

	ret = (p_article_block_pool->block_count - 1) * BBS_article_count_per_block +
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
			log_error("article_count_of_topic(%d) error: article %d not linked to the topic", aid, p_article->aid);
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
		log_error("NULL pointer error");
		return -1;
	}

	if (p_section_src->sid == p_section_dest->sid)
	{
		log_error("section_list_move_topic() src and dest section are the same");
		return -1;
	}

	if ((p_article = article_block_find_by_aid(aid)) == NULL)
	{
		log_error("article_block_find_by_aid(aid = %d) error: article not found", aid);
		return -2;
	}

	if (p_section_src->sid != p_article->sid)
	{
		log_error("section_list_move_topic() error: src section sid %d != article %d sid %d",
				  p_section_src->sid, p_article->aid, p_article->sid);
		return -2;
	}

	if (p_article->tid != 0)
	{
		log_error("section_list_move_topic(aid = %d) error: article is not head of topic, tid = %d", aid, p_article->tid);
		return -2;
	}

	last_unaffected_aid_src = (p_article == p_section_src->p_article_head ? 0 : p_article->p_prior->aid);

	move_article_count = article_count_of_topic(aid);
	if (move_article_count <= 0)
	{
		log_error("article_count_of_topic(aid = %d) <= 0", aid);
		return -2;
	}

	if (p_section_dest->article_count + move_article_count > BBS_article_limit_per_section)
	{
		log_error("section_list_move_topic() error: article_count %d reach limit in section %d",
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
			log_error("section_list_move_topic() warning: src section sid %d != article %d sid %d",
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
			log_error("section_list_move_topic() warning: article %d already in section %d", p_article->aid, p_section_dest->sid);
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
				log_error("section_list_calculate_page(dest section = %d, aid = %d) error",
						  p_section_dest->sid, first_inserted_aid_dest);
			}

			first_inserted_aid_dest = p_article->aid;
		}
	} while (p_article->aid != aid);

	if (p_section_dest->article_count - dest_article_count_old != move_article_count)
	{
		log_error("section_list_move_topic() warning: count of moved articles %d != %d",
				  p_section_dest->article_count - dest_article_count_old, move_article_count);
	}

	// Re-calculate pages of src section
	if (section_list_calculate_page(p_section_src, last_unaffected_aid_src) < 0)
	{
		log_error("section_list_calculate_page(src section = %d, aid = %d) error at aid = %d",
				  p_section_src->sid, last_unaffected_aid_src, aid);
	}

	if (move_counter % CALCULATE_PAGE_THRESHOLD != 0)
	{
		// Re-calculate pages of desc section
		if (section_list_calculate_page(p_section_dest, first_inserted_aid_dest) < 0)
		{
			log_error("section_list_calculate_page(dest section = %d, aid = %d) error",
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
		log_error("get_section_index() error: uninitialized");
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
			log_error("get_section_index(%d) error: index out of range", index);
			return -2;
		}
	}

	return index;
}

int get_section_info(SECTION_LIST *p_section, char *sname, char *stitle, char *master_list)
{
	if (p_section == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	if (section_list_rd_lock(p_section) < 0)
	{
		log_error("section_list_rd_lock(sid=%d) error", p_section->sid);
		return -2;
	}

	if (sname != NULL)
	{
		memcpy(sname, p_section->sname, sizeof(p_section->sname));
	}
	if (stitle != NULL)
	{
		memcpy(stitle, p_section->stitle, sizeof(p_section->stitle));
	}
	if (master_list != NULL)
	{
		memcpy(master_list, p_section->master_list, sizeof(p_section->master_list));
	}

	// release lock of section
	if (section_list_rd_unlock(p_section) < 0)
	{
		log_error("section_list_rd_unlock(sid=%d) error", p_section->sid);
		return -2;
	}

	return 0;
}

int section_list_try_rd_lock(SECTION_LIST *p_section, int wait_sec)
{
	int index;
#ifdef HAVE_SYSTEM_V
	struct sembuf sops[4];
#endif
	struct timespec timeout;
	int ret = 0;

	index = get_section_index(p_section);
	if (index < 0)
	{
		return -2;
	}

	timeout.tv_sec = wait_sec;
	timeout.tv_nsec = 0;

#ifdef HAVE_SYSTEM_V
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

	ret = semtimedop(p_section_list_pool->semid, sops, (index == BBS_max_section ? 2 : 4), &timeout);
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semop(index = %d, lock read) error %d", index, errno);
	}
#else
	if (sem_timedwait(&(p_section_list_pool->sem[index]), &timeout) == -1)
	{
		if (errno != ETIMEDOUT && errno != EAGAIN && errno != EINTR)
		{
			log_error("sem_timedwait(sem[%d]) error %d", index, errno);
		}
		return -1;
	}

	if (index != BBS_max_section)
	{
		if (sem_timedwait(&(p_section_list_pool->sem[BBS_max_section]), &timeout) == -1)
		{
			if (errno != ETIMEDOUT && errno != EAGAIN && errno != EINTR)
			{
				log_error("sem_timedwait(sem[%d]) error %d", BBS_max_section, errno);
			}
			// release previously acquired lock
			if (sem_post(&(p_section_list_pool->sem[index])) == -1)
			{
				log_error("sem_post(sem[%d]) error %d", index, errno);
			}
			return -1;
		}
	}

	if (p_section_list_pool->write_lock_count[index] == 0 &&
		(index != BBS_max_section && p_section_list_pool->write_lock_count[BBS_max_section] == 0))
	{
		p_section_list_pool->read_lock_count[index]++;
		if (index != BBS_max_section)
		{
			p_section_list_pool->read_lock_count[BBS_max_section]++;
		}
	}
	else
	{
		errno = EAGAIN;
		ret = -1;
	}

	if (index != BBS_max_section)
	{
		// release lock on "all section"
		if (sem_post(&(p_section_list_pool->sem[BBS_max_section])) == -1)
		{
			log_error("sem_post(sem[%d]) error %d", BBS_max_section, errno);
			ret = -1;
		}
	}

	if (sem_post(&(p_section_list_pool->sem[index])) == -1)
	{
		log_error("sem_post(sem[%d]) error %d", index, errno);
		return -1;
	}
#endif

	return ret;
}

int section_list_try_rw_lock(SECTION_LIST *p_section, int wait_sec)
{
	int index;
#ifdef HAVE_SYSTEM_V
	struct sembuf sops[3];
#endif
	struct timespec timeout;
	int ret = 0;

	index = get_section_index(p_section);
	if (index < 0)
	{
		return -2;
	}

	timeout.tv_sec = wait_sec;
	timeout.tv_nsec = 0;

#ifdef HAVE_SYSTEM_V
	sops[0].sem_num = (unsigned short)(index * 2 + 1); // w_sem of section index
	sops[0].sem_op = 0;								   // wait until unlocked
	sops[0].sem_flg = 0;

	sops[1].sem_num = (unsigned short)(index * 2 + 1); // w_sem of section index
	sops[1].sem_op = 1;								   // lock
	sops[1].sem_flg = SEM_UNDO;						   // undo on terminate

	sops[2].sem_num = (unsigned short)(index * 2); // r_sem of section index
	sops[2].sem_op = 0;							   // wait until unlocked
	sops[2].sem_flg = 0;

	ret = semtimedop(p_section_list_pool->semid, sops, 3, &timeout);
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semop(index = %d, lock write) error %d", index, errno);
	}
#else
	if (sem_timedwait(&(p_section_list_pool->sem[index]), &timeout) == -1)
	{
		if (errno != ETIMEDOUT && errno != EAGAIN && errno != EINTR)
		{
			log_error("sem_timedwait(sem[%d]) error %d", index, errno);
		}
		return -1;
	}

	if (p_section_list_pool->read_lock_count[index] == 0 && p_section_list_pool->write_lock_count[index] == 0)
	{
		p_section_list_pool->write_lock_count[index]++;
	}
	else
	{
		errno = EAGAIN;
		ret = -1;
	}

	if (sem_post(&(p_section_list_pool->sem[index])) == -1)
	{
		log_error("sem_post(sem[%d]) error %d", index, errno);
		return -1;
	}
#endif

	return ret;
}

int section_list_rd_unlock(SECTION_LIST *p_section)
{
	int index;
#ifdef HAVE_SYSTEM_V
	struct sembuf sops[2];
#endif
	int ret = 0;

	index = get_section_index(p_section);
	if (index < 0)
	{
		return -2;
	}

#ifdef HAVE_SYSTEM_V
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
		log_error("semop(index = %d, unlock read) error %d", index, errno);
	}
#else
	if (sem_wait(&(p_section_list_pool->sem[index])) == -1)
	{
		if (errno != ETIMEDOUT && errno != EAGAIN && errno != EINTR)
		{
			log_error("sem_wait(sem[%d]) error %d", index, errno);
		}
		return -1;
	}

	if (index != BBS_max_section)
	{
		if (sem_wait(&(p_section_list_pool->sem[BBS_max_section])) == -1)
		{
			if (errno != ETIMEDOUT && errno != EAGAIN && errno != EINTR)
			{
				log_error("sem_wait(sem[%d]) error %d", BBS_max_section, errno);
			}
			// release previously acquired lock
			if (sem_post(&(p_section_list_pool->sem[index])) == -1)
			{
				log_error("sem_post(sem[%d]) error %d", index, errno);
			}
			return -1;
		}
	}

	if (p_section_list_pool->read_lock_count[index] > 0)
	{
		p_section_list_pool->read_lock_count[index]--;
	}
	else
	{
		log_error("read_lock_count[%d] already 0", index);
	}

	if (index != BBS_max_section && p_section_list_pool->read_lock_count[BBS_max_section] > 0)
	{
		p_section_list_pool->read_lock_count[BBS_max_section]--;
	}
	else
	{
		log_error("read_lock_count[%d] already 0", BBS_max_section);
	}

	if (index != BBS_max_section)
	{
		// release lock on "all section"
		if (sem_post(&(p_section_list_pool->sem[BBS_max_section])) == -1)
		{
			log_error("sem_post(sem[%d]) error %d", BBS_max_section, errno);
			ret = -1;
		}
	}

	if (sem_post(&(p_section_list_pool->sem[index])) == -1)
	{
		log_error("sem_post(sem[%d]) error %d", index, errno);
		return -1;
	}
#endif

	return ret;
}

int section_list_rw_unlock(SECTION_LIST *p_section)
{
	int index;
#ifdef HAVE_SYSTEM_V
	struct sembuf sops[1];
#endif
	int ret = 0;

	index = get_section_index(p_section);
	if (index < 0)
	{
		return -2;
	}

#ifdef HAVE_SYSTEM_V
	sops[0].sem_num = (unsigned short)(index * 2 + 1); // w_sem of section index
	sops[0].sem_op = -1;							   // unlock
	sops[0].sem_flg = IPC_NOWAIT | SEM_UNDO;		   // no wait

	ret = semop(p_section_list_pool->semid, sops, 1);
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semop(index = %d, unlock write) error %d", index, errno);
	}
#else
	if (sem_wait(&(p_section_list_pool->sem[index])) == -1)
	{
		if (errno != ETIMEDOUT && errno != EAGAIN && errno != EINTR)
		{
			log_error("sem_wait(sem[%d]) error %d", index, errno);
		}
		return -1;
	}

	if (p_section_list_pool->write_lock_count[index] > 0)
	{
		p_section_list_pool->write_lock_count[index]--;
	}
	else
	{
		log_error("write_lock_count[%d] already 0", index);
	}

	if (sem_post(&(p_section_list_pool->sem[index])) == -1)
	{
		log_error("sem_post(sem[%d]) error %d", index, errno);
		return -1;
	}
#endif

	return ret;
}

int section_list_rd_lock(SECTION_LIST *p_section)
{
	int timer = 0;
	int sid = (p_section == NULL ? 0 : p_section->sid);
	int ret = -1;
	time_t tm_first_failure = 0;

	while (!SYS_server_exit)
	{
		ret = section_list_try_rd_lock(p_section, SECTION_TRY_LOCK_WAIT_TIME);
		if (ret == 0) // success
		{
			break;
		}
		else if (errno == EAGAIN || errno == EINTR) // retry
		{
			// Dead lock detection
			if (tm_first_failure == 0)
			{
				time(&tm_first_failure);
			}

			timer++;
			if (timer % SECTION_TRY_LOCK_TIMES == 0)
			{
				log_error("section_list_try_rd_lock() tried %d times on section %d", timer, sid);
				if (time(NULL) - tm_first_failure >= SECTION_DEAD_LOCK_TIMEOUT)
				{
					log_error("Unable to acquire rd_lock for %d seconds", time(NULL) - tm_first_failure);
#ifndef HAVE_SYSTEM_V
					section_list_reset_lock(p_section);
					log_error("Reset POSIX semaphore to resolve dead lock");
#endif
					break;
				}
			}
			usleep(100 * 1000); // 0.1 second
		}
		else // failed
		{
			log_error("section_list_try_rd_lock() failed on section %d", sid);
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
	time_t tm_first_failure = 0;

	while (!SYS_server_exit)
	{
		ret = section_list_try_rw_lock(p_section, SECTION_TRY_LOCK_WAIT_TIME);
		if (ret == 0) // success
		{
			break;
		}
		else if (errno == EAGAIN || errno == EINTR) // retry
		{
			// Dead lock detection
			if (tm_first_failure == 0)
			{
				time(&tm_first_failure);
			}

			timer++;
			if (timer % SECTION_TRY_LOCK_TIMES == 0)
			{
				log_error("section_list_try_rw_lock() tried %d times on section %d", timer, sid);
				if (time(NULL) - tm_first_failure >= SECTION_DEAD_LOCK_TIMEOUT)
				{
					log_error("Unable to acquire rw_lock for %d seconds", time(NULL) - tm_first_failure);
#ifndef HAVE_SYSTEM_V
					section_list_reset_lock(p_section);
					log_error("Reset POSIX semaphore to resolve dead lock");
#endif
					break;
				}
			}
			usleep(100 * 1000); // 0.1 second
		}
		else // failed
		{
			log_error("section_list_try_rw_lock() failed on section %d", sid);
			break;
		}
	}

	return ret;
}

#ifndef HAVE_SYSTEM_V
int section_list_reset_lock(SECTION_LIST *p_section)
{
	int index;

	if (p_section == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	index = get_section_index(p_section);
	if (index < 0)
	{
		return -2;
	}

	if (sem_destroy(&(p_section_list_pool->sem[index])) == -1)
	{
		log_error("sem_destroy(sem[%d]) error (%d)", index, errno);
	}

	p_section_list_pool->read_lock_count[index] = 0;
	p_section_list_pool->write_lock_count[index] = 0;

	if (sem_init(&(p_section_list_pool->sem[index]), 1, 1) == -1)
	{
		log_error("sem_init(sem[%d]) error (%d)", index, errno);
	}

	if (index != BBS_max_section)
	{
		if (sem_destroy(&(p_section_list_pool->sem[BBS_max_section])) == -1)
		{
			log_error("sem_destroy(sem[%d]) error (%d)", BBS_max_section, errno);
		}

		p_section_list_pool->read_lock_count[BBS_max_section] = 0;
		p_section_list_pool->write_lock_count[BBS_max_section] = 0;

		if (sem_init(&(p_section_list_pool->sem[BBS_max_section]), 1, 1) == -1)
		{
			log_error("sem_init(sem[%d]) error (%d)", BBS_max_section, errno);
		}
	}

	return 0;
}
#endif
