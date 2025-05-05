/***************************************************************************
						  net_server.c  -  description
							 -------------------
	begin                : Mon Oct 11 2004
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

#include "net_server.h"
#include "common.h"
#include "log.h"
#include "io.h"
#include "fork.h"
#include "tcplib.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int net_server(const char *hostaddr, unsigned int port)
{
	unsigned int namelen;
	int result;
	int flags;
	struct sockaddr_in sin;
	fd_set testfds;
	struct timeval timeout;

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

	strcpy(hostaddr_server, inet_ntoa(sin.sin_addr));
	port_server = ntohs(sin.sin_port);

	log_std("Listening at %s:%d\n", hostaddr_server, port_server);

	namelen = sizeof(sin);
	while (!SYS_exit)
	{
		FD_ZERO(&testfds);
		FD_SET(socket_server, &testfds);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		result = SignalSafeSelect(FD_SETSIZE, &testfds, NULL, NULL, &timeout);
		if (result < 0)
		{
			log_error("Accept connection error\n");
			continue;
		}

		if (result == 0)
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

		strcpy(hostaddr_client, (const char *)inet_ntoa(sin.sin_addr));
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
