/***************************************************************************
						 user_list.c  -  description
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

#include "common.h"
#include "database.h"
#include "log.h"
#include "trie_dict.h"
#include "user_list.h"
#include "user_stat.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/sem.h>
#include <sys/shm.h>

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

#define USER_LIST_TRY_LOCK_WAIT_TIME 1 // second
#define USER_LIST_TRY_LOCK_TIMES 10

struct user_list_pool_t
{
	int shmid;
	int semid;
	USER_LIST user_list[2];
	USER_LIST *p_current;
	USER_LIST *p_new;
	USER_ONLINE_LIST user_online_list[2];
	USER_ONLINE_LIST *p_online_current;
	USER_ONLINE_LIST *p_online_new;
	USER_STAT_MAP user_stat_map;
};
typedef struct user_list_pool_t USER_LIST_POOL;

static USER_LIST_POOL *p_user_list_pool = NULL;
static TRIE_NODE *p_trie_action_dict = NULL;

typedef struct user_action_map_t
{
	char name[BBS_current_action_max_len + 1];
	char title[BBS_current_action_max_len + 1];
} USER_ACTION_MAP;

const USER_ACTION_MAP user_action_map[] =
	{
		{"ARTICLE_FAVOR", "浏览收藏"},
		{"BBS_NET", "站点穿梭"},
		{"CHICKEN", "电子小鸡"},
		{"EDIT_ARTICLE", "修改文章"},
		{"LOGIN", "进入大厅"},
		{"MENU", "菜单选择"},
		{"POST_ARTICLE", "撰写文章"},
		{"REPLY_ARTICLE", "回复文章"},
		{"USER_LIST", "查花名册"},
		{"USER_ONLINE", "环顾四周"},
		{"VIEW_ARTICLE", "阅读文章"},
		{"VIEW_FILE", "查看文档"},
		{"WWW", "Web浏览"}};

const int user_action_map_size = sizeof(user_action_map) / sizeof(USER_ACTION_MAP);

static int user_list_try_rd_lock(int semid, int wait_sec);
static int user_list_try_rw_lock(int semid, int wait_sec);
static int user_list_rd_unlock(int semid);
static int user_list_rw_unlock(int semid);
static int user_list_rd_lock(int semid);
static int user_list_rw_lock(int semid);

static int user_list_load(MYSQL *db, USER_LIST *p_list);
static int user_online_list_load(MYSQL *db, USER_ONLINE_LIST *p_list);

static int user_info_index_uid_comp(const void *ptr1, const void *ptr2)
{
	const USER_INFO_INDEX_UID *p1 = ptr1;
	const USER_INFO_INDEX_UID *p2 = ptr2;

	if (p1->uid < p2->uid)
	{
		return -1;
	}
	else if (p1->uid > p2->uid)
	{
		return 1;
	}
	else if (p1->id < p2->id)
	{
		return -1;
	}
	else if (p1->id > p2->id)
	{
		return 1;
	}
	return 0;
}

int user_list_load(MYSQL *db, USER_LIST *p_list)
{
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int ret = 0;
	int i;
	int j;
	int32_t last_uid;
	size_t intro_buf_offset;
	size_t intro_len;

	if (db == NULL || p_list == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_list->user_count > 0)
	{
		last_uid = p_list->users[p_list->user_count - 1].uid;
	}
	else
	{
		last_uid = -1;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT user_list.UID AS UID, username, nickname, gender, gender_pub, life, exp, visit_count, "
			 "UNIX_TIMESTAMP(signup_dt), UNIX_TIMESTAMP(last_login_dt), UNIX_TIMESTAMP(last_logout_dt), "
			 "UNIX_TIMESTAMP(birthday), `introduction` "
			 "FROM user_list INNER JOIN user_pubinfo ON user_list.UID = user_pubinfo.UID "
			 "INNER JOIN user_reginfo ON user_list.UID = user_reginfo.UID "
			 "WHERE enable ORDER BY username");

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user info error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	if ((rs = mysql_use_result(db)) == NULL)
	{
		log_error("Get user info data failed\n");
		ret = -1;
		goto cleanup;
	}

	intro_buf_offset = 0;
	i = 0;
	while ((row = mysql_fetch_row(rs)))
	{
		// record
		p_list->users[i].id = i;
		p_list->users[i].uid = atoi(row[0]);
		strncpy(p_list->users[i].username, row[1], sizeof(p_list->users[i].username) - 1);
		p_list->users[i].username[sizeof(p_list->users[i].username) - 1] = '\0';
		strncpy(p_list->users[i].nickname, row[2], sizeof(p_list->users[i].nickname) - 1);
		p_list->users[i].nickname[sizeof(p_list->users[i].nickname) - 1] = '\0';
		p_list->users[i].gender = row[3][0];
		p_list->users[i].gender_pub = (int8_t)(row[4] == NULL ? 0 : atoi(row[4]));
		p_list->users[i].life = (row[5] == NULL ? 0 : atoi(row[5]));
		p_list->users[i].exp = (row[6] == NULL ? 0 : atoi(row[6]));
		p_list->users[i].visit_count = (row[7] == NULL ? 0 : atoi(row[7]));
		p_list->users[i].signup_dt = (row[8] == NULL ? 0 : atol(row[8]));
		p_list->users[i].last_login_dt = (row[9] == NULL ? 0 : atol(row[9]));
		p_list->users[i].last_logout_dt = (row[10] == NULL ? 0 : atol(row[10]));
		p_list->users[i].birthday = (row[10] == NULL ? 0 : atol(row[11]));
		intro_len = strlen((row[12] == NULL ? "" : row[12]));
		if (intro_len >= sizeof(p_list->user_intro_buf) - 1 - intro_buf_offset)
		{
			log_error("OOM for user introduction: len=%d, i=%d\n", intro_len, i);
			break;
		}
		memcpy(p_list->user_intro_buf + intro_buf_offset,
			   (row[12] == NULL ? "" : row[12]),
			   intro_len + 1);
		p_list->users[i].intro = p_list->user_intro_buf + intro_buf_offset;
		intro_buf_offset += (intro_len + 1);

		i++;
		if (i >= BBS_max_user_count)
		{
			log_error("Too many users, exceed limit %d\n", BBS_max_user_count);
			break;
		}
	}
	mysql_free_result(rs);
	rs = NULL;

	if (i != p_list->user_count || p_list->users[i - 1].uid != last_uid) // Count of users changed
	{
		// Rebuild index
		for (j = 0; j < i; j++)
		{
			p_list->index_uid[j].uid = p_list->users[j].uid;
			p_list->index_uid[j].id = j;
		}

		qsort(p_list->index_uid, (size_t)i, sizeof(USER_INFO_INDEX_UID), user_info_index_uid_comp);

#ifdef _DEBUG
		log_error("Rebuild index of %d users, last_uid=%d\n", i, p_list->users[i - 1].uid);
#endif
	}

	p_list->user_count = i;

#ifdef _DEBUG
	log_error("Loaded %d users\n", p_list->user_count);
#endif

cleanup:
	mysql_free_result(rs);

	return ret;
}

int user_online_list_load(MYSQL *db, USER_ONLINE_LIST *p_list)
{
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int ret = 0;
	int i;
	int j;

	if (db == NULL || p_list == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT SID, UID, ip, current_action, UNIX_TIMESTAMP(login_tm), "
			 "UNIX_TIMESTAMP(last_tm) FROM user_online "
			 "WHERE last_tm >= SUBDATE(NOW(), INTERVAL %d SECOND) AND UID <> 0 "
			 "ORDER BY last_tm DESC",
			 BBS_user_off_line);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query user online error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	if ((rs = mysql_use_result(db)) == NULL)
	{
		log_error("Get user online data failed\n");
		ret = -1;
		goto cleanup;
	}

	i = 0;
	while ((row = mysql_fetch_row(rs)))
	{
		p_list->users[i].id = i;
		strncpy(p_list->users[i].session_id, row[0], sizeof(p_list->users[i].session_id) - 1);
		p_list->users[i].session_id[sizeof(p_list->users[i].session_id) - 1] = '\0';

		if ((ret = query_user_info_by_uid(atoi(row[1]), &(p_list->users[i].user_info))) <= 0)
		{
			log_error("query_user_info_by_uid(%d) error\n", atoi(row[1]));
			continue;
		}

		strncpy(p_list->users[i].ip, row[2], sizeof(p_list->users[i].ip) - 1);
		p_list->users[i].ip[sizeof(p_list->users[i].ip) - 1] = '\0';

		strncpy(p_list->users[i].current_action, row[3], sizeof(p_list->users[i].current_action) - 1);
		p_list->users[i].current_action[sizeof(p_list->users[i].current_action) - 1] = '\0';
		p_list->users[i].current_action_title = NULL;
		if (p_list->users[i].current_action[0] == '\0')
		{
			p_list->users[i].current_action_title = "";
		}
		else if (trie_dict_get(p_trie_action_dict, p_list->users[i].current_action, (int64_t *)(&(p_list->users[i].current_action_title))) < 0)
		{
			log_error("trie_dict_get(p_trie_action_dict, %s) error on session_id=%s\n",
					  p_list->users[i].current_action, p_list->users[i].session_id);
			continue;
		}

		p_list->users[i].login_tm = (row[4] == NULL ? 0 : atol(row[4]));
		p_list->users[i].last_tm = (row[5] == NULL ? 0 : atol(row[5]));

		i++;
		if (i >= BBS_max_user_online_count)
		{
			log_error("Too many online users, exceed limit %d\n", BBS_max_user_online_count);
			break;
		}
	}
	mysql_free_result(rs);
	rs = NULL;

	if (i > 0)
	{
		// Rebuild index
		for (j = 0; j < i; j++)
		{
			p_list->index_uid[j].uid = p_list->users[j].user_info.uid;
			p_list->index_uid[j].id = j;
		}

		qsort(p_list->index_uid, (size_t)i, sizeof(USER_INFO_INDEX_UID), user_info_index_uid_comp);

#ifdef _DEBUG
		log_error("Rebuild index of %d online users\n", i);
#endif
	}

	p_list->user_count = i;

#ifdef _DEBUG
	log_error("Loaded %d online users\n", p_list->user_count);
#endif

cleanup:
	mysql_free_result(rs);

	return ret;
}

int user_list_pool_init(const char *filename)
{
	int shmid;
	int semid;
	int proj_id;
	key_t key;
	size_t size;
	void *p_shm;
	union semun arg;
	int i;

	if (p_user_list_pool != NULL || p_trie_action_dict != NULL)
	{
		log_error("p_user_list_pool already initialized\n");
		return -1;
	}

	p_trie_action_dict = trie_dict_create();
	if (p_trie_action_dict == NULL)
	{
		log_error("trie_dict_create() error\n");
		return -1;
	}

	for (i = 0; i < user_action_map_size; i++)
	{
		if (trie_dict_set(p_trie_action_dict, user_action_map[i].name, (int64_t)(user_action_map[i].title)) < 0)
		{
			log_error("trie_dict_set(p_trie_action_dict, %s) error\n", user_action_map[i].name);
		}
	}

	// Allocate shared memory
	proj_id = (int)(time(NULL) % getpid());
	key = ftok(filename, proj_id);
	if (key == -1)
	{
		log_error("ftok(%s %d) error (%d)\n", filename, proj_id, errno);
		return -2;
	}

	size = sizeof(USER_LIST_POOL);
	shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
	if (shmid == -1)
	{
		log_error("shmget(size = %d) error (%d)\n", size, errno);
		return -3;
	}
	p_shm = shmat(shmid, NULL, 0);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(shmid=%d) error (%d)\n", shmid, errno);
		return -3;
	}

	p_user_list_pool = p_shm;
	p_user_list_pool->shmid = shmid;

	// Allocate semaphore as user list pool lock
	size = 2; // r_sem and w_sem
	semid = semget(key, (int)size, IPC_CREAT | IPC_EXCL | 0600);
	if (semid == -1)
	{
		log_error("semget(user_list_pool_sem, size = %d) error (%d)\n", size, errno);
		return -3;
	}

	// Initialize sem value to 0
	arg.val = 0;
	for (i = 0; i < size; i++)
	{
		if (semctl(semid, i, SETVAL, arg) == -1)
		{
			log_error("semctl(user_list_pool_sem, SETVAL) error (%d)\n", errno);
			return -3;
		}
	}

	p_user_list_pool->semid = semid;

	// Set user counts to 0
	p_user_list_pool->user_list[0].user_count = 0;
	p_user_list_pool->user_list[1].user_count = 0;

	p_user_list_pool->p_current = &(p_user_list_pool->user_list[0]);
	p_user_list_pool->p_new = &(p_user_list_pool->user_list[1]);

	p_user_list_pool->p_online_current = &(p_user_list_pool->user_online_list[0]);
	p_user_list_pool->p_online_new = &(p_user_list_pool->user_online_list[1]);

	user_stat_map_init(&(p_user_list_pool->user_stat_map));

	return 0;
}

void user_list_pool_cleanup(void)
{
	int shmid;

	if (p_user_list_pool == NULL)
	{
		return;
	}

	shmid = p_user_list_pool->shmid;

	if (semctl(p_user_list_pool->semid, 0, IPC_RMID) == -1)
	{
		log_error("semctl(semid = %d, IPC_RMID) error (%d)\n", p_user_list_pool->semid, errno);
	}

	if (shmdt(p_user_list_pool) == -1)
	{
		log_error("shmdt(shmid = %d) error (%d)\n", shmid, errno);
	}

	if (shmctl(shmid, IPC_RMID, NULL) == -1)
	{
		log_error("shmctl(shmid = %d, IPC_RMID) error (%d)\n", shmid, errno);
	}

	p_user_list_pool = NULL;

	if (p_trie_action_dict != NULL)
	{
		trie_dict_destroy(p_trie_action_dict);

		p_trie_action_dict = NULL;
	}
}

int set_user_list_pool_shm_readonly(void)
{
	int shmid;
	void *p_shm;

	if (p_user_list_pool == NULL)
	{
		log_error("p_user_list_pool not initialized\n");
		return -1;
	}

	shmid = p_user_list_pool->shmid;

	// Remap shared memory in read-only mode
	p_shm = shmat(shmid, p_user_list_pool, SHM_RDONLY | SHM_REMAP);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(user_list_pool shmid = %d) error (%d)\n", shmid, errno);
		return -3;
	}

	p_user_list_pool = p_shm;

	return 0;
}

int detach_user_list_pool_shm(void)
{
	if (p_user_list_pool != NULL && shmdt(p_user_list_pool) == -1)
	{
		log_error("shmdt(user_list_pool) error (%d)\n", errno);
		return -1;
	}

	p_user_list_pool = NULL;

	return 0;
}

int user_list_pool_reload(int online_user)
{
	MYSQL *db = NULL;
	USER_LIST *p_tmp;
	USER_ONLINE_LIST *p_online_tmp;
	int ret = 0;

	if (p_user_list_pool == NULL)
	{
		log_error("p_user_list_pool not initialized\n");
		return -1;
	}

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		return -1;
	}

	if (online_user)
	{
		if (user_online_list_load(db, p_user_list_pool->p_online_new) < 0)
		{
			log_error("user_online_list_load() error\n");
			ret = -2;
			goto cleanup;
		}
	}
	else
	{
		if (user_list_load(db, p_user_list_pool->p_new) < 0)
		{
			log_error("user_list_load() error\n");
			ret = -2;
			goto cleanup;
		}
	}

	mysql_close(db);
	db = NULL;

	if (user_list_rw_lock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rw_lock() error\n");
		ret = -3;
		goto cleanup;
	}

	if (online_user)
	{
		// Swap p_online_current and p_online_new
		p_online_tmp = p_user_list_pool->p_online_current;
		p_user_list_pool->p_online_current = p_user_list_pool->p_online_new;
		p_user_list_pool->p_online_new = p_online_tmp;
	}
	else
	{
		// Swap p_current and p_new
		p_tmp = p_user_list_pool->p_current;
		p_user_list_pool->p_current = p_user_list_pool->p_new;
		p_user_list_pool->p_new = p_tmp;
	}

	if (user_list_rw_unlock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rw_unlock() error\n");
		ret = -3;
		goto cleanup;
	}

cleanup:
	mysql_close(db);

	return ret;
}

int user_list_try_rd_lock(int semid, int wait_sec)
{
	struct sembuf sops[2];
	struct timespec timeout;
	int ret;

	sops[0].sem_num = 1; // w_sem
	sops[0].sem_op = 0;	 // wait until unlocked
	sops[0].sem_flg = 0;

	sops[1].sem_num = 0;		// r_sem
	sops[1].sem_op = 1;			// lock
	sops[1].sem_flg = SEM_UNDO; // undo on terminate

	timeout.tv_sec = wait_sec;
	timeout.tv_nsec = 0;

	ret = semtimedop(semid, sops, 2, &timeout);
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semtimedop(lock read) error %d\n", errno);
	}

	return ret;
}

int user_list_try_rw_lock(int semid, int wait_sec)
{
	struct sembuf sops[3];
	struct timespec timeout;
	int ret;

	sops[0].sem_num = 1; // w_sem
	sops[0].sem_op = 0;	 // wait until unlocked
	sops[0].sem_flg = 0;

	sops[1].sem_num = 1;		// w_sem
	sops[1].sem_op = 1;			// lock
	sops[1].sem_flg = SEM_UNDO; // undo on terminate

	sops[2].sem_num = 0; // r_sem
	sops[2].sem_op = 0;	 // wait until unlocked
	sops[2].sem_flg = 0;

	timeout.tv_sec = wait_sec;
	timeout.tv_nsec = 0;

	ret = semtimedop(semid, sops, 3, &timeout);
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semtimedop(lock write) error %d\n", errno);
	}

	return ret;
}

int user_list_rd_unlock(int semid)
{
	struct sembuf sops[2];
	int ret;

	sops[0].sem_num = 0;					 // r_sem
	sops[0].sem_op = -1;					 // unlock
	sops[0].sem_flg = IPC_NOWAIT | SEM_UNDO; // no wait

	ret = semop(semid, sops, 1);
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semop(unlock read) error %d\n", errno);
	}

	return ret;
}

int user_list_rw_unlock(int semid)
{
	struct sembuf sops[1];
	int ret;

	sops[0].sem_num = 1;					 // w_sem
	sops[0].sem_op = -1;					 // unlock
	sops[0].sem_flg = IPC_NOWAIT | SEM_UNDO; // no wait

	ret = semop(semid, sops, 1);
	if (ret == -1 && errno != EAGAIN && errno != EINTR)
	{
		log_error("semop(unlock write) error %d\n", errno);
	}

	return ret;
}

int user_list_rd_lock(int semid)
{
	int timer = 0;
	int ret = -1;

	while (!SYS_server_exit)
	{
		ret = user_list_try_rd_lock(semid, USER_LIST_TRY_LOCK_WAIT_TIME);
		if (ret == 0) // success
		{
			break;
		}
		else if (errno == EAGAIN || errno == EINTR) // retry
		{
			timer++;
			if (timer % USER_LIST_TRY_LOCK_TIMES == 0)
			{
				log_error("user_list_try_rd_lock() tried %d times\n", timer);
			}
		}
		else // failed
		{
			log_error("user_list_try_rd_lock() failed\n");
			break;
		}
	}

	return ret;
}

int user_list_rw_lock(int semid)
{
	int timer = 0;
	int ret = -1;

	while (!SYS_server_exit)
	{
		ret = user_list_try_rw_lock(semid, USER_LIST_TRY_LOCK_WAIT_TIME);
		if (ret == 0) // success
		{
			break;
		}
		else if (errno == EAGAIN || errno == EINTR) // retry
		{
			timer++;
			if (timer % USER_LIST_TRY_LOCK_TIMES == 0)
			{
				log_error("user_list_try_rw_lock() tried %d times\n", timer);
			}
		}
		else // failed
		{
			log_error("user_list_try_rw_lock() failed\n");
			break;
		}
	}

	return ret;
}

int query_user_list(int page_id, USER_INFO *p_users, int *p_user_count, int *p_page_count)
{
	int ret = 0;

	if (p_users == NULL || p_user_count == NULL || p_page_count == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	*p_user_count = 0;
	*p_page_count = 0;

	// acquire lock of user list
	if (user_list_rd_lock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_lock() error\n");
		return -2;
	}

	if (p_user_list_pool->p_current->user_count == 0)
	{
		// empty list
		ret = 0;
		goto cleanup;
	}

	*p_page_count = p_user_list_pool->p_current->user_count / BBS_user_limit_per_page +
					(p_user_list_pool->p_current->user_count % BBS_user_limit_per_page == 0 ? 0 : 1);

	if (page_id < 0 || page_id >= *p_page_count)
	{
		log_error("Invalid page_id = %d, not in range [0, %d)\n", page_id, *p_page_count);
		ret = -3;
		goto cleanup;
	}

	*p_user_count = MIN(BBS_user_limit_per_page,
						p_user_list_pool->p_current->user_count -
							page_id * BBS_user_limit_per_page);

	memcpy(p_users,
		   p_user_list_pool->p_current->users + page_id * BBS_user_limit_per_page,
		   sizeof(USER_INFO) * (size_t)(*p_user_count));

cleanup:
	// release lock of user list
	if (user_list_rd_unlock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_unlock() error\n");
		ret = -1;
	}

	return ret;
}

int query_user_online_list(int page_id, USER_ONLINE_INFO *p_online_users, int *p_user_count, int *p_page_count)
{
	int ret = 0;

	if (p_online_users == NULL || p_user_count == NULL || p_page_count == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	*p_user_count = 0;
	*p_page_count = 0;

	// acquire lock of user list
	if (user_list_rd_lock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_lock() error\n");
		return -2;
	}

	if (p_user_list_pool->p_online_current->user_count == 0)
	{
		// empty list
		ret = 0;
		goto cleanup;
	}

	*p_page_count = p_user_list_pool->p_online_current->user_count / BBS_user_limit_per_page +
					(p_user_list_pool->p_online_current->user_count % BBS_user_limit_per_page == 0 ? 0 : 1);

	if (page_id < 0 || page_id >= *p_page_count)
	{
		log_error("Invalid page_id = %d, not in range [0, %d)\n", page_id, *p_page_count);
		ret = -3;
		goto cleanup;
	}

	*p_user_count = MIN(BBS_user_limit_per_page,
						p_user_list_pool->p_online_current->user_count -
							page_id * BBS_user_limit_per_page);

	memcpy(p_online_users,
		   p_user_list_pool->p_online_current->users + page_id * BBS_user_limit_per_page,
		   sizeof(USER_ONLINE_INFO) * (size_t)(*p_user_count));

cleanup:
	// release lock of user list
	if (user_list_rd_unlock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_unlock() error\n");
		ret = -1;
	}

	return ret;
}

int query_user_info(int32_t id, USER_INFO *p_user)
{
	int ret = 0;

	if (p_user == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	// acquire lock of user list
	if (user_list_rd_lock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_lock() error\n");
		return -2;
	}

	if (id >= 0 && id < p_user_list_pool->p_current->user_count) // Found
	{
		*p_user = p_user_list_pool->p_current->users[id];
		ret = 1;
	}

	// release lock of user list
	if (user_list_rd_unlock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_unlock() error\n");
		ret = -1;
	}

	return ret;
}

int query_user_info_by_uid(int32_t uid, USER_INFO *p_user)
{
	int left;
	int right;
	int mid;
	int32_t id;
	int ret = 0;

	if (p_user == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	// acquire lock of user list
	if (user_list_rd_lock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_lock() error\n");
		return -2;
	}

	left = 0;
	right = p_user_list_pool->p_current->user_count - 1;

	while (left < right)
	{
		mid = (left + right) / 2;
		if (uid < p_user_list_pool->p_current->index_uid[mid].uid)
		{
			right = mid - 1;
		}
		else if (uid > p_user_list_pool->p_current->index_uid[mid].uid)
		{
			left = mid + 1;
		}
		else // if (uid == p_user_list_pool->p_current->index_uid[mid].uid)
		{
			left = mid;
			break;
		}
	}

	if (uid == p_user_list_pool->p_current->index_uid[left].uid) // Found
	{
		id = p_user_list_pool->p_current->index_uid[left].id;
		*p_user = p_user_list_pool->p_current->users[id];
		ret = 1;
	}

	// release lock of user list
	if (user_list_rd_unlock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_unlock() error\n");
		ret = -1;
	}

	return ret;
}

int query_user_online_info(int32_t id, USER_ONLINE_INFO *p_user)
{
	int ret = 0;

	if (p_user == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	// acquire lock of user list
	if (user_list_rd_lock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_lock() error\n");
		return -2;
	}

	if (id >= 0 && id < p_user_list_pool->p_online_current->user_count) // Found
	{
		*p_user = p_user_list_pool->p_online_current->users[id];
		ret = 1;
	}

	// release lock of user list
	if (user_list_rd_unlock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_unlock() error\n");
		ret = -1;
	}

	return ret;
}

int query_user_online_info_by_uid(int32_t uid, USER_ONLINE_INFO *p_users, int *p_user_cnt, int start_id)
{
	int left;
	int right;
	int mid;
	int32_t id;
	int ret = 0;
	int i;
	int user_cnt;

	if (p_users == NULL || p_user_cnt == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	user_cnt = *p_user_cnt;
	*p_user_cnt = 0;

	// acquire lock of user list
	if (user_list_rd_lock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_lock() error\n");
		return -2;
	}

	left = start_id;
	right = p_user_list_pool->p_online_current->user_count - 1;

	while (left < right)
	{
		mid = (left + right) / 2;
		if (uid < p_user_list_pool->p_online_current->index_uid[mid].uid)
		{
			right = mid - 1;
		}
		else if (uid > p_user_list_pool->p_online_current->index_uid[mid].uid)
		{
			left = mid + 1;
		}
		else // if (uid == p_user_list_pool->p_online_current->index_uid[mid].uid)
		{
			left = mid;
			break;
		}
	}

	if (uid == p_user_list_pool->p_online_current->index_uid[left].uid)
	{
		right = left;
		left = start_id;

		while (left < right)
		{
			mid = (left + right) / 2;
			if (uid - 1 < p_user_list_pool->p_online_current->index_uid[mid].uid)
			{
				right = mid;
			}
			else // if (uid - 1 >= p_user_list_pool->p_online_current->index_uid[mid].uid)
			{
				left = mid + 1;
			}
		}

		for (i = 0;
			 left < p_user_list_pool->p_online_current->user_count && i < user_cnt &&
			 uid == p_user_list_pool->p_online_current->index_uid[left].uid;
			 left++, i++)
		{
			id = p_user_list_pool->p_online_current->index_uid[left].id;
			p_users[i] = p_user_list_pool->p_online_current->users[id];
		}

		if (i > 0)
		{
			*p_user_cnt = i;
			ret = 1;
		}
	}

	// release lock of user list
	if (user_list_rd_unlock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_unlock() error\n");
		ret = -1;
	}

	return ret;
}

int get_user_id_list(int32_t *p_uid_list, int *p_user_cnt, int start_uid)
{
	int left;
	int right;
	int mid;
	int ret = 0;
	int i;

	if (p_uid_list == NULL || p_user_cnt == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	// acquire lock of user list
	if (user_list_rd_lock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_lock() error\n");
		return -2;
	}

	left = 0;
	right = p_user_list_pool->p_current->user_count - 1;

	while (left < right)
	{
		mid = (left + right) / 2;
		if (start_uid < p_user_list_pool->p_current->index_uid[mid].uid)
		{
			right = mid - 1;
		}
		else if (start_uid > p_user_list_pool->p_current->index_uid[mid].uid)
		{
			left = mid + 1;
		}
		else // if (start_uid == p_user_list_pool->p_current->index_uid[mid].uid)
		{
			left = mid;
			break;
		}
	}

	for (i = 0; i < *p_user_cnt && left + i < p_user_list_pool->p_current->user_count; i++)
	{
		p_uid_list[i] = p_user_list_pool->p_current->index_uid[left + i].uid;
	}
	*p_user_cnt = i;

	// release lock of user list
	if (user_list_rd_unlock(p_user_list_pool->semid) < 0)
	{
		log_error("user_list_rd_unlock() error\n");
		ret = -1;
	}

	return ret;
}

int user_stat_update(void)
{
	return user_stat_map_update(&(p_user_list_pool->user_stat_map));
}

int user_article_cnt_inc(int32_t uid, int n)
{
	return user_stat_article_cnt_inc(&(p_user_list_pool->user_stat_map), uid, n);
}

int get_user_article_cnt(int32_t uid)
{
	const USER_STAT *p_stat;
	int ret;

	ret = user_stat_get(&(p_user_list_pool->user_stat_map), uid, &p_stat);
	if (ret < 0)
	{
		log_error("user_stat_get(uid=%d) error: %d\n", uid, ret);
		return -1;
	}
	else if (ret == 0) // user not found
	{
		return -1;
	}

	return p_stat->article_count;
}
