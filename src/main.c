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
#include "init.h"
#include "common.h"
#include "net_server.h"
#include "log.h"
#include "io.h"
#include "menu.h"
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>

void app_help(void)
{
	prints("Usage: bbsd [-fhv] [...]\n\n"
		   "-f\t--foreground\t\tForce program run in foreground\n"
		   "-h\t--help\t\t\tDisplay this help message\n"
		   "-v\t--version\t\tDisplay version information\n"
		   "\t--display-log\t\tDisplay standard log information\n"
		   "\t--display-error-log\tDisplay error log information\n"
		   "\n    If meet any bug, please report to <leaflet@leafok.com>\n\n");
}

void arg_error(void)
{
	prints("Invalid arguments\n");
	app_help();
}

int main(int argc, char *argv[])
{
	char file_path_temp[FILE_PATH_LEN];
	int daemon = 1;
	int std_log_redir = 0;
	int error_log_redir = 0;
	int ret = 0;

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
						exit(0);
					case 'v':
						puts(app_version);
						exit(0);
					default:
						arg_error();
						exit(1);
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
					exit(0);
				}
				if (strcmp(argv[i] + 2, "version") == 0)
				{
					puts(app_version);
					exit(0);
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
		exit(-1);
	}

	if ((!daemon) && std_log_redir)
	{
		log_std_redirect(STDERR_FILENO);
	}
	if ((!daemon) && error_log_redir)
	{
		log_err_redirect(STDERR_FILENO);
	}

	// Load configuration
	if (load_conf(CONF_BBSD) < 0)
	{
		exit(-2);
	}

	// Load menus
	if (load_menu(&bbs_menu, CONF_MENU) < 0)
	{
		unload_menu(&bbs_menu);
		exit(-3);
	}

	// Set signal handler
	signal(SIGHUP, sig_hup_handler);
	signal(SIGCHLD, sig_chld_handler);
	signal(SIGTERM, sig_term_handler);

	// Initialize socket server
	if (net_server(BBS_address, BBS_port) < 0)
	{
		ret = -4;;
	}

	// Cleanup
	unload_menu(&bbs_menu);

	log_std("Main process exit normally\n");
	
	return ret;
}
