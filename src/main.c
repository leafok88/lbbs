/***************************************************************************
						  main.c  -  description
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

#include "bbs.h"
#include "common.h"
#include "file_loader.h"
#include "init.h"
#include "io.h"
#include "log.h"
#include "menu.h"
#include "net_server.h"
#include "section_list_loader.h"
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static void app_help(void)
{
	fprintf(stderr, "Usage: bbsd [-fhv] [...]\n\n"
					"-f\t--foreground\t\tForce program run in foreground\n"
					"-h\t--help\t\t\tDisplay this help message\n"
					"-v\t--version\t\tDisplay version information\n"
					"\t--display-log\t\tDisplay standard log information\n"
					"\t--display-error-log\tDisplay error log information\n"
					"\n    If meet any bug, please report to <leaflet@leafok.com>\n\n");
}

static void arg_error(void)
{
	fprintf(stderr, "Invalid arguments\n");
	app_help();
}

int main(int argc, char *argv[])
{
	char file_path_temp[FILE_PATH_LEN];
	int daemon = 1;
	int std_log_redir = 0;
	int error_log_redir = 0;
	FILE *fp;
	int ret;
	int last_aid;
	struct sigaction act = {0};

	// Parse args
	for (int i = 1; i < argc; i++)
	{
		switch (argv[i][0])
		{
		case '-':
			if (argv[i][1] != '-')
			{
				for (int j = 1; j < strlen(argv[i]); j++)
				{
					switch (argv[i][j])
					{
					case 'f':
						daemon = 0;
						break;
					case 'h':
						app_help();
						return 0;
					case 'v':
						puts(APP_INFO);
						return 0;
					default:
						arg_error();
						return 1;
					}
				}
			}
			else
			{
				if (strcmp(argv[i] + 2, "foreground") == 0)
				{
					daemon = 0;
					break;
				}
				if (strcmp(argv[i] + 2, "help") == 0)
				{
					app_help();
					return 0;
				}
				if (strcmp(argv[i] + 2, "version") == 0)
				{
					puts(APP_INFO);
					return 0;
				}
				if (strcmp(argv[i] + 2, "display-log") == 0)
				{
					std_log_redir = 1;
				}
				if (strcmp(argv[i] + 2, "display-error-log") == 0)
				{
					error_log_redir = 1;
				}
			}
			break;
		}
	}

	// Initialize daemon
	if (daemon)
	{
		init_daemon();
	}

	// Change current dir
	strncpy(file_path_temp, argv[0], sizeof(file_path_temp) - 1);
	file_path_temp[sizeof(file_path_temp) - 1] = '\0';

	chdir(dirname(file_path_temp));
	chdir("..");

	// Initialize log
	if (log_begin(LOG_FILE_INFO, LOG_FILE_ERROR) < 0)
	{
		return -1;
	}

	if ((!daemon) && std_log_redir)
	{
		log_common_redir(STDERR_FILENO);
	}
	if ((!daemon) && error_log_redir)
	{
		log_error_redir(STDERR_FILENO);
	}

	log_common("Starting %s\n", APP_INFO);

	// Load configuration
	if (load_conf(CONF_BBSD) < 0)
	{
		return -2;
	}

	// Check article cache dir
	ret = mkdir(VAR_ARTICLE_CACHE_DIR, 0750);
	if (ret == -1 && errno != EEXIST)
	{
		log_error("mkdir(%s) error (%d)\n", VAR_ARTICLE_CACHE_DIR, errno);
		goto cleanup;
	}

	// Initialize data pools
	if ((fp = fopen(VAR_TRIE_DICT_SHM, "w")) == NULL)
	{
		log_error("fopen(%s) error\n", VAR_TRIE_DICT_SHM);
		goto cleanup;
	}
	fclose(fp);
	if ((fp = fopen(VAR_ARTICLE_BLOCK_SHM, "w")) == NULL)
	{
		log_error("fopen(%s) error\n", VAR_ARTICLE_BLOCK_SHM);
		goto cleanup;
	}
	fclose(fp);
	if ((fp = fopen(VAR_SECTION_LIST_SHM, "w")) == NULL)
	{
		log_error("fopen(%s) error\n", VAR_SECTION_LIST_SHM);
		goto cleanup;
	}
	fclose(fp);

	if (trie_dict_init(VAR_TRIE_DICT_SHM, TRIE_NODE_PER_POOL) < 0)
	{
		printf("trie_dict_init failed\n");
		goto cleanup;
	}
	if (article_block_init(VAR_ARTICLE_BLOCK_SHM, BBS_article_limit_per_section * BBS_max_section / ARTICLE_PER_BLOCK) < 0)
	{
		log_error("article_block_init(%s, %d) error\n", VAR_ARTICLE_BLOCK_SHM, BBS_article_limit_per_section * BBS_max_section / ARTICLE_PER_BLOCK);
		goto cleanup;
	}
	if (section_list_init(VAR_SECTION_LIST_SHM) < 0)
	{
		log_error("section_list_pool_init(%s) error\n", VAR_SECTION_LIST_SHM);
		goto cleanup;
	}

	// Load BBS cmd
	if (load_cmd() < 0)
	{
		goto cleanup;
	}

	// Load menus
	p_bbs_menu = calloc(1, sizeof(MENU_SET));
	if (p_bbs_menu == NULL)
	{
		log_error("OOM: calloc(MENU_SET)\n");
		goto cleanup;
	}
	if (load_menu(p_bbs_menu, CONF_MENU) < 0)
	{
		goto cleanup;
	}

	// Load data files
	if (file_loader_init() < 0)
	{
		log_error("file_loader_init() error\n");
		goto cleanup;
	}
	for (int i = 0; i < data_files_load_startup_count; i++)
	{
		if (load_file(data_files_load_startup[i]) < 0)
		{
			log_error("load_file_mmap(%s) error\n", data_files_load_startup[i]);
		}
	}

	// Load section config
	if (load_section_config_from_db(0) < 0)
	{
		log_error("load_section_config_from_db() error\n");
		goto cleanup;
	}

	set_last_article_op_log_from_db();
	last_aid = 0;

	// Load section articles
	do
	{
		if ((ret = append_articles_from_db(last_aid + 1, 1, LOAD_ARTICLE_COUNT_LIMIT)) < 0)
		{
			log_error("append_articles_from_db(0, 1, %d) error\n", LOAD_ARTICLE_COUNT_LIMIT);
			goto cleanup;
		}

		last_aid = article_block_last_aid();
	} while (ret == LOAD_ARTICLE_COUNT_LIMIT);

	log_common("Initially load %d articles, last_aid = %d\n", article_block_article_count(), article_block_last_aid());

	// Set signal handler
	act.sa_handler = sig_hup_handler;
	if (sigaction(SIGHUP, &act, NULL) == -1)
	{
		log_error("set signal action of SIGHUP error: %d\n", errno);
		goto cleanup;
	}
	act.sa_handler = sig_chld_handler;
	if (sigaction(SIGCHLD, &act, NULL) == -1)
	{
		log_error("set signal action of SIGCHLD error: %d\n", errno);
		goto cleanup;
	}
	act.sa_handler = sig_term_handler;
	if (sigaction(SIGTERM, &act, NULL) == -1)
	{
		log_error("set signal action of SIGTERM error: %d\n", errno);
		goto cleanup;
	}

	// Launch section_list_loader process
	if (section_list_loader_launch() < 0)
	{
		log_error("section_list_loader_launch() error\n");
		goto cleanup;
	}

	// Initialize socket server
	net_server(BBS_address, BBS_port);

cleanup:
	// Cleanup loaded data files
	file_loader_cleanup();

	// Cleanup menu
	unload_menu(p_bbs_menu);
	free(p_bbs_menu);
	p_bbs_menu = NULL;

	// Cleanup data pools
	section_list_cleanup();
	article_block_cleanup();
	trie_dict_cleanup();

	if (unlink(VAR_TRIE_DICT_SHM) < 0)
	{
		log_error("unlink(%s) error\n", VAR_TRIE_DICT_SHM);
	}
	if (unlink(VAR_ARTICLE_BLOCK_SHM) < 0)
	{
		log_error("unlink(%s) error\n", VAR_ARTICLE_BLOCK_SHM);
	}
	if (unlink(VAR_SECTION_LIST_SHM) < 0)
	{
		log_error("unlink(%s) error\n", VAR_SECTION_LIST_SHM);
	}

	log_common("Main process exit normally\n");

	log_end();

	return 0;
}
