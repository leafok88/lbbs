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
#include <sys/epoll.h>
#include <arpa/inet.h>

int net_server(const char *hostaddr, in_port_t port)
{
	unsigned int namelen;
	int ret;
	int flags;
	struct sockaddr_in sin;
	struct epoll_event ev, events[MAX_EVENTS];
	int nfds, epollfd;
	sigset_t nsigset;
	sigset_t osigset;
	siginfo_t siginfo;

	socket_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (socket_server < 0)
	{
		log_error("Create socket failed\n");
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = (hostaddr[0] != '\0' ? inet_addr(hostaddr) : INADDR_ANY);
	sin.sin_port = htons(port);

	// Reuse address and port
	flags = 1;
	if (setsockopt(socket_server, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) < 0)
	{
		log_error("setsockopt SO_REUSEADDR error (%d)\n", errno);
	}
	if (setsockopt(socket_server, SOL_SOCKET, SO_REUSEPORT, &flags, sizeof(flags)) < 0)
	{
		log_error("setsockopt SO_REUSEPORT error (%d)\n", errno);
	}

	if (bind(socket_server, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		log_error("Bind address %s:%u failed (%d)\n",
				  inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), errno);
		return -1;
	}

	if (listen(socket_server, 10) < 0)
	{
		log_error("Socket listen failed (%d)\n", errno);
		return -1;
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

	epollfd = epoll_create1(0);
	if (epollfd < 0)
	{
		log_error("epoll_create1() error (%d)\n", errno);
		return -1;
	}

	ev.events = EPOLLIN;
	ev.data.fd = socket_server;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_server, &ev) == -1)
	{
		log_error("epoll_ctl(socket_server) error (%d)\n", errno);
		return -1;
	}

	flags = fcntl(socket_server, F_GETFL, 0);
	fcntl(socket_server, F_SETFL, flags | O_NONBLOCK);

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

		nfds = epoll_wait(epollfd, events, MAX_EVENTS, 100); // 0.1 second

		if (nfds < 0)
		{
			if (errno != EINTR)
			{
				log_error("epoll_wait() error (%d)\n", errno);
				break;
			}
			continue;
		}

		// Stop accept new connection on exit
		if (SYS_server_exit)
		{
			continue;
		}

		for (int i = 0; i < nfds; i++)
		{
			if (events[i].data.fd == socket_server)
			{
				while (!SYS_server_exit) // Accept all incoming connections until error
				{
					socket_client = accept(socket_server, (struct sockaddr *)&sin, &namelen);
					if (socket_client < 0)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
							break;
						}
						else if (errno == EINTR)
						{
							continue;
						}
						else
						{
							log_error("accept(socket_server) error (%d)\n", errno);
							break;
						}
					}

					strncpy(hostaddr_client, inet_ntoa(sin.sin_addr), sizeof(hostaddr_client) - 1);
					hostaddr_client[sizeof(hostaddr_client) - 1] = '\0';

					port_client = ntohs(sin.sin_port);

					log_std("Accept connection from %s:%d\n", hostaddr_client, port_client);

					if (fork_server() < 0)
					{
						log_error("fork_server() error\n");
					}

					if (close(socket_client) == -1)
					{
						log_error("close(socket_lient) error (%d)\n", errno);
					}
				}
			}
		}
	}

	fcntl(socket_server, F_SETFL, flags);

	if (close(socket_server) == -1)
	{
		log_error("Close server socket failed\n");
	}

	return 0;
}
