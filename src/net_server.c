/***************************************************************************
						  net_server.c  -  description
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

#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "net_server.h"
#include "common.h"
#include "log.h"
#include "io.h"
#include "fork.h"
#include "menu.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>

int net_server(const char *hostaddr, in_port_t port)
{
	unsigned int namelen;
	int ret;
	int flags;
	struct sockaddr_in sin;
	fd_set testfds;
	struct timeval timeout;
	sigset_t nsigset;
	sigset_t osigset;
	siginfo_t siginfo;

	socket_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (socket_server < 0)
	{
		log_error("Create socket failed\n");
		exit(1);
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr =
		(strnlen(hostaddr, sizeof(hostaddr)) > 0 ? inet_addr(hostaddr) : INADDR_ANY);
	sin.sin_port = htons(port);

	if (bind(socket_server, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		log_error("Bind address %s:%u failed\n",
				  inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		exit(2);
	}

	if (listen(socket_server, 10) < 0)
	{
		log_error("Socket listen failed\n");
		exit(3);
	}

	strncpy(hostaddr_server, inet_ntoa(sin.sin_addr), sizeof(hostaddr_server) - 1);
	hostaddr_server[sizeof(hostaddr_server) - 1] = '\0';

	port_server = ntohs(sin.sin_port);
	namelen = sizeof(sin);

	log_std("Listening at %s:%d\n", hostaddr_server, port_server);

	sigemptyset(&nsigset);
	sigaddset(&nsigset, SIGHUP);
	sigaddset(&nsigset, SIGCHLD);
	sigaddset(&nsigset, SIGTERM);

	while (!SYS_server_exit || SYS_child_process_count > 0)
	{
		sigprocmask(SIG_BLOCK, &nsigset, &osigset);

		while ((SYS_child_exit || SYS_server_exit) && SYS_child_process_count > 0)
		{
			siginfo.si_pid = 0;
			ret = waitid(P_ALL, 0, &siginfo, WEXITED | WNOHANG);
			if (ret == 0 && siginfo.si_pid > 0)
			{
				SYS_child_process_count--;
				log_std("Child process (%d) exited\n", siginfo.si_pid);
			}
			else if (ret == 0)
			{
				SYS_child_exit = 0;
				break;
			}
			else if (ret < 0)
			{
				log_error("Error in waitid: %d\n", errno);
				break;
			}
		}

		if (SYS_server_exit && !SYS_child_exit && SYS_child_process_count > 0)
		{
			log_std("Notify %d child process to exit\n", SYS_child_process_count);
			if (kill(0, SIGTERM) < 0)
			{
				log_error("Send SIGTERM signal failed (%d)\n", errno);
			}
		}

		if (SYS_menu_reload && !SYS_server_exit)
		{
			if (reload_menu(&bbs_menu) < 0)
			{
				log_error("Reload menu failed\n");
			}
			else
			{
				log_std("Reload menu successfully\n");
			}
			SYS_menu_reload = 0;
		}

		sigprocmask(SIG_SETMASK, &osigset, NULL);

		FD_ZERO(&testfds);
		FD_SET(socket_server, &testfds);

		timeout.tv_sec = 0;
		timeout.tv_usec = 100 * 1000; // 0.1 second

		ret = select(FD_SETSIZE, &testfds, NULL, NULL, &timeout);

		if (ret < 0)
		{
			if (errno != EINTR)
			{
				log_error("Accept connection error: %d\n", errno);
			}
			continue;
		}
		else if (ret == 0) // timeout
		{
			continue;
		}

		// Stop accept new connection on exit
		if (SYS_server_exit)
		{
			continue;
		}

		if (FD_ISSET(socket_server, &testfds))
		{
			flags = fcntl(socket_server, F_GETFL, 0);
			fcntl(socket_server, F_SETFL, flags | O_NONBLOCK);
			while ((socket_client =
						accept(socket_server, (struct sockaddr *)&sin, &namelen)) < 0)
			{
				if (errno != EWOULDBLOCK && errno != ECONNABORTED && errno != EINTR)
				{
					log_error("Accept connection error\n");
					break;
				}
			}
			fcntl(socket_server, F_SETFL, flags);
		}

		if (socket_client < 0)
		{
			log_error("Accept connection error\n");
			continue;
		}

		strncpy(hostaddr_client, inet_ntoa(sin.sin_addr), sizeof(hostaddr_client) - 1);
		hostaddr_client[sizeof(hostaddr_client) - 1] = '\0';

		port_client = ntohs(sin.sin_port);

		log_std("Accept connection from %s:%d\n", hostaddr_client,
				port_client);

		if (fork_server() < 0)
		{
			log_error("Fork error\n");
		}

		if (close(socket_client) == -1)
		{
			log_error("Close client socket failed\n");
		}
	}

	if (close(socket_server) == -1)
	{
		log_error("Close server socket failed\n");
	}

	return 0;
}
