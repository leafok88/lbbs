/***************************************************************************
						  init.c  -  description
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
#include "database.h"
#include "init.h"
#include "io.h"
#include "log.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CONF_DELIM_WITH_SPACE " \t\r\n"

int init_daemon(void)
{
	int pid;

	pid = fork();

	if (pid > 0) // Parent process
	{
		_exit(0);
	}
	else if (pid < 0) // error
	{
		_exit(pid);
	}

	// Child process
	setsid();

	pid = fork();

	if (pid > 0) // Parent process
	{
		_exit(0);
	}
	else if (pid < 0) // error
	{
		_exit(pid);
	}

	// Child process
	umask(022);

	return 0;
}

int load_conf(const char *conf_file)
{
	char buffer[LINE_BUFFER_LEN];
	char *saveptr = NULL;
	char *p, *q, *r;
	char *y, *m, *d;
	FILE *fin;

	// Load configuration
	if ((fin = fopen(conf_file, "r")) == NULL)
	{
		log_error("Open %s failed", conf_file);
		return -1;
	}

	while (fgets(buffer, sizeof(buffer), fin) != NULL)
	{
		p = strtok_r(buffer, CONF_DELIM_WITH_SPACE, &saveptr);
		if (p == NULL) // Blank line
		{
			continue;
		}

		if (*p == '#' || *p == '\r' || *p == '\n') // Comment or blank line
		{
			continue;
		}

		q = strtok_r(NULL, CONF_DELIM_WITH_SPACE, &saveptr);
		if (q == NULL) // Empty value
		{
			log_error("Skip empty value of config item: %s\n", p);
			continue;
		}

		// Check syntax
		r = strtok_r(NULL, CONF_DELIM_WITH_SPACE, &saveptr);
		if (r != NULL && r[0] != '#')
		{
			log_error("Skip config line with extra value %s = %s %s\n", p, q, r);
			continue;
		}

		if (strcasecmp(p, "bbs_id") == 0)
		{
			strncpy(BBS_id, q, sizeof(BBS_id) - 1);
			BBS_id[sizeof(BBS_id) - 1] = '\0';
		}
		else if (strcasecmp(p, "bbs_name") == 0)
		{
			strncpy(BBS_name, q, sizeof(BBS_name) - 1);
			BBS_name[sizeof(BBS_name) - 1] = '\0';
		}
		else if (strcasecmp(p, "bbs_server") == 0)
		{
			strncpy(BBS_server, q, sizeof(BBS_server) - 1);
			BBS_server[sizeof(BBS_server) - 1] = '\0';
		}
		else if (strcasecmp(p, "bbs_address") == 0)
		{
			strncpy(BBS_address, q, sizeof(BBS_address) - 1);
			BBS_address[sizeof(BBS_address) - 1] = '\0';
		}
		else if (strcasecmp(p, "bbs_port") == 0)
		{
			BBS_port[0] = (in_port_t)atoi(q);
		}
		else if (strcasecmp(p, "bbs_ssh_port") == 0)
		{
			BBS_port[1] = (in_port_t)atoi(q);
		}
		else if (strcasecmp(p, "bbs_max_client") == 0)
		{
			BBS_max_client = atoi(q);
			if (BBS_max_client <= 0 || BBS_max_client > MAX_CLIENT_LIMIT)
			{
				log_error("Ignore config bbs_max_client with incorrect value %d\n", BBS_max_client);
				BBS_max_client = MAX_CLIENT_LIMIT;
			}
		}
		else if (strcasecmp(p, "bbs_max_client_per_ip") == 0)
		{
			BBS_max_client_per_ip = atoi(q);
			if (BBS_max_client_per_ip <= 0 || BBS_max_client_per_ip > MAX_CLIENT_PER_IP_LIMIT)
			{
				log_error("Ignore config bbs_max_client with incorrect value %d\n", BBS_max_client_per_ip);
				BBS_max_client_per_ip = MAX_CLIENT_PER_IP_LIMIT;
			}
		}
		else if (strcasecmp(p, "bbs_max_user") == 0)
		{
			BBS_max_user = atoi(q);
			if (BBS_max_user <= 0 || BBS_max_user > BBS_MAX_USER_LIMIT)
			{
				log_error("Ignore config bbs_max_client with incorrect value %d\n", BBS_max_user);
				BBS_max_user = BBS_MAX_USER_LIMIT;
			}
		}
		else if (strcasecmp(p, "bbs_start_dt") == 0)
		{
			y = strtok_r(q, "-", &saveptr);
			m = strtok_r(NULL, "-", &saveptr);
			d = strtok_r(NULL, "-", &saveptr);

			if (y == NULL || m == NULL || d == NULL)
			{
				log_error("Ignore config bbs_start_dt with incorrect value\n");
				continue;
			}
			snprintf(BBS_start_dt, sizeof(BBS_start_dt), "%4s年%2s月%2s日", y, m, d);
		}
		else if (strcasecmp(p, "db_host") == 0)
		{
			strncpy(DB_host, q, sizeof(DB_host) - 1);
			DB_host[sizeof(DB_host) - 1] = '\0';
		}
		else if (strcasecmp(p, "db_username") == 0)
		{
			strncpy(DB_username, q, sizeof(DB_username) - 1);
			DB_username[sizeof(DB_username) - 1] = '\0';
		}
		else if (strcasecmp(p, "db_password") == 0)
		{
			strncpy(DB_password, q, sizeof(DB_password) - 1);
			DB_password[sizeof(DB_password) - 1] = '\0';
		}
		else if (strcasecmp(p, "db_database") == 0)
		{
			strncpy(DB_database, q, sizeof(DB_database) - 1);
			DB_database[sizeof(DB_database) - 1] = '\0';
		}
		else if (strcasecmp(p, "db_timezone") == 0)
		{
			strncpy(DB_timezone, q, sizeof(DB_timezone) - 1);
			DB_timezone[sizeof(DB_timezone) - 1] = '\0';
		}
		else
		{
			log_error("Unknown config %s = %s\n", p, q);
		}
	}

	fclose(fin);

	return 0;
}
