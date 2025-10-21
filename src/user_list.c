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
#include "user_list.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>

#ifdef _SEM_SEMUN_UNDEFINED
union semun
{
    int val;               /* Value for SETVAL */
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
};
typedef struct user_list_pool_t USER_LIST_POOL;

static USER_LIST_POOL *p_user_list_pool = NULL;

int user_list_load(MYSQL *db, USER_LIST *p_list)
{
    MYSQL_RES *rs = NULL;
    MYSQL_ROW row;
    char sql[SQL_BUFFER_LEN];
    int ret = 0;
    int i;

    if (db == NULL || p_list == NULL)
    {
        log_error("NULL pointer error\n");
        return -1;
    }

    snprintf(sql, sizeof(sql),
             "SELECT user_list.UID AS UID, username, nickname, gender, gender_pub, life, exp, "
             "UNIX_TIMESTAMP(signup_dt), UNIX_TIMESTAMP(last_login_dt), UNIX_TIMESTAMP(birthday) "
             "FROM user_list INNER JOIN user_pubinfo ON user_list.UID = user_pubinfo.UID "
             "INNER JOIN user_reginfo ON user_list.UID = user_reginfo.UID "
             "WHERE enable ORDER BY UID");

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

    i = 0;
    while ((row = mysql_fetch_row(rs)))
    {
        p_list->users[i].uid = atoi(row[0]);
        strncpy(p_list->users[i].username, row[1], sizeof(p_list->users[i].username) - 1);
        p_list->users[i].username[sizeof(p_list->users[i].username) - 1] = '\0';
        strncpy(p_list->users[i].nickname, row[2], sizeof(p_list->users[i].nickname) - 1);
        p_list->users[i].nickname[sizeof(p_list->users[i].nickname) - 1] = '\0';
        p_list->users[i].gender = row[3][0];
        p_list->users[i].gender_pub = (int8_t)(row[4] == NULL ? 0 : atoi(row[4]));
        p_list->users[i].life = (row[5] == NULL ? 0 : atoi(row[5]));
        p_list->users[i].exp = (row[6] == NULL ? 0 : atoi(row[6]));
        p_list->users[i].signup_dt = (row[7] == NULL ? 0 : atol(row[7]));
        p_list->users[i].last_login_dt = (row[8] == NULL ? 0 : atol(row[8]));
        p_list->users[i].birthday = (row[9] == NULL ? 0 : atol(row[9]));

        i++;
    }
    mysql_free_result(rs);
    rs = NULL;

    p_list->user_count = i;

#ifdef _DEBUG
    log_error("Loaded %d users\n", p_list->user_count);
#endif

cleanup:
    mysql_free_result(rs);

    return ret;
}

int user_list_pool_init(void)
{
    int shmid;
    int semid;
    int proj_id;
    key_t key;
    size_t size;
    void *p_shm;
    union semun arg;
    int i;

    if (p_user_list_pool != NULL)
    {
        log_error("p_user_list_pool already initialized\n");
        return -1;
    }

    // Allocate shared memory
    proj_id = (int)(time(NULL) % getpid());
    key = ftok(VAR_USER_LIST_SHM, proj_id);
    if (key == -1)
    {
        log_error("ftok(%s %d) error (%d)\n", VAR_USER_LIST_SHM, proj_id, errno);
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

int user_list_pool_reload(void)
{
    MYSQL *db = NULL;
    USER_LIST *p_tmp;
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

    if (user_list_rw_lock() < 0)
    {
        log_error("user_list_rw_lock() error\n");
        return -2;
    }

    if (user_list_load(db, p_user_list_pool->p_new) < 0)
    {
        log_error("user_list_rw_lock() error\n");
        ret = -3;
        goto cleanup;
    }

    // Swap p_current and p_new
    p_tmp = p_user_list_pool->p_current;
    p_user_list_pool->p_current = p_user_list_pool->p_new;
    p_user_list_pool->p_new = p_tmp;

cleanup:
    if (user_list_rw_unlock() < 0)
    {
        log_error("user_list_rw_unlock() error\n");
        ret = -2;
    }

    mysql_close(db);

    return ret;
}

int user_list_try_rd_lock(int wait_sec)
{
    struct sembuf sops[2];
    struct timespec timeout;
    int ret;

    sops[0].sem_num = 1; // w_sem
    sops[0].sem_op = 0;  // wait until unlocked
    sops[0].sem_flg = 0;

    sops[1].sem_num = 0;        // r_sem
    sops[1].sem_op = 1;         // lock
    sops[1].sem_flg = SEM_UNDO; // undo on terminate

    timeout.tv_sec = wait_sec;
    timeout.tv_nsec = 0;

    ret = semtimedop(p_user_list_pool->semid, sops, 2, &timeout);
    if (ret == -1 && errno != EAGAIN && errno != EINTR)
    {
        log_error("semtimedop(lock read) error %d\n", errno);
    }

    return ret;
}

int user_list_try_rw_lock(int wait_sec)
{
    struct sembuf sops[3];
    struct timespec timeout;
    int ret;

    sops[0].sem_num = 1; // w_sem
    sops[0].sem_op = 0;  // wait until unlocked
    sops[0].sem_flg = 0;

    sops[1].sem_num = 1;        // w_sem
    sops[1].sem_op = 1;         // lock
    sops[1].sem_flg = SEM_UNDO; // undo on terminate

    sops[2].sem_num = 0; // r_sem
    sops[2].sem_op = 0;  // wait until unlocked
    sops[2].sem_flg = 0;

    timeout.tv_sec = wait_sec;
    timeout.tv_nsec = 0;

    ret = semtimedop(p_user_list_pool->semid, sops, 3, &timeout);
    if (ret == -1 && errno != EAGAIN && errno != EINTR)
    {
        log_error("semtimedop(lock write) error %d\n", errno);
    }

    return ret;
}

int user_list_rd_unlock(void)
{
    struct sembuf sops[2];
    int ret;

    sops[0].sem_num = 0;                     // r_sem
    sops[0].sem_op = -1;                     // unlock
    sops[0].sem_flg = IPC_NOWAIT | SEM_UNDO; // no wait

    ret = semop(p_user_list_pool->semid, sops, 1);
    if (ret == -1 && errno != EAGAIN && errno != EINTR)
    {
        log_error("semop(unlock read) error %d\n", errno);
    }

    return ret;
}

int user_list_rw_unlock(void)
{
    struct sembuf sops[1];
    int ret;

    sops[0].sem_num = 1;                     // w_sem
    sops[0].sem_op = -1;                     // unlock
    sops[0].sem_flg = IPC_NOWAIT | SEM_UNDO; // no wait

    ret = semop(p_user_list_pool->semid, sops, 1);
    if (ret == -1 && errno != EAGAIN && errno != EINTR)
    {
        log_error("semop(unlock write) error %d\n", errno);
    }

    return ret;
}

int user_list_rd_lock(void)
{
    int timer = 0;
    int ret = -1;

    while (!SYS_server_exit)
    {
        ret = user_list_try_rd_lock(USER_LIST_TRY_LOCK_WAIT_TIME);
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

int user_list_rw_lock(void)
{
    int timer = 0;
    int ret = -1;

    while (!SYS_server_exit)
    {
        ret = user_list_try_rw_lock(USER_LIST_TRY_LOCK_WAIT_TIME);
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
