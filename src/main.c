/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * main
 *   - entry of server program
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bbs.h"
#include "bwf.h"
#include "common.h"
#include "file_loader.h"
#include "init.h"
#include "io.h"
#include "log.h"
#include "menu.h"
#include "net_server.h"
#include "section_list_loader.h"
#include "user_list.h"
#include <errno.h>
#include <libgen.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

static inline void app_help(void)
{
	fprintf(stderr, "Usage: bbsd [-fhv] [...]\n\n"
					"-f\t--foreground\t\tForce program run in foreground\n"
					"-h\t--help\t\t\tDisplay this help message\n"
					"-v\t--version\t\tDisplay version information\n"
					"\t--display-log\t\tDisplay standard log information\n"
					"\t--display-error-log\tDisplay error log information\n"
					"-C\t--compile-config\tDisplay compile configuration\n"
					"\n    If meet any bug, please report to <leaflet@leafok.com>\n\n");
}

static inline void arg_error(void)
{
	fprintf(stderr, "Invalid arguments\n");
	app_help();
}

static inline void app_compile_info(void)
{
	printf("%s\n"
		   "--enable-shared\t\t[%s]\n"
		   "--enable-systemd\t[%s]\n"
		   "--with-debug\t\t[%s]\n"
		   "--with-epoll\t\t[%s]\n"
		   "--with-mariadb\t\t[%s]\n"
		   "--with-sysv\t\t[%s]\n",
		   APP_INFO,
#ifdef _ENABLE_SHARED
		   "yes",
#else
		   "no",
#endif
#ifdef HAVE_SYSTEMD_SD_DAEMON_H
		   "yes",
#else
		   "no",
#endif
#ifdef _DEBUG
		   "yes",
#else
		   "no",
#endif
#ifdef HAVE_SYS_EPOLL_H
		   "yes",
#else
		   "no",
#endif
#ifdef HAVE_MARIADB_CLIENT
		   "yes",
#else
		   "no",
#endif
#ifdef HAVE_SYSTEM_V
		   "yes"
#else
		   "no"
#endif
	);
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
	int i;
	int j;
	struct stat file_stat;

	// Parse args
	for (i = 1; i < argc; i++)
	{
		switch (argv[i][0])
		{
		case '-':
			if (argv[i][1] != '-')
			{
				for (j = 1; j < strlen(argv[i]); j++)
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
						printf("%s\n", APP_INFO);
						printf("%s\n", COPYRIGHT_INFO);
						return 0;
					case 'C':
						app_compile_info();
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
					printf("%s\n", APP_INFO);
					printf("%s\n", COPYRIGHT_INFO);
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
				if (strcmp(argv[i] + 2, "compile-config") == 0)
				{
					app_compile_info();
					return 0;
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

	if (chdir(dirname(file_path_temp)) < 0)
	{
		fprintf(stderr, "chdir(%s) error: %d\n", dirname(file_path_temp), errno);
		return -1;
	}
	if (chdir("..") < 0)
	{
		fprintf(stderr, "chdir(..) error: %d\n", errno);
		return -1;
	}

	// Apply the specified locale
	if (setlocale(LC_ALL, "en_US.UTF-8") == NULL)
	{
		fprintf(stderr, "setlocale(LC_ALL, en_US.UTF-8) error\n");
		return -1;
	}

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

	// Load BWF config
	if (bwf_load(CONF_BWF) < 0)
	{
		return -2;
	}

	// Get EULA modification tm
	if (stat(DATA_EULA, &file_stat) == -1)
	{
		log_error("stat(%s) error\n", DATA_EULA, errno);
		goto cleanup;
	}
	BBS_eula_tm = file_stat.st_mtim.tv_sec;

	// Check article cache dir
	ret = mkdir(VAR_ARTICLE_CACHE_DIR, 0750);
	if (ret == -1 && errno != EEXIST)
	{
		log_error("mkdir(%s) error (%d)\n", VAR_ARTICLE_CACHE_DIR, errno);
		goto cleanup;
	}

	// Check section aid location dir
	ret = mkdir(VAR_SECTION_AID_LOC_DIR, 0750);
	if (ret == -1 && errno != EEXIST)
	{
		log_error("mkdir(%s) error (%d)\n", VAR_SECTION_AID_LOC_DIR, errno);
		goto cleanup;
	}

	// Initialize data pools
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
	if ((fp = fopen(VAR_TRIE_DICT_SHM, "w")) == NULL)
	{
		log_error("fopen(%s) error\n", VAR_TRIE_DICT_SHM);
		goto cleanup;
	}
	fclose(fp);
	if ((fp = fopen(VAR_USER_LIST_SHM, "w")) == NULL)
	{
		log_error("fopen(%s) error\n", VAR_USER_LIST_SHM);
		goto cleanup;
	}
	fclose(fp);

	if (trie_dict_init(VAR_TRIE_DICT_SHM, TRIE_NODE_PER_POOL) < 0)
	{
		log_error("trie_dict_init(%s, %d) error\n", VAR_TRIE_DICT_SHM, TRIE_NODE_PER_POOL);
		goto cleanup;
	}
	if (article_block_init(VAR_ARTICLE_BLOCK_SHM, BBS_article_limit_per_section * BBS_max_section / BBS_article_count_per_block) < 0)
	{
		log_error("article_block_init(%s, %d) error\n", VAR_ARTICLE_BLOCK_SHM, BBS_article_limit_per_section * BBS_max_section / BBS_article_count_per_block);
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
	if (load_menu(&bbs_menu, CONF_MENU) < 0)
	{
		log_error("load_menu(bbs_menu) error\n");
		goto cleanup;
	}
	if (load_menu(&top10_menu, CONF_TOP10_MENU) < 0)
	{
		log_error("load_menu(top10_menu) error\n");
		goto cleanup;
	}
	top10_menu.allow_exit = 1;

	// Load data files
	for (int i = 0; i < data_files_load_startup_count; i++)
	{
		if (load_file(data_files_load_startup[i]) < 0)
		{
			log_error("load_file(%s) error\n", data_files_load_startup[i]);
		}
	}

	// Load user_list and online_user_list
	if (user_list_pool_init(VAR_USER_LIST_SHM) < 0)
	{
		log_error("user_list_pool_init(%s) error\n", VAR_USER_LIST_SHM);
		goto cleanup;
	}
	if (user_list_pool_reload(0) < 0)
	{
		log_error("user_list_pool_reload(all_user) error\n");
		goto cleanup;
	}
	if (user_list_pool_reload(1) < 0)
	{
		log_error("user_list_pool_reload(online_user) error\n");
		goto cleanup;
	}

	// Load section config and gen_ex
	if (load_section_config_from_db(1) < 0)
	{
		log_error("load_section_config_from_db(0) error\n");
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

	if ((ret = user_stat_update()) < 0)
	{
		log_error("user_stat_update() error\n");
		goto cleanup;
	}

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
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &act, NULL) == -1)
	{
		log_error("set signal action of SIGPIPE error: %d\n", errno);
		goto cleanup;
	}
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGUSR1, &act, NULL) == -1)
	{
		log_error("set signal action of SIGUSR1 error: %d\n", errno);
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
	// Cleanup loader process if still running
	if (SYS_child_process_count > 0)
	{
		SYS_child_exit = 0;

		if (kill(section_list_loader_pid, SIGTERM) < 0)
		{
			log_error("Send SIGTERM signal failed (%d)\n", errno);
		}

		for (i = 0; SYS_child_exit == 0 && i < 5; i++)
		{
			sleep(1); // second
		}

		if ((ret = waitpid(section_list_loader_pid, NULL, WNOHANG)) < 0)
		{
			log_error("waitpid(%d) error (%d)\n", section_list_loader_pid, errno);
		}
		else if (ret == 0)
		{
			log_common("Force kill section_list_loader process (%d)\n", section_list_loader_pid);
			if (kill(section_list_loader_pid, SIGKILL) < 0)
			{
				log_error("Send SIGKILL signal failed (%d)\n", errno);
			}
		}
	}

	// Cleanup loaded data files
	for (int i = 0; i < data_files_load_startup_count; i++)
	{
		if (unload_file(data_files_load_startup[i]) < 0)
		{
			log_error("unload_file(%s) error\n", data_files_load_startup[i]);
		}
	}

	// Cleanup menu
	unload_menu(&bbs_menu);
	unload_menu(&top10_menu);

	// Cleanup data pools
	section_list_cleanup();
	article_block_cleanup();
	trie_dict_cleanup();
	user_list_pool_cleanup();

	if (unlink(VAR_ARTICLE_BLOCK_SHM) < 0)
	{
		log_error("unlink(%s) error\n", VAR_ARTICLE_BLOCK_SHM);
	}
	if (unlink(VAR_SECTION_LIST_SHM) < 0)
	{
		log_error("unlink(%s) error\n", VAR_SECTION_LIST_SHM);
	}
	if (unlink(VAR_TRIE_DICT_SHM) < 0)
	{
		log_error("unlink(%s) error\n", VAR_TRIE_DICT_SHM);
	}
	if (unlink(VAR_USER_LIST_SHM) < 0)
	{
		log_error("unlink(%s) error\n", VAR_SECTION_LIST_SHM);
	}

	log_common("Main process exit normally\n");

	log_end();

	return 0;
}
