/***************************************************************************
						  fork.c  -  description
							 -------------------
	begin                : Mon Oct 18 2004
	copyright            : (C) 2004 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "common.h"
#include "bbs_main.h"
#include "log.h"
#include "io.h"
#include "fork.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int fork_server()
{
	int pid;

	if (pid = fork())
	{
		SYS_child_process_count++;
		log_std("Child process (%d) start\n", pid);
		return 0;
	}
	else if (pid < 0)
		return -1;

	if (close(socket_server) == -1)
	{
		log_error("Close server socket failed\n");
		return -2;
	}

	// Redirect Input
	close(0);
	if (dup2(socket_client, 0) == -1)
	{
		log_error("Redirect stdin to client socket failed\n");
		return -3;
	}

	// Redirect Output
	close(1);
	if (dup2(socket_client, 1) == -1)
	{
		log_error("Redirect stdout to client socket failed\n");
		return -4;
	}

	bbs_main();

	if (close(socket_client) == -1)
	{
		log_error("Close client socket failed\n");
	}

	// Close Input and Output for client
	close(0);
	close(1);

	log_std("Process exit normally\n");

	log_end();

	// Exit child process normally
	exit(0);

	return 0;
}
