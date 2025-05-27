/***************************************************************************
					section_list_loader.c  -  description
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

#include "section_list_loader.h"
#include "log.h"
#include "database.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#define SECTION_LIST_LOAD_INTERVAL 10 // second

static int section_list_loader_pid;

int load_section_config_from_db(void)
{
	MYSQL *db;
	MYSQL_RES *rs, *rs2;
	MYSQL_ROW row, row2;
	char sql[SQL_BUFFER_LEN];
	int32_t sid;
	char master_name[BBS_username_max_len + 1];
	SECTION_LIST *p_section;
	int ret;

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		return -2;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT section_config.SID, sname, section_config.title, section_config.CID, "
			 "read_user_level, write_user_level, section_config.enable * section_class.enable AS enable "
			 "FROM section_config INNER JOIN section_class ON section_config.CID = section_class.CID "
			 "ORDER BY section_config.SID");

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query section_list error: %s\n", mysql_error(db));
		return -3;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get section_list data failed\n");
		return -3;
	}

	ret = 0;
	while ((row = mysql_fetch_row(rs)))
	{
		sid = atoi(row[0]);

		// Query section master
		snprintf(sql, sizeof(sql),
				 "SELECT username FROM section_master "
				 "INNER JOIN user_list ON section_master.UID = user_list.UID "
				 "WHERE SID = %d AND section_master.enable AND (NOW() BETWEEN begin_dt AND end_dt) "
				 "ORDER BY major DESC LIMIT 1",
				 sid);

		if (mysql_query(db, sql) != 0)
		{
			log_error("Query section_master error: %s\n", mysql_error(db));
			ret = -3;
			break;
		}
		if ((rs2 = mysql_store_result(db)) == NULL)
		{
			log_error("Get section_master data failed\n");
			ret = -3;
			break;
		}
		if ((row2 = mysql_fetch_row(rs2)))
		{
			strncpy(master_name, row2[0], sizeof(master_name) - 1);
			master_name[sizeof(master_name) - 1] = '\0';
		}
		else
		{
			master_name[0] = '\0';
		}
		mysql_free_result(rs2);

		p_section = section_list_find_by_sid(sid);

		if (p_section == NULL)
		{
			p_section = section_list_create(sid, row[1], row[2], "");
			if (p_section == NULL)
			{
				log_error("section_list_create() error: load new section sid = %d sname = %s\n", sid, row[1]);
				ret = -4;
				break;
			}

			// acquire rw lock
			ret = section_list_rw_lock(p_section);
			if (ret < 0)
			{
				break;
			}
		}
		else
		{
			// acquire rw lock
			ret = section_list_rw_lock(p_section);
			if (ret < 0)
			{
				break;
			}

			strncpy(p_section->sname, row[1], sizeof(p_section->sname) - 1);
			p_section->sname[sizeof(p_section->sname) - 1] = '\0';
			strncpy(p_section->stitle, row[1], sizeof(p_section->stitle) - 1);
			p_section->stitle[sizeof(p_section->stitle) - 1] = '\0';
			strncpy(p_section->master_name, master_name, sizeof(p_section->master_name) - 1);
			p_section->master_name[sizeof(p_section->master_name) - 1] = '\0';
		}

		p_section->class_id = atoi(row[3]);
		p_section->read_user_level = atoi(row[4]);
		p_section->write_user_level = atoi(row[5]);
		p_section->enable = (int8_t)atoi(row[6]);

		// release rw lock
		ret = section_list_rw_unlock(p_section);
		if (ret < 0)
		{
			break;
		}
	}
	mysql_free_result(rs);

	mysql_close(db);

	return ret;
}

int append_articles_from_db(int32_t start_aid, int global_lock)
{
	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	ARTICLE article;
	SECTION_LIST *p_section = NULL;
	int32_t last_sid = 0;
	int ret = 0;
	int i;

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		return -2;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT AID, TID, SID, CID, UID, visible, excerption, "
			 "ontop, `lock`, username, nickname, title, UNIX_TIMESTAMP(sub_dt) AS sub_dt "
			 "FROM bbs WHERE AID >= %d ORDER BY AID",
			 start_aid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article list error: %s\n", mysql_error(db));
		return -3;
	}
	if ((rs = mysql_use_result(db)) == NULL)
	{
		log_error("Get article list data failed\n");
		return -3;
	}

	// acquire global lock
	if (global_lock)
	{
		if ((ret = section_list_rw_lock(NULL)) < 0)
		{
			log_error("section_list_rw_lock(sid = 0) error\n");
			goto cleanup;
		}
	}

	while ((row = mysql_fetch_row(rs)))
	{
		bzero(&article, sizeof(ARTICLE));

		// copy data of article
		i = 0;

		article.aid = atoi(row[i++]);
		article.tid = atoi(row[i++]);
		article.sid = atoi(row[i++]);
		article.cid = atoi(row[i++]);
		article.uid = atoi(row[i++]);
		article.visible = (int8_t)atoi(row[i++]);
		article.excerption = (int8_t)atoi(row[i++]);
		article.ontop = (int8_t)atoi(row[i++]);
		article.lock = (int8_t)atoi(row[i++]);

		strncpy(article.username, row[i++], sizeof(article.username) - 1);
		article.username[sizeof(article.username) - 1] = '\0';
		strncpy(article.nickname, row[i++], sizeof(article.nickname) - 1);
		article.nickname[sizeof(article.nickname) - 1] = '\0';
		strncpy(article.title, row[i++], sizeof(article.title) - 1);
		article.title[sizeof(article.title) - 1] = '\0';

		article.sub_dt = atol(row[i++]);

		// release lock of last section if different from current one
		if (!global_lock && article.sid != last_sid && last_sid != 0)
		{
			if ((ret = section_list_rw_unlock(p_section)) < 0)
			{
				log_error("section_list_rw_unlock(sid = %d) error\n", p_section->sid);
				break;
			}
		}

		if ((p_section = section_list_find_by_sid(article.sid)) == NULL)
		{
			log_error("section_list_find_by_sid(%d) error: unknown section, try reloading section config\n", article.sid);
			ret = ERR_UNKNOWN_SECTION; // Unknown section found
			break;
		}

		// acquire lock of current section if different from last one
		if (!global_lock && article.sid != last_sid)
		{
			if ((ret = section_list_rw_lock(p_section)) < 0)
			{
				log_error("section_list_rw_lock(sid = 0) error\n");
				break;
			}
		}

		// append article to section list
		last_sid = article.sid;

		if (section_list_append_article(p_section, &article) < 0)
		{
			log_error("section_list_append_article(sid = %d, aid = %d) error\n",
					  p_section->sid, article.aid);
			ret = -3;
			break;
		}
	}

	// release lock of last section
	if (!global_lock && last_sid != 0)
	{
		if ((ret = section_list_rw_unlock(p_section)) < 0)
		{
			log_error("section_list_rw_unlock(sid = %d) error\n", p_section->sid);
		}
	}

	// release global lock
	if (global_lock)
	{
		if ((ret = section_list_rw_unlock(NULL)) < 0)
		{
			log_error("section_list_rw_unlock(sid = 0) error\n");
		}
	}

cleanup:
	mysql_free_result(rs);

	mysql_close(db);

	return ret;
}

int section_list_loader_launch(void)
{
	int pid;
	int ret;
	int32_t last_aid;
	int article_count;
	int load_count;
	int i;

	if (section_list_loader_pid != 0)
	{
		log_error("section_list_loader already running, pid = %d\n", section_list_loader_pid);
		return -2;
	}

	pid = fork();

	if (pid > 0) // Parent process
	{
		SYS_child_process_count++;
		section_list_loader_pid = pid;
		log_std("Section list loader process (%d) start\n", pid);
		return 0;
	}
	else if (pid < 0) // Error
	{
		log_error("fork() error (%d)\n", errno);
		return -1;
	}

	// Child process
	SYS_child_process_count = 0;

	// Detach menu in shared memory
	detach_menu_shm(p_bbs_menu);
	free(p_bbs_menu);
	p_bbs_menu = NULL;

	// Do section data loader periodically
	while (!SYS_server_exit)
	{
		if (SYS_section_list_reload)
		{
			SYS_section_list_reload = 0;

			// Load section config
			if (load_section_config_from_db() < 0)
			{
				log_error("load_section_config_from_db() error\n");
			}
			else
			{
				log_error("Reload section config successfully\n");
			}
		}

		// Load section articles
		last_aid = article_block_last_aid();
		article_count = article_block_article_count();

		if ((ret = append_articles_from_db(last_aid + 1, 0)) < 0)
		{
			log_error("append_articles_from_db(%d, 0) error\n", last_aid + 1);

			if (ret == ERR_UNKNOWN_SECTION)
			{
				SYS_section_list_reload = 1; // Force reload section_list
			}
		}
		else
		{
			load_count = article_block_article_count() - article_count;

			if (load_count > 0)
			{
				log_std("Incrementally load %d articles, last_aid = %d\n", load_count, article_block_last_aid());
			}

			for (i = 0; i < SECTION_LIST_LOAD_INTERVAL && !SYS_server_exit && !SYS_section_list_reload; i++)
			{
				sleep(1);
			}
		}
	}

	// Child process exit

	// Detach data pools shm
	detach_section_list_shm();
	detach_article_block_shm();
	detach_trie_dict_shm();

	log_std("Section list loader process exit normally\n");
	log_end();

	section_list_loader_pid = 0;

	_exit(0);

	return 0;
}

int section_list_loader_reload(void)
{
	if (section_list_loader_pid == 0)
	{
		log_error("section_list_loader not running\n");
		return -2;
	}

	if (kill(section_list_loader_pid, SIGHUP) < 0)
	{
		log_error("Send SIGTERM signal failed (%d)\n", errno);
		return -1;
	}

	return 0;
}
