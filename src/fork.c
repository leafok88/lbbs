/***************************************************************************
						  fork.c  -  description
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
#include "bbs_main.h"
#include "log.h"
#include "io.h"
#include "fork.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

int fork_server()
{
	int pid;

	pid = fork();

	if (pid > 0) // Parent process
	{
		SYS_child_process_count++;
		log_std("Child process (%d) start\n", pid);
		return 0;
	}
	else if (pid < 0) // Error
	{
		log_error("fork() error (%d)\n", errno);
		return -1;
	}

	// Child process
	if (close(socket_server) == -1)
	{
		log_error("Close server socket failed\n");
		return -2;
	}

	// Redirect Input
	close(STDIN_FILENO);
	if (dup2(socket_client, STDIN_FILENO) == -1)
	{
		log_error("Redirect stdin to client socket failed\n");
		return -3;
	}

	// Redirect Output
	close(STDOUT_FILENO);
	if (dup2(socket_client, STDOUT_FILENO) == -1)
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
	close(STDIN_FILENO);
	close(STDOUT_FILENO);

	log_std("Process exit normally\n");

	log_end();

	// Exit child process normally
	exit(0);

	return 0;
}
