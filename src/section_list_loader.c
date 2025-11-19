/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * section_list_loader
 *   - load and query operations of section articles
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "article_cache.h"
#include "article_view_log.h"
#include "bbs.h"
#include "database.h"
#include "ip_mask.h"
#include "log.h"
#include "menu.h"
#include "section_list_loader.h"
#include "user_list.h"
#include "user_priv.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int section_list_loader_pid;
int last_article_op_log_mid;

static void loader_proc_sig_usr1_handler(int i)
{
	// Restart log
	if (log_restart() < 0)
	{
		log_error("Restart logging failed\n");
	}
}

int load_section_config_from_db(int update_gen_ex)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL, *rs2 = NULL;
	MYSQL_ROW row, row2;
	char sql[SQL_BUFFER_LEN];
	int32_t sid;
	char master_list[(BBS_username_max_len + 1) * 3 + 1];
	SECTION_LIST *p_section;
	char ex_menu_conf[FILE_PATH_LEN];
	int ret = 0;

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT section_config.SID, sname, section_config.title, section_config.CID, "
			 "read_user_level, write_user_level, section_config.enable * section_class.enable AS enable, "
			 "UNIX_TIMESTAMP(ex_menu_tm) AS ex_menu_tm "
			 "FROM section_config INNER JOIN section_class ON section_config.CID = section_class.CID "
			 "ORDER BY section_config.SID");

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query section_list error: %s\n", mysql_error(db));
		ret = -2;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get section_list data failed\n");
		ret = -2;
		goto cleanup;
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
				 "ORDER BY major DESC, begin_dt ASC LIMIT 3",
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

		master_list[0] = '\0';
		while ((row2 = mysql_fetch_row(rs2)))
		{
			strncat(master_list, row2[0], sizeof(master_list) - 1 - strnlen(master_list, sizeof(master_list)));
			strncat(master_list, " ", sizeof(master_list) - 1 - strnlen(master_list, sizeof(master_list)));
		}
		mysql_free_result(rs2);
		rs2 = NULL;

		p_section = section_list_find_by_sid(sid);

		if (p_section == NULL)
		{
			p_section = section_list_create(sid, row[1], row[2], master_list);
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

			if (section_list_update(p_section, row[1], row[2], master_list) < 0)
			{
				log_error("section_list_update(sid=%d) error\n", p_section->sid);
				ret = -4;
				break;
			}
		}

		p_section->class_id = atoi(row[3]);
		p_section->read_user_level = atoi(row[4]);
		p_section->write_user_level = atoi(row[5]);
		p_section->enable = (int8_t)atoi(row[6]);

		// Update gen_ex menu set
		if (update_gen_ex && p_section->enable && atol(row[7] == NULL ? "0" : row[7]) > p_section->ex_menu_tm)
		{
			snprintf(ex_menu_conf, sizeof(ex_menu_conf), "%s/%d", VAR_GEN_EX_MENU_DIR, p_section->sid);

			if (detach_menu_shm(&(p_section->ex_menu_set)) < 0)
			{
				log_error("detach_menu_shm(%s) error\n", ex_menu_conf);
			}
			if (load_menu(&(p_section->ex_menu_set), ex_menu_conf) < 0)
			{
				log_error("load_menu(%s) error\n", ex_menu_conf);
				unload_menu(&(p_section->ex_menu_set));
			}
			else
			{
				p_section->ex_menu_set.allow_exit = 1; // Allow exit menu
				p_section->ex_menu_tm = atol(row[7]);
#ifdef _DEBUG
				log_common("Loaded gen_ex_menu of section %d [%s]\n", p_section->sid, p_section->sname);
#endif
			}
		}

		// release rw lock
		ret = section_list_rw_unlock(p_section);
		if (ret < 0)
		{
			break;
		}
	}

cleanup:
	mysql_free_result(rs2);
	mysql_free_result(rs);
	mysql_close(db);

	return ret;
}

int append_articles_from_db(int32_t start_aid, int global_lock, int article_count_limit)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	ARTICLE article;
	ARTICLE *p_topic;
	SECTION_LIST *p_section = NULL;
	int32_t last_sid = 0;
	char sub_ip[IP_ADDR_LEN];
	int article_count = 0;
	int ret = 0;
	int i;

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT bbs.AID, TID, SID, bbs.CID, UID, visible, excerption, ontop, `lock`, "
			 "transship, username, nickname, title, UNIX_TIMESTAMP(sub_dt) AS sub_dt, "
			 "sub_ip, bbs_content.content "
			 "FROM bbs INNER JOIN bbs_content ON bbs.CID = bbs_content.CID "
			 "WHERE bbs.AID >= %d ORDER BY bbs.AID LIMIT %d",
			 start_aid, article_count_limit);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article list error: %s\n", mysql_error(db));
		ret = -2;
		goto cleanup;
	}
	if ((rs = mysql_use_result(db)) == NULL)
	{
		log_error("Get article list data failed\n");
		ret = -2;
		goto cleanup;
	}

	// acquire global lock
	if (global_lock && (ret = section_list_rw_lock(NULL)) < 0)
	{
		log_error("section_list_rw_lock(sid = 0) error\n");
		goto cleanup;
	}

	while ((row = mysql_fetch_row(rs)))
	{
		memset(&article, 0, sizeof(ARTICLE));

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
		article.transship = (int8_t)atoi(row[i++]);

		strncpy(article.username, row[i++], sizeof(article.username) - 1);
		article.username[sizeof(article.username) - 1] = '\0';
		strncpy(article.nickname, row[i++], sizeof(article.nickname) - 1);
		article.nickname[sizeof(article.nickname) - 1] = '\0';
		strncpy(article.title, row[i++], sizeof(article.title) - 1);
		article.title[sizeof(article.title) - 1] = '\0';

		article.sub_dt = atol(row[i++]);

		strncpy(sub_ip, row[i++], sizeof(sub_ip) - 1);
		sub_ip[sizeof(sub_ip) - 1] = '\0';
		ip_mask(sub_ip, 1, '*');

		// release lock of last section if different from current one
		if (!global_lock && article.sid != last_sid && last_sid != 0)
		{
			if ((ret = section_list_rw_unlock(p_section)) < 0)
			{
				log_error("section_list_rw_unlock(sid=%d) error\n", p_section->sid);
				break;
			}
		}

		if ((p_section = section_list_find_by_sid(article.sid)) == NULL)
		{
			log_error("section_list_find_by_sid(%d) error: unknown section, try reloading section config\n", article.sid);
			ret = ERR_UNKNOWN_SECTION; // Unknown section found
			break;
		}

		if (article.visible != 0 && article.tid != 0)
		{
			// Check if topic article is visible
			p_topic = article_block_find_by_aid(article.tid);
			if (p_topic == NULL || p_topic->visible == 0)
			{
				// log_error("Set article (aid = %d) as invisible due to invisible or non-existing topic head\n", article.aid);
				article.tid = 0;
				article.visible = 0;
			}
		}

		// acquire lock of current section if different from last one
		if (!global_lock && article.sid != last_sid)
		{
			if ((ret = section_list_rw_lock(p_section)) < 0)
			{
				log_error("section_list_rw_lock(sid=0) error\n");
				break;
			}
		}

		// append article to section list
		last_sid = article.sid;

		if (section_list_append_article(p_section, &article) < 0)
		{
			log_error("section_list_append_article(sid=%d, aid=%d) error\n",
					  p_section->sid, article.aid);
			ret = -3;
			break;
		}

		article_count++;

		if (article_cache_generate(VAR_ARTICLE_CACHE_DIR, &article, p_section, row[i++], sub_ip, 0) < 0)
		{
			log_error("article_cache_generate(aid=%d, cid=%d) error\n", article.aid, article.cid);
			ret = -4;
			break;
		}
	}

	// release lock of last section
	if (!global_lock && last_sid != 0)
	{
		if ((ret = section_list_rw_unlock(p_section)) < 0)
		{
			log_error("section_list_rw_unlock(sid=%d) error\n", p_section->sid);
		}
	}

	// release global lock
	if (global_lock && (ret = section_list_rw_unlock(NULL)) < 0)
	{
		log_error("section_list_rw_unlock(sid=0) error\n");
	}

cleanup:
	mysql_free_result(rs);
	mysql_close(db);

	article_cache_cleanup();

	return (ret < 0 ? ret : article_count);
}

int set_last_article_op_log_from_db(void)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int ret = 0;

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT MID FROM bbs_article_op ORDER BY MID DESC LIMIT 1");

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article op error: %s\n", mysql_error(db));
		ret = -2;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get article op data failed\n");
		ret = -2;
		goto cleanup;
	}

	if ((row = mysql_fetch_row(rs)))
	{
		last_article_op_log_mid = atoi(row[0]);
	}

cleanup:
	mysql_free_result(rs);
	mysql_close(db);

	return (ret < 0 ? ret : last_article_op_log_mid);
}

int apply_article_op_log_from_db(int op_count_limit)
{
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL, *rs2 = NULL;
	MYSQL_ROW row, row2;
	char sql[SQL_BUFFER_LEN];
	ARTICLE *p_article;
	SECTION_LIST *p_section = NULL;
	SECTION_LIST *p_section_dest;
	char sub_ip[IP_ADDR_LEN];
	int32_t last_sid = 0;
	int32_t sid_dest;
	int op_count = 0;
	int ret = 0;

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT MID, AID, type FROM bbs_article_op "
			 "WHERE MID > %d AND type NOT IN ('A') "
			 "ORDER BY MID LIMIT %d",
			 last_article_op_log_mid, op_count_limit);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article log error: %s\n", mysql_error(db));
		ret = -2;
		goto cleanup;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get article log data failed\n");
		ret = -2;
		goto cleanup;
	}

	while ((row = mysql_fetch_row(rs)))
	{
		p_article = article_block_find_by_aid(atoi(row[1]));
		if (p_article == NULL) // related article has not been appended yet
		{
			ret = -2;
			break;
		}

		// release lock of last section if different from current one
		if (p_article->sid != last_sid && last_sid != 0)
		{
			if ((ret = section_list_rw_unlock(p_section)) < 0)
			{
				log_error("section_list_rw_unlock(sid = %d) error\n", p_section->sid);
				break;
			}
		}

		if ((p_section = section_list_find_by_sid(p_article->sid)) == NULL)
		{
			log_error("section_list_find_by_sid(%d) error: unknown section, try reloading section config\n", p_article->sid);
			ret = ERR_UNKNOWN_SECTION; // Unknown section found
			break;
		}

		// acquire lock of current section if different from last one
		if (p_article->sid != last_sid)
		{
			if ((ret = section_list_rw_lock(p_section)) < 0)
			{
				log_error("section_list_rw_lock(sid = 0) error\n");
				break;
			}
		}

		last_sid = p_article->sid;

		switch (row[2][0])
		{
		case 'A': // Add article
			log_error("Operation type=A should not be found\n");
			break;
		case 'D': // Delete article
		case 'X': // Delete article by Admin
			if (section_list_set_article_visible(p_section, atoi(row[1]), 0) < 0)
			{
				log_error("section_list_set_article_visible(sid=%d, aid=%d, visible=0) error\n", p_section->sid, atoi(row[1]));
			}
			if (section_list_calculate_page(p_section, atoi(row[1])) < 0)
			{
				log_error("section_list_calculate_page(aid=%d) error\n", atoi(row[1]));
			}
			break;
		case 'S': // Restore article
			if (section_list_set_article_visible(p_section, atoi(row[1]), 1) < 0)
			{
				log_error("section_list_set_article_visible(sid=%d, aid=%d, visible=1) error\n", p_section->sid, atoi(row[1]));
			}
			if (section_list_calculate_page(p_section, atoi(row[1])) < 0)
			{
				log_error("section_list_calculate_page(aid=%d) error\n", atoi(row[1]));
			}
			break;
		case 'L': // Lock article
			p_article->lock = 1;
			break;
		case 'U': // Unlock article
			p_article->lock = 0;
			break;
		case 'M': // Modify article
			snprintf(sql, sizeof(sql),
					 "SELECT bbs.CID, sub_ip, bbs_content.content "
					 "FROM bbs INNER JOIN bbs_content ON bbs.CID = bbs_content.CID "
					 "WHERE bbs.AID = %d",
					 p_article->aid);

			if (mysql_query(db, sql) != 0)
			{
				log_error("Query article error: %s\n", mysql_error(db));
				ret = -3;
				break;
			}
			if ((rs2 = mysql_use_result(db)) == NULL)
			{
				log_error("Get article data failed\n");
				ret = -3;
				break;
			}
			if ((row2 = mysql_fetch_row(rs2)))
			{
				p_article->cid = atoi(row2[0]);

				strncpy(sub_ip, row2[1], sizeof(sub_ip) - 1);
				sub_ip[sizeof(sub_ip) - 1] = '\0';
				ip_mask(sub_ip, 1, '*');

				if (article_cache_generate(VAR_ARTICLE_CACHE_DIR, p_article, p_section, row2[2], sub_ip, 0) < 0)
				{
					log_error("article_cache_generate(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
					ret = -4;
				}
			}
			else
			{
				p_article->cid = 0;
				ret = -4;
			}
			mysql_free_result(rs2);
			rs2 = NULL;

			p_article->excerption = 0; // Set excerption = 0 implicitly in case of rare condition
			break;
		case 'T': // Move article
			snprintf(sql, sizeof(sql),
					 "SELECT SID FROM bbs WHERE AID = %d",
					 p_article->aid);

			if (mysql_query(db, sql) != 0)
			{
				log_error("Query article error: %s\n", mysql_error(db));
				ret = -3;
				break;
			}
			if ((rs2 = mysql_store_result(db)) == NULL)
			{
				log_error("Get article data failed\n");
				ret = -3;
				break;
			}
			if ((row2 = mysql_fetch_row(rs2)))
			{
				sid_dest = atoi(row2[0]);
			}
			else
			{
				sid_dest = 0;
				ret = -4;
			}
			mysql_free_result(rs2);
			rs2 = NULL;

			if (sid_dest > 0 && sid_dest != p_article->sid)
			{
				p_section_dest = section_list_find_by_sid(sid_dest);
				if (p_section_dest == NULL)
				{
					ret = ERR_UNKNOWN_SECTION;
					break;
				}
				// acquire lock of dest section
				if ((ret = section_list_rw_lock(p_section_dest)) < 0)
				{
					log_error("section_list_rw_lock(sid = %d) error\n", p_section_dest);
					break;
				}
				// Move topic
				if ((ret = section_list_move_topic(p_section, p_section_dest, p_article->aid)) < 0)
				{
					log_error("section_list_move_topic(src_sid=%d, dest_sid=%d, aid=%d) error (%d), retry in the next loop\n",
							  p_section->sid, p_section_dest->sid, p_article->aid, ret);
				}
				// release lock of dest section
				if (section_list_rw_unlock(p_section_dest) < 0)
				{
					log_error("section_list_rw_unlock(sid = %d) error\n", p_section_dest);
					ret = -1;
				}
			}
			break;
		case 'E': // Set article as excerption
			p_article->excerption = 1;
			break;
		case 'O': // Unset article as excerption
			p_article->excerption = 0;
			break;
		case 'F': // Set article on top
		case 'V': // Unset article on top
			p_article->ontop = (row[2][0] == 'F' ? 1 : 0);
			if (section_list_update_article_ontop(p_section, p_article) < 0)
			{
				log_error("section_list_update_article_ontop(sid=%d, aid=%d) error\n",
						  p_section->sid, p_article->aid);
			}
			break;
		case 'Z': // Set article as trnasship
			p_article->transship = 1;
			break;
		default:
			// log_error("Operation type=%s unknown, mid=%s\n", row[2], row[0]);
			break;
		}

		if (ret < 0)
		{
			break;
		}

		// Update MID with last successfully proceeded article_op_log
		last_article_op_log_mid = atoi(row[0]);

		op_count++;
	}

	// release lock of last section
	if (last_sid != 0)
	{
		if ((ret = section_list_rw_unlock(p_section)) < 0)
		{
			log_error("section_list_rw_unlock(sid = %d) error\n", p_section->sid);
		}
	}

cleanup:
	mysql_free_result(rs2);
	mysql_free_result(rs);
	mysql_close(db);

	return (ret < 0 ? ret : op_count);
}

static void section_list_ex_menu_set_cleanup(void)
{
	int i;

	for (i = 0; i < p_section_list_pool->section_count; i++)
	{
		if (p_section_list_pool->sections[i].ex_menu_tm > 0)
		{
			unload_menu(&(p_section_list_pool->sections[i].ex_menu_set));
		}
	}
}

int section_list_loader_launch(void)
{
	struct sigaction act = {0};
	int pid;
	int ret;
	int32_t last_aid;
	int article_count;
	int load_count;
	int last_mid;
	time_t tm_section_list_reload = 0;
	time_t tm_user_list_reload = time(NULL);
	time_t tm_user_online_list_reload = time(NULL);

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
		log_common("Section list loader process (pid = %d) start\n", pid);
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
	detach_menu_shm(&bbs_menu);
	detach_menu_shm(&top10_menu);

	// Set signal handler
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, NULL) == -1)
	{
		log_error("set signal action of SIGHUP error: %d\n", errno);
	}
	act.sa_handler = SIG_DFL;
	if (sigaction(SIGCHLD, &act, NULL) == -1)
	{
		log_error("set signal action of SIGCHLD error: %d\n", errno);
	}
	act.sa_handler = loader_proc_sig_usr1_handler;
	if (sigaction(SIGUSR1, &act, NULL) == -1)
	{
		log_error("set signal action of SIGUSR1 error: %d\n", errno);
	}

	// Do section data loader periodically
	while (!SYS_server_exit)
	{
		tm_section_list_reload = time(NULL);

		if (SYS_conf_reload)
		{
			SYS_conf_reload = 0;

			// Load section config
			if (load_section_config_from_db(0) < 0)
			{
				log_error("load_section_config_from_db(0) error\n");
			}
			else
			{
				log_common("Reload section config successfully\n");
			}
		}

		// Load section articles
		article_count = article_block_article_count();

		do
		{
			last_aid = article_block_last_aid();

			if ((ret = append_articles_from_db(last_aid + 1, 0, LOAD_ARTICLE_COUNT_LIMIT)) < 0)
			{
				log_error("append_articles_from_db(%d, 0, %d) error\n", last_aid + 1, LOAD_ARTICLE_COUNT_LIMIT);

				if (ret == ERR_UNKNOWN_SECTION)
				{
					SYS_conf_reload = 1; // Force reload section_list
				}
			}
		} while (ret == LOAD_ARTICLE_COUNT_LIMIT);

		load_count = article_block_article_count() - article_count;
		if (load_count > 0)
		{
			log_common("Incrementally load %d articles, last_aid = %d\n", load_count, article_block_last_aid());
		}

		if (SYS_conf_reload)
		{
			continue;
		}

		// Load article_op log
		last_mid = last_article_op_log_mid;

		do
		{
			if ((ret = apply_article_op_log_from_db(LOAD_ARTICLE_COUNT_LIMIT)) < 0)
			{
				log_error("apply_article_op_log_from_db() error\n");

				if (ret == ERR_UNKNOWN_SECTION)
				{
					SYS_conf_reload = 1; // Force reload section_list
				}
			}
		} while (ret == LOAD_ARTICLE_COUNT_LIMIT);

		if (last_article_op_log_mid > last_mid)
		{
			log_common("Proceeded %d article logs, last_mid = %d\n", last_article_op_log_mid - last_mid, last_article_op_log_mid);
		}

		// Reload user list
		if (time(NULL) - tm_user_list_reload >= BBS_user_list_load_interval)
		{
			if (user_list_pool_reload(0) < 0)
			{
				log_error("user_list_pool_reload(all_user) error\n");
			}

			if (user_stat_update() < 0)
			{
				log_error("user_stat_update() error\n");
			}

			tm_user_list_reload = time(NULL);
		}

		// Reload user online list
		if (time(NULL) - tm_user_online_list_reload >= BBS_user_online_list_load_interval)
		{
			if (user_list_pool_reload(1) < 0)
			{
				log_error("user_list_pool_reload(online_user) error\n");
			}

			tm_user_online_list_reload = time(NULL);
		}

		// Wait for BBS_section_list_load_interval seconds
		while (!SYS_server_exit && time(NULL) - tm_section_list_reload < BBS_section_list_load_interval)
		{
			sleep(1);
		}
	}

	// Child process exit

	article_cache_cleanup();

	// gen_ex_menu cleanup
	section_list_ex_menu_set_cleanup();

	// Detach data pools shm
	detach_section_list_shm();
	detach_article_block_shm();
	detach_trie_dict_shm();
	detach_user_list_pool_shm();

	log_common("Section list loader process exit normally\n");
	log_end();

	section_list_loader_pid = 0;

	_exit(0);

	return 0;
}

int query_section_articles(SECTION_LIST *p_section, int page_id, ARTICLE *p_articles[],
						   int *p_article_count, int *p_page_count, int *p_ontop_start_offset)
{
	ARTICLE *p_article;
	ARTICLE *p_next_page_first_article;
	int ret = 0;
	int i;

	if (p_section == NULL || p_articles == NULL || p_article_count == NULL || p_page_count == NULL || p_ontop_start_offset == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	// acquire lock of section
	if ((ret = section_list_rd_lock(p_section)) < 0)
	{
		log_error("section_list_rd_lock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	*p_page_count = section_list_page_count_with_ontop(p_section);
	*p_article_count = 0;
	*p_ontop_start_offset = BBS_article_limit_per_page;

	if (p_section->visible_article_count == 0)
	{
		// empty section
	}
	else if (page_id < 0 || page_id >= *p_page_count)
	{
#ifdef _DEBUG
		log_error("Invalid page_id=%d, not in range [0, %d)\n", page_id, *p_page_count);
#endif
		ret = -3;
	}
	else
	{
		ret = page_id;

		if (page_id <= p_section->page_count - 1)
		{
			p_article = p_section->p_page_first_article[page_id];
			p_next_page_first_article =
				(page_id == p_section->page_count - 1 ? p_section->p_article_head : p_section->p_page_first_article[page_id + 1]);

			do
			{
				if (p_article->visible)
				{
					p_articles[*p_article_count] = p_article;
					(*p_article_count)++;
				}
				p_article = p_article->p_next;
			} while (p_article != p_next_page_first_article && (*p_article_count) <= BBS_article_limit_per_page);

			if (*p_article_count != (page_id < p_section->page_count - 1 ? BBS_article_limit_per_page : p_section->last_page_visible_article_count))
			{
				log_error("Inconsistent visible article count %d detected in section %d page %d\n", *p_article_count, p_section->sid, page_id);
			}
		}

		*p_ontop_start_offset = *p_article_count;

		// Append ontop articles
		if (page_id >= p_section->page_count - 1)
		{
			i = (page_id == p_section->page_count - 1
					 ? 0
					 : (page_id - p_section->page_count + 1) * BBS_article_limit_per_page - p_section->last_page_visible_article_count);
			while (*p_article_count < BBS_article_limit_per_page && i < p_section->ontop_article_count)
			{
				p_articles[*p_article_count] = p_section->p_ontop_articles[i++];
				(*p_article_count)++;
			}
		}
	}

	// release lock of section
	if (section_list_rd_unlock(p_section) < 0)
	{
		log_error("section_list_rd_unlock(sid = %d) error\n", p_section->sid);
		ret = -2;
	}

	return ret;
}

int locate_article_in_section(SECTION_LIST *p_section, const ARTICLE *p_article_cur, int direction, int step,
							  int *p_page_id, int *p_visible_offset, int *p_article_count)
{
	const ARTICLE *p_article = NULL;
	const ARTICLE *p_article_last = NULL;
	ARTICLE *p_tmp;
	int32_t aid = 0;
	int page_id = 0;
	int offset = 0;
	int ret = 0;
	int i;

	if (p_section == NULL || p_article_cur == NULL || p_page_id == NULL || p_visible_offset == NULL || p_article_count == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	// acquire lock of section
	if ((ret = section_list_rd_lock(p_section)) < 0)
	{
		log_error("section_list_rd_lock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	if (direction == 0)
	{
		aid = p_article_cur->aid;
	}
	else if (direction == 1)
	{
		for (p_article_last = p_article_cur, p_article = p_article_cur->p_topic_next;
			 step > 0 && p_article->aid > p_article_cur->aid;
			 p_article = p_article->p_topic_next)
		{
			if (p_article->visible)
			{
				step--;
				p_article_last = p_article;
			}
		}
		p_article = p_article_last;

		aid = (p_article->aid > p_article_cur->aid && p_article->visible ? p_article->aid : 0);
	}
	else if (direction == -1)
	{
		for (p_article_last = p_article_cur, p_article = p_article_cur->p_topic_prior;
			 step > 0 && p_article->aid < p_article_cur->aid;
			 p_article = p_article->p_topic_prior)
		{
			if (p_article->visible)
			{
				step--;
				p_article_last = p_article;
			}
		}
		p_article = p_article_last;

		aid = (p_article->aid < p_article_cur->aid && p_article->visible ? p_article->aid : 0);
	}

	p_article = NULL;

	if (aid > 0)
	{
		p_article = section_list_find_article_with_offset(p_section, aid, &page_id, &offset, &p_tmp);
		if (p_article != NULL)
		{
			*p_article_count = (page_id >= p_section->page_count - 1 ? p_section->last_page_visible_article_count : BBS_article_limit_per_page);

			p_article = p_section->p_page_first_article[page_id];
			for (i = 0; i < *p_article_count && p_article != NULL;)
			{
				if (p_article->visible)
				{
					if (p_article->aid == aid)
					{
						*p_page_id = page_id;
						*p_visible_offset = i;
						break;
					}
					i++;
					if (i >= *p_article_count)
					{
						log_error("Visible article (aid=%d) not found in page %d\n", aid, page_id);
						p_article = NULL;
						break;
					}
				}

				p_article = p_article->p_next;
				if (p_article == p_section->p_page_first_article[page_id])
				{
					log_error("Dead loop detected at page=%d, article_count=%d\n", page_id, *p_article_count);
					break;
				}
			}

			// Include ontop articles
			*p_article_count = section_list_page_article_count_with_ontop(p_section, page_id);
		}
	}

	// release lock of section
	if (section_list_rd_unlock(p_section) < 0)
	{
		log_error("section_list_rd_unlock(sid = %d) error\n", p_section->sid);
		ret = -2;
	}

	return (ret < 0 ? ret : (p_article == NULL ? 0 : 1));
}

int last_article_in_section(SECTION_LIST *p_section, const ARTICLE **pp_article)
{
	int ret = 0;

	const ARTICLE *p_article;

	if (p_section == NULL || pp_article == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	*pp_article = NULL;

	// acquire lock of section
	if ((ret = section_list_rd_lock(p_section)) < 0)
	{
		log_error("section_list_rd_lock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	for (p_article = p_section->p_article_tail;
		 p_article && p_article != p_section->p_article_head && !p_article->visible;
		 p_article = p_article->p_prior)
		;

	if (p_article && p_article->visible)
	{
		*pp_article = p_article;
		ret = 1;
	}

	// release lock of section
	if (section_list_rd_unlock(p_section) < 0)
	{
		log_error("section_list_rd_unlock(sid = %d) error\n", p_section->sid);
		ret = -2;
	}

	return ret;
}

int scan_unread_article_in_section(SECTION_LIST *p_section, const ARTICLE *p_article_cur, const ARTICLE **pp_article_unread)
{
	ARTICLE *p_article;
	int ret = 0;

	if (p_section == NULL || p_article_cur == NULL || pp_article_unread == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_article_cur->sid != p_section->sid)
	{
		log_error("Inconsistent SID\n");
		return -1;
	}

	// acquire lock of section
	if ((ret = section_list_rd_lock(p_section)) < 0)
	{
		log_error("section_list_rd_lock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	*pp_article_unread = NULL;

	for (p_article = p_article_cur->p_next; p_article != NULL && p_article != p_article_cur; p_article = p_article->p_next)
	{
		if (p_article->visible && p_article->uid != BBS_priv.uid && !article_view_log_is_viewed(p_article->aid, &BBS_article_view_log))
		{
			*pp_article_unread = p_article;
			break;
		}
	}

	// release lock of section
	if (section_list_rd_unlock(p_section) < 0)
	{
		log_error("section_list_rd_unlock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	return (p_article != NULL && p_article != p_article_cur ? 1 : 0);
}

int scan_article_in_section_by_uid(SECTION_LIST *p_section, const ARTICLE *p_article_cur,
								   int direction, int32_t uid, const ARTICLE **pp_article)
{
	ARTICLE *p_article;
	int ret = 0;

	if (p_section == NULL || p_article_cur == NULL || pp_article == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_article_cur->sid != p_section->sid)
	{
		log_error("Inconsistent SID\n");
		return -1;
	}

	// acquire lock of section
	if ((ret = section_list_rd_lock(p_section)) < 0)
	{
		log_error("section_list_rd_lock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	*pp_article = NULL;

	if (direction >= 0)
	{
		for (p_article = p_article_cur->p_next; p_article != NULL && p_article != p_article_cur; p_article = p_article->p_next)
		{
			if (p_article->visible && p_article->uid == uid)
			{
				*pp_article = p_article;
				break;
			}
		}
	}
	else // direction < 0
	{
		for (p_article = p_article_cur->p_prior; p_article != NULL && p_article != p_article_cur; p_article = p_article->p_prior)
		{
			if (p_article->visible && p_article->uid == uid)
			{
				*pp_article = p_article;
				break;
			}
		}
	}

	// release lock of section
	if (section_list_rd_unlock(p_section) < 0)
	{
		log_error("section_list_rd_unlock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	return (p_article != NULL && p_article != p_article_cur ? 1 : 0);
}

int scan_article_in_section_by_username(SECTION_LIST *p_section, const ARTICLE *p_article_cur,
										int direction, const char *username, const ARTICLE **pp_article)
{
	ARTICLE *p_article;
	int ret = 0;

	if (p_section == NULL || p_article_cur == NULL || username == NULL || pp_article == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_article_cur->sid != p_section->sid)
	{
		log_error("Inconsistent SID\n");
		return -1;
	}

	// acquire lock of section
	if ((ret = section_list_rd_lock(p_section)) < 0)
	{
		log_error("section_list_rd_lock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	*pp_article = NULL;

	if (direction >= 0)
	{
		for (p_article = p_article_cur->p_next; p_article != NULL && p_article != p_article_cur; p_article = p_article->p_next)
		{
			if (p_article->visible && strcasecmp(p_article->username, username) == 0)
			{
				*pp_article = p_article;
				break;
			}
		}
	}
	else // direction < 0
	{
		for (p_article = p_article_cur->p_prior; p_article != NULL && p_article != p_article_cur; p_article = p_article->p_prior)
		{
			if (p_article->visible && strcasecmp(p_article->username, username) == 0)
			{
				*pp_article = p_article;
				break;
			}
		}
	}

	// release lock of section
	if (section_list_rd_unlock(p_section) < 0)
	{
		log_error("section_list_rd_unlock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	return (p_article != NULL && p_article != p_article_cur ? 1 : 0);
}

int scan_article_in_section_by_title(SECTION_LIST *p_section, const ARTICLE *p_article_cur,
									 int direction, const char *title, const ARTICLE **pp_article)
{
	ARTICLE *p_article;
	int ret = 0;

	if (p_section == NULL || p_article_cur == NULL || title == NULL || pp_article == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	if (p_article_cur->sid != p_section->sid)
	{
		log_error("Inconsistent SID\n");
		return -1;
	}

	// acquire lock of section
	if ((ret = section_list_rd_lock(p_section)) < 0)
	{
		log_error("section_list_rd_lock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	*pp_article = NULL;

	if (direction >= 0)
	{
		for (p_article = p_article_cur->p_next; p_article != NULL && p_article != p_article_cur; p_article = p_article->p_next)
		{
			if (p_article->visible && strcasestr(p_article->title, title) != NULL)
			{
				*pp_article = p_article;
				break;
			}
		}
	}
	else // direction < 0
	{
		for (p_article = p_article_cur->p_prior; p_article != NULL && p_article != p_article_cur; p_article = p_article->p_prior)
		{
			if (p_article->visible && strcasestr(p_article->title, title) != NULL)
			{
				*pp_article = p_article;
				break;
			}
		}
	}

	// release lock of section
	if (section_list_rd_unlock(p_section) < 0)
	{
		log_error("section_list_rd_unlock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	return (p_article != NULL && p_article != p_article_cur ? 1 : 0);
}

int get_section_ex_menu_set(SECTION_LIST *p_section, MENU_SET *p_ex_menu_set)
{
	int ret = 0;

	if (p_section == NULL || p_ex_menu_set == NULL)
	{
		log_error("NULL pointer error\n");
		return -1;
	}

	// acquire lock of section
	if ((ret = section_list_rd_lock(p_section)) < 0)
	{
		log_error("section_list_rd_lock(sid = %d) error\n", p_section->sid);
		return -2;
	}

	memcpy(p_ex_menu_set, &(p_section->ex_menu_set), sizeof(p_section->ex_menu_set));

	// release lock of section
	if (section_list_rd_unlock(p_section) < 0)
	{
		log_error("section_list_rd_unlock(sid = %d) error\n", p_section->sid);
		ret = -2;
	}

	return ret;
}
