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
#include "lml.h"
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

typedef void (*arg_option_handler_t)(void);

struct arg_option_t
{
	const char *short_arg;
	const char *long_arg;
	const char *description;
	arg_option_handler_t handler;
};
typedef struct arg_option_t ARG_OPTION;

static void arg_foreground(void);
static void arg_help(void);
static void arg_version(void);
static void arg_display_log(void);
static void arg_display_error_log(void);
static void arg_compile_info(void);
static void arg_error(void);

static const ARG_OPTION arg_options[] = {
	{"-f", "--foreground", "Run in foreground", arg_foreground},
	{"-h", "--help", "Display help message", arg_help},
	{"-v", "--version", "Display version information", arg_version},
	{NULL, "--display-log", "Display standard log information", arg_display_log},
	{NULL, "--display-error-log", "Display error log information", arg_display_error_log},
	{"-C", "--compile-config", "Display compile configuration", arg_compile_info}};

static const int arg_options_count = sizeof(arg_options) / sizeof(ARG_OPTION);

static int daemon_service = 1;
static int std_log_redir = 0;
static int error_log_redir = 0;

static void arg_foreground(void)
{
	daemon_service = 0;
}

static void arg_help(void)
{
	fprintf(stderr, "Usage: bbsd [-fhv] [...]\n\n");

	for (int i = 0; i < arg_options_count; i++)
	{
		fprintf(stderr, "%s%*s%s%*s%s\n",
				arg_options[i].short_arg ? arg_options[i].short_arg : "",
				8 - (arg_options[i].short_arg ? (int)strlen(arg_options[i].short_arg) : 0), "",
				arg_options[i].long_arg ? arg_options[i].long_arg : "",
				24 - (arg_options[i].long_arg ? (int)strlen(arg_options[i].long_arg) : 0), "",
				arg_options[i].description ? arg_options[i].description : "");
	}

	fprintf(stderr, "\n    If meet any bug, please report to <leaflet@leafok.com>\n\n");
}

static void arg_version(void)
{
	printf("%s\n", APP_INFO);
	printf("%s\n", COPYRIGHT_INFO);
}

static void arg_display_log(void)
{
	std_log_redir = 1;
}

static void arg_display_error_log(void)
{
	error_log_redir = 1;
}

static void arg_compile_info(void)
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

static void arg_error(void)
{
	fprintf(stderr, "Invalid arguments\n");
	arg_help();
}

int main(int argc, char *argv[])
{
	char file_path_temp[FILE_PATH_LEN];
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
		for (j = 0; j < arg_options_count; j++)
		{
			if (arg_options[j].short_arg && strcmp(argv[i], arg_options[j].short_arg) == 0)
			{
				arg_options[j].handler();
				break;
			}
			else if (arg_options[j].long_arg && strcmp(argv[i], arg_options[j].long_arg) == 0)
			{
				arg_options[j].handler();
				break;
			}
		}
		if (j == arg_options_count)
		{
			arg_error();
			return -1;
		}
	}

	// Initialize daemon
	if (daemon_service)
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

	if ((!daemon_service) && std_log_redir)
	{
		log_common_redir(STDERR_FILENO);
	}
	if ((!daemon_service) && error_log_redir)
	{
		log_error_redir(STDERR_FILENO);
	}

	log_common("Starting %s", APP_INFO);

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
		log_error("stat(%s) error", DATA_EULA, errno);
		goto cleanup;
	}
	BBS_eula_tm = file_stat.st_mtim.tv_sec;

	// Check article cache dir
	ret = mkdir(VAR_ARTICLE_CACHE_DIR, 0750);
	if (ret == -1 && errno != EEXIST)
	{
		log_error("mkdir(%s) error (%d)", VAR_ARTICLE_CACHE_DIR, errno);
		goto cleanup;
	}

	// Check section aid location dir
	ret = mkdir(VAR_SECTION_AID_LOC_DIR, 0750);
	if (ret == -1 && errno != EEXIST)
	{
		log_error("mkdir(%s) error (%d)", VAR_SECTION_AID_LOC_DIR, errno);
		goto cleanup;
	}

	// Initialize data pools
	if ((fp = fopen(VAR_ARTICLE_BLOCK_SHM, "w")) == NULL)
	{
		log_error("fopen(%s) error", VAR_ARTICLE_BLOCK_SHM);
		goto cleanup;
	}
	fclose(fp);
	if ((fp = fopen(VAR_SECTION_LIST_SHM, "w")) == NULL)
	{
		log_error("fopen(%s) error", VAR_SECTION_LIST_SHM);
		goto cleanup;
	}
	fclose(fp);
	if ((fp = fopen(VAR_TRIE_DICT_SHM, "w")) == NULL)
	{
		log_error("fopen(%s) error", VAR_TRIE_DICT_SHM);
		goto cleanup;
	}
	fclose(fp);
	if ((fp = fopen(VAR_USER_LIST_SHM, "w")) == NULL)
	{
		log_error("fopen(%s) error", VAR_USER_LIST_SHM);
		goto cleanup;
	}
	fclose(fp);

	if (trie_dict_init(VAR_TRIE_DICT_SHM, TRIE_NODE_PER_POOL) < 0)
	{
		log_error("trie_dict_init(%s, %d) error", VAR_TRIE_DICT_SHM, TRIE_NODE_PER_POOL);
		goto cleanup;
	}
	if (article_block_init(VAR_ARTICLE_BLOCK_SHM, BBS_article_limit_per_section * BBS_max_section / BBS_article_count_per_block) < 0)
	{
		log_error("article_block_init(%s, %d) error", VAR_ARTICLE_BLOCK_SHM, BBS_article_limit_per_section * BBS_max_section / BBS_article_count_per_block);
		goto cleanup;
	}
	if (section_list_init(VAR_SECTION_LIST_SHM) < 0)
	{
		log_error("section_list_pool_init(%s) error", VAR_SECTION_LIST_SHM);
		goto cleanup;
	}

	// Init LML module
	if (lml_init() < 0)
	{
		log_error("lml_init() error");
		goto cleanup;
	}

	// Load BBS cmd
	if (load_cmd() < 0)
	{
		log_error("load_cmd() error");
		goto cleanup;
	}

	// Load menus
	if (load_menu(&bbs_menu, CONF_MENU) < 0)
	{
		log_error("load_menu(bbs_menu) error");
		goto cleanup;
	}
	if (load_menu(&top10_menu, CONF_TOP10_MENU) < 0)
	{
		log_error("load_menu(top10_menu) error");
		goto cleanup;
	}
	top10_menu.allow_exit = 1;

	// Load data files
	for (int i = 0; i < data_files_load_startup_count; i++)
	{
		if (load_file(data_files_load_startup[i]) < 0)
		{
			log_error("load_file(%s) error", data_files_load_startup[i]);
		}
	}

	// Load user_list and online_user_list
	if (user_list_pool_init(VAR_USER_LIST_SHM) < 0)
	{
		log_error("user_list_pool_init(%s) error", VAR_USER_LIST_SHM);
		goto cleanup;
	}
	if (user_list_pool_reload(0) < 0)
	{
		log_error("user_list_pool_reload(all_user) error");
		goto cleanup;
	}
	if (user_list_pool_reload(1) < 0)
	{
		log_error("user_list_pool_reload(online_user) error");
		goto cleanup;
	}

	// Load section config and gen_ex
	if (load_section_config_from_db(1) < 0)
	{
		log_error("load_section_config_from_db(0) error");
		goto cleanup;
	}

	set_last_article_op_log_from_db();
	last_aid = 0;

	// Load section articles
	do
	{
		if ((ret = append_articles_from_db(last_aid + 1, 1, LOAD_ARTICLE_COUNT_LIMIT)) < 0)
		{
			log_error("append_articles_from_db(0, 1, %d) error", LOAD_ARTICLE_COUNT_LIMIT);
			goto cleanup;
		}

		last_aid = article_block_last_aid();
	} while (ret == LOAD_ARTICLE_COUNT_LIMIT);

	log_common("Initially load %d articles, last_aid = %d", article_block_article_count(), article_block_last_aid());

	if ((ret = user_stat_update()) < 0)
	{
		log_error("user_stat_update() error");
		goto cleanup;
	}

	// Set signal handler
	act.sa_handler = sig_hup_handler;
	if (sigaction(SIGHUP, &act, NULL) == -1)
	{
		log_error("set signal action of SIGHUP error: %d", errno);
		goto cleanup;
	}
	act.sa_handler = sig_chld_handler;
	if (sigaction(SIGCHLD, &act, NULL) == -1)
	{
		log_error("set signal action of SIGCHLD error: %d", errno);
		goto cleanup;
	}
	act.sa_handler = sig_term_handler;
	if (sigaction(SIGTERM, &act, NULL) == -1)
	{
		log_error("set signal action of SIGTERM error: %d", errno);
		goto cleanup;
	}
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &act, NULL) == -1)
	{
		log_error("set signal action of SIGPIPE error: %d", errno);
		goto cleanup;
	}
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGUSR1, &act, NULL) == -1)
	{
		log_error("set signal action of SIGUSR1 error: %d", errno);
		goto cleanup;
	}

	// Launch section_list_loader process
	if (section_list_loader_launch() < 0)
	{
		log_error("section_list_loader_launch() error");
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
			log_error("Send SIGTERM signal failed (%d)", errno);
		}

		for (i = 0; SYS_child_exit == 0 && i < 5; i++)
		{
			sleep(1); // second
		}

		if ((ret = waitpid(section_list_loader_pid, NULL, WNOHANG)) < 0)
		{
			log_error("waitpid(%d) error (%d)", section_list_loader_pid, errno);
		}
		else if (ret == 0)
		{
			log_common("Force kill section_list_loader process (%d)", section_list_loader_pid);
			if (kill(section_list_loader_pid, SIGKILL) < 0)
			{
				log_error("Send SIGKILL signal failed (%d)", errno);
			}
		}
	}

	// Cleanup loaded data files
	for (int i = 0; i < data_files_load_startup_count; i++)
	{
		if (unload_file(data_files_load_startup[i]) < 0)
		{
			log_error("unload_file(%s) error", data_files_load_startup[i]);
		}
	}

	// Cleanup menu
	unload_menu(&bbs_menu);
	unload_menu(&top10_menu);

	// Cleanup LML module
	lml_cleanup();

	// Cleanup data pools
	section_list_cleanup();
	article_block_cleanup();
	trie_dict_cleanup();
	user_list_pool_cleanup();

	if (unlink(VAR_ARTICLE_BLOCK_SHM) < 0)
	{
		log_error("unlink(%s) error", VAR_ARTICLE_BLOCK_SHM);
	}
	if (unlink(VAR_SECTION_LIST_SHM) < 0)
	{
		log_error("unlink(%s) error", VAR_SECTION_LIST_SHM);
	}
	if (unlink(VAR_TRIE_DICT_SHM) < 0)
	{
		log_error("unlink(%s) error", VAR_TRIE_DICT_SHM);
	}
	if (unlink(VAR_USER_LIST_SHM) < 0)
	{
		log_error("unlink(%s) error", VAR_SECTION_LIST_SHM);
	}

	log_common("Main process exit normally");

	log_end();

	return 0;
}
