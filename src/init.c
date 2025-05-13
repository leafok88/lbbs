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

#include "init.h"
#include "database.h"
#include "bbs.h"
#include "common.h"
#include "log.h"
#include "io.h"
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int init_daemon(void)
{
	int pid;

	pid = fork();

	if (pid != 0) // Parent or error
	{
		return pid;
	}

	// Child process
	setsid();

	pid = fork();

	if (pid != 0) // Parent or error
	{
		return pid;
	}

	// Child process
	umask(022);

	return 0;
}

int load_conf(const char *conf_file)
{
	char temp[256];

	// Load configuration
	char c_name[256];
	FILE *fin;

	if ((fin = fopen(conf_file, "r")) == NULL)
	{
		log_error("Open %s failed", conf_file);
		return -1;
	}

	while (fscanf(fin, "%s", c_name) != EOF)
	{
		if (c_name[0] == '#')
		{
			fgets(temp, 256, fin);
			continue;
		}
		fscanf(fin, "%*c");
		if (strcmp(c_name, "bbs_id") == 0)
		{
			fscanf(fin, "%s", BBS_id);
		}
		if (strcmp(c_name, "bbs_name") == 0)
		{
			fscanf(fin, "%s", BBS_name);
		}
		if (strcmp(c_name, "bbs_server") == 0)
		{
			fscanf(fin, "%s", BBS_server);
		}
		if (strcmp(c_name, "bbs_address") == 0)
		{
			fscanf(fin, "%s", BBS_address);
		}
		if (strcmp(c_name, "bbs_port") == 0)
		{
			fscanf(fin, "%hu", &BBS_port);
		}
		if (strcmp(c_name, "bbs_max_client") == 0)
		{
			fscanf(fin, "%d", &BBS_max_client);
		}
		if (strcmp(c_name, "bbs_max_user") == 0)
		{
			fscanf(fin, "%d", &BBS_max_user);
		}
		if (strcmp(c_name, "bbs_start_dt") == 0)
		{
			int y = 0, m = 0, d = 0;
			fscanf(fin, "%d-%d-%d", &y, &m, &d);
			snprintf(BBS_start_dt, sizeof(BBS_start_dt), "%4dƒÍ%2d‘¬%2d»’", y, m, d);
		}
		if (strcmp(c_name, "db_host") == 0)
		{
			fscanf(fin, "%s", DB_host);
		}
		if (strcmp(c_name, "db_username") == 0)
		{
			fscanf(fin, "%s", DB_username);
		}
		if (strcmp(c_name, "db_password") == 0)
		{
			fscanf(fin, "%s", DB_password);
		}
		if (strcmp(c_name, "db_database") == 0)
		{
			fscanf(fin, "%s", DB_database);
		}
		if (strcmp(c_name, "db_timezone") == 0)
		{
			fscanf(fin, "%s", DB_timezone);
		}
	}

	fclose(fin);

	return 0;
}
