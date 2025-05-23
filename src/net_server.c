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
#include "file_loader.h"
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
#include <systemd/sd-daemon.h>

int net_server(const char *hostaddr, in_port_t port)
{
	unsigned int namelen;
	int ret;
	int flags;
	struct sockaddr_in sin;
	struct epoll_event ev, events[MAX_EVENTS];
	int nfds, epollfd;
	siginfo_t siginfo;
	int sd_notify_stopping = 0;
	MENU_SET *p_bbs_menu_new;

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
		if (close(epollfd) < 0)
		{
			log_error("close(epoll) error (%d)\n");
		}
		return -1;
	}

	flags = fcntl(socket_server, F_GETFL, 0);
	fcntl(socket_server, F_SETFL, flags | O_NONBLOCK);

	// Startup complete
	sd_notifyf(0, "READY=1\n"
				  "STATUS=Listening at %s:%d\n"
				  "MAINPID=%d",
			   hostaddr_server, port_server, getpid());

	while (!SYS_server_exit || SYS_child_process_count > 0)
	{
		if (SYS_server_exit && !sd_notify_stopping)
		{
			sd_notify(0, "STOPPING=1");
			sd_notify_stopping = 1;
		}

		while ((SYS_child_exit || SYS_server_exit) && SYS_child_process_count > 0)
		{
			SYS_child_exit = 0;

			siginfo.si_pid = 0;
			ret = waitid(P_ALL, 0, &siginfo, WEXITED | WNOHANG);
			if (ret == 0 && siginfo.si_pid > 0)
			{
				SYS_child_exit = 1; // Retry waitid

				SYS_child_process_count--;
				log_std("Child process (%d) exited\n", siginfo.si_pid);
			}
			else if (ret == 0)
			{
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

			sd_notifyf(0, "STATUS=Waiting for %d child process to exit", SYS_child_process_count);
		}

		if (SYS_menu_reload && !SYS_server_exit)
		{
			SYS_menu_reload = 0;
			sd_notify(0, "RELOADING=1");

			p_bbs_menu_new = calloc(1, sizeof(MENU_SET));
			if (p_bbs_menu_new == NULL)
			{
				log_error("OOM: calloc(MENU_SET)\n");
			}
			else if (load_menu(p_bbs_menu_new, CONF_MENU) < 0)
			{
				unload_menu(p_bbs_menu_new);
				free(p_bbs_menu_new);
				p_bbs_menu_new = NULL;

				log_error("Reload menu failed\n");
			}
			else
			{
				unload_menu(p_bbs_menu);
				free(p_bbs_menu);

				p_bbs_menu = p_bbs_menu_new;
				p_bbs_menu_new = NULL;

				log_std("Reload menu successfully\n");
			}

			sd_notify(0, "READY=1");
		}

		if (SYS_data_file_reload && !SYS_server_exit)
		{
			SYS_data_file_reload = 0;
			sd_notify(0, "RELOADING=1");

			for (int i = 0; i < data_files_load_startup_count; i++)
			{
				if (load_file_shm(data_files_load_startup[i]) < 0)
				{
					log_error("load_file_mmap(%s) error\n", data_files_load_startup[i]);
				}
			}

			log_std("Reload data files successfully\n");
			sd_notify(0, "READY=1");
		}

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

	if (close(epollfd) < 0)
	{
		log_error("close(epoll) error (%d)\n");
	}

	fcntl(socket_server, F_SETFL, flags);

	if (close(socket_server) == -1)
	{
		log_error("Close server socket failed\n");
	}

	return 0;
}
