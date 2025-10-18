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

#include "bbs.h"
#include "bbs_main.h"
#include "common.h"
#include "database.h"
#include "file_loader.h"
#include "io.h"
#include "init.h"
#include "log.h"
#include "login.h"
#include "menu.h"
#include "net_server.h"
#include "section_list.h"
#include "section_list_loader.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <systemd/sd-daemon.h>

#define WAIT_CHILD_PROCESS_EXIT_TIMEOUT 5 // second
#define WAIT_CHILD_PROCESS_KILL_TIMEOUT 1 // second

struct process_sockaddr_t
{
	pid_t pid;
	in_addr_t s_addr;
};
typedef struct process_sockaddr_t PROCESS_SOCKADDR;

static PROCESS_SOCKADDR process_sockaddr_pool[MAX_CLIENT_LIMIT];

#define SSH_AUTH_MAX_DURATION (60 * 1000) // milliseconds

struct ssl_server_cb_data_t
{
	int tries;
	int error;
};

static int auth_password(ssh_session session, const char *user,
						 const char *password, void *userdata)
{
	struct ssl_server_cb_data_t *p_data = userdata;
	int ret;

	if (strcmp(user, "guest") == 0)
	{
		ret = load_guest_info();
	}
	else
	{
		ret = check_user(user, password);
	}

	if (ret == 0)
	{
		return SSH_AUTH_SUCCESS;
	}

	if ((++(p_data->tries)) >= BBS_login_retry_times)
	{
		p_data->error = 1;
	}

	return SSH_AUTH_DENIED;
}

static int pty_request(ssh_session session, ssh_channel channel, const char *term,
					   int x, int y, int px, int py, void *userdata)
{
	return 0;
}

static int shell_request(ssh_session session, ssh_channel channel, void *userdata)
{
	return 0;
}

static struct ssh_channel_callbacks_struct channel_cb = {
	.channel_pty_request_function = pty_request,
	.channel_shell_request_function = shell_request};

static ssh_channel new_session_channel(ssh_session session, void *userdata)
{
	if (SSH_channel != NULL)
	{
		return NULL;
	}

	SSH_channel = ssh_channel_new(session);
	ssh_callbacks_init(&channel_cb);
	ssh_set_channel_callbacks(SSH_channel, &channel_cb);

	return SSH_channel;
}

static int fork_server(void)
{
	ssh_event event;
	long int ssh_timeout = 0;
	int pid;
	int i;
	int ret;

	struct ssl_server_cb_data_t cb_data = {
		.tries = 0,
		.error = 0,
	};

	struct ssh_server_callbacks_struct cb = {
		.userdata = &cb_data,
		.auth_password_function = auth_password,
		.channel_open_request_session_function = new_session_channel,
	};

	pid = fork();

	if (pid > 0) // Parent process
	{
		SYS_child_process_count++;
		log_common("Child process (%d) start\n", pid);
		return pid;
	}
	else if (pid < 0) // Error
	{
		log_error("fork() error (%d)\n", errno);
		return -1;
	}

	// Child process

	if (close(socket_server[0]) == -1 || close(socket_server[1]) == -1)
	{
		log_error("Close server socket failed\n");
	}

	SSH_session = ssh_new();

	if (SSH_v2)
	{
		if (ssh_bind_accept_fd(sshbind, SSH_session, socket_client) != SSH_OK)
		{
			log_error("ssh_bind_accept_fd() error: %s\n", ssh_get_error(SSH_session));
			goto cleanup;
		}

		ssh_bind_free(sshbind);

		ssh_callbacks_init(&cb);
		ssh_set_server_callbacks(SSH_session, &cb);

		ssh_timeout = 60; // second
		if (ssh_options_set(SSH_session, SSH_OPTIONS_TIMEOUT, &ssh_timeout) < 0)
		{
			log_error("Error setting SSH options: %s\n", ssh_get_error(SSH_session));
			goto cleanup;
		}

		if (ssh_handle_key_exchange(SSH_session))
		{
			log_error("ssh_handle_key_exchange() error: %s\n", ssh_get_error(SSH_session));
			goto cleanup;
		}
		ssh_set_auth_methods(SSH_session, SSH_AUTH_METHOD_PASSWORD);

		event = ssh_event_new();
		ssh_event_add_session(event, SSH_session);

		for (i = 0; i < SSH_AUTH_MAX_DURATION && !SYS_server_exit && !cb_data.error && SSH_channel == NULL; i += 100)
		{
			ret = ssh_event_dopoll(event, 100); // 0.1 second
			if (ret == SSH_ERROR)
			{
#ifdef _DEBUG
				log_error("ssh_event_dopoll() error: %s\n", ssh_get_error(SSH_session));
#endif
				goto cleanup;
			}
		}

		if (cb_data.error)
		{
			log_error("SSH auth error, tried %d times\n", cb_data.tries);
			goto cleanup;
		}

		ssh_timeout = 0;
		if (ssh_options_set(SSH_session, SSH_OPTIONS_TIMEOUT, &ssh_timeout) < 0)
		{
			log_error("Error setting SSH options: %s\n", ssh_get_error(SSH_session));
			goto cleanup;
		}
	}

	// Redirect Input
	close(STDIN_FILENO);
	if (dup2(socket_client, STDIN_FILENO) == -1)
	{
		log_error("Redirect stdin to client socket failed\n");
		goto cleanup;
	}

	// Redirect Output
	close(STDOUT_FILENO);
	if (dup2(socket_client, STDOUT_FILENO) == -1)
	{
		log_error("Redirect stdout to client socket failed\n");
		goto cleanup;
	}

	SYS_child_process_count = 0;

	bbs_main();

cleanup:
	// Child process exit
	SYS_server_exit = 1;

	if (SSH_v2)
	{
		ssh_channel_free(SSH_channel);
		ssh_disconnect(SSH_session);
	}
	else if (close(socket_client) == -1)
	{
		log_error("Close client socket failed\n");
	}

	ssh_free(SSH_session);
	ssh_finalize();

	// Close Input and Output for client
	close(STDIN_FILENO);
	close(STDOUT_FILENO);

	log_common("Process exit normally\n");
	log_end();

	_exit(0);

	return 0;
}

int net_server(const char *hostaddr, in_port_t port[])
{
	unsigned int addrlen;
	int ret;
	int flags[2];
	struct sockaddr_in sin;
	struct epoll_event ev, events[MAX_EVENTS];
	int nfds, epollfd;
	siginfo_t siginfo;
	int notify_child_exit = 0;
	time_t tm_notify_child_exit = time(NULL);
	int sd_notify_stopping = 0;
	MENU_SET bbs_menu_new;
	MENU_SET top10_menu_new;
	int i, j;
	pid_t pid;
	int ssh_log_level = SSH_LOG_NOLOG;

	ssh_init();

	sshbind = ssh_bind_new();

	if (ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, hostaddr) < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &port) < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, SSH_HOST_KEYFILE) < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY_ALGORITHMS, "ssh-rsa,rsa-sha2-512,rsa-sha2-256") < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &ssh_log_level) < 0)
	{
		log_error("Error setting SSH bind options: %s\n", ssh_get_error(sshbind));
		ssh_bind_free(sshbind);
		return -1;
	}

	epollfd = epoll_create1(0);
	if (epollfd < 0)
	{
		log_error("epoll_create1() error (%d)\n", errno);
		return -1;
	}

	// Server socket
	for (i = 0; i < 2; i++)
	{
		socket_server[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if (socket_server[i] < 0)
		{
			log_error("Create socket_server error (%d)\n", errno);
			return -1;
		}

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = (hostaddr[0] != '\0' ? inet_addr(hostaddr) : INADDR_ANY);
		sin.sin_port = htons(port[i]);

		// Reuse address and port
		flags[i] = 1;
		if (setsockopt(socket_server[i], SOL_SOCKET, SO_REUSEADDR, &flags[i], sizeof(flags[i])) < 0)
		{
			log_error("setsockopt SO_REUSEADDR error (%d)\n", errno);
		}
		if (setsockopt(socket_server[i], SOL_SOCKET, SO_REUSEPORT, &flags[i], sizeof(flags[i])) < 0)
		{
			log_error("setsockopt SO_REUSEPORT error (%d)\n", errno);
		}

		if (bind(socket_server[i], (struct sockaddr *)&sin, sizeof(sin)) < 0)
		{
			log_error("Bind address %s:%u error (%d)\n",
					  inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), errno);
			return -1;
		}

		if (listen(socket_server[i], 10) < 0)
		{
			log_error("Telnet socket listen error (%d)\n", errno);
			return -1;
		}

		log_common("Listening at %s:%u\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

		ev.events = EPOLLIN;
		ev.data.fd = socket_server[i];
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_server[i], &ev) == -1)
		{
			log_error("epoll_ctl(socket_server[%d]) error (%d)\n", i, errno);
			if (close(epollfd) < 0)
			{
				log_error("close(epoll) error (%d)\n");
			}
			return -1;
		}

		flags[i] = fcntl(socket_server[i], F_GETFL, 0);
		fcntl(socket_server[i], F_SETFL, flags[i] | O_NONBLOCK);
	}

	// Startup complete
	sd_notifyf(0, "READY=1\n"
				  "STATUS=Listening at %s:%d (Telnet) and %s:%d (SSH2)\n"
				  "MAINPID=%d",
			   hostaddr, port[0], hostaddr, port[1], getpid());

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
				log_common("Child process (%d) exited\n", siginfo.si_pid);

				if (siginfo.si_pid != section_list_loader_pid)
				{
					i = 0;
					for (; i < BBS_max_client; i++)
					{
						if (process_sockaddr_pool[i].pid == siginfo.si_pid)
						{
							process_sockaddr_pool[i].pid = 0;
							break;
						}
					}
					if (i >= BBS_max_client)
					{
						log_error("Child process (%d) not found in process sockaddr pool\n", siginfo.si_pid);
					}
				}
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
			if (notify_child_exit == 0)
			{
				sd_notifyf(0, "STATUS=Notify %d child process to exit", SYS_child_process_count);
				log_common("Notify %d child process to exit\n", SYS_child_process_count);

				if (kill(0, SIGTERM) < 0)
				{
					log_error("Send SIGTERM signal failed (%d)\n", errno);
				}

				notify_child_exit = 1;
				tm_notify_child_exit = time(NULL);
			}
			else if (notify_child_exit == 1 && time(NULL) - tm_notify_child_exit >= WAIT_CHILD_PROCESS_EXIT_TIMEOUT)
			{
				sd_notifyf(0, "STATUS=Kill %d child process", SYS_child_process_count);

				for (i = 0; i < BBS_max_client; i++)
				{
					if (process_sockaddr_pool[i].pid != 0)
					{
						log_error("Kill child process (pid=%d)\n", process_sockaddr_pool[i].pid);
						if (kill(process_sockaddr_pool[i].pid, SIGKILL) < 0)
						{
							log_error("Send SIGKILL signal failed (%d)\n", errno);
						}
					}
				}

				notify_child_exit = 2;
				tm_notify_child_exit = time(NULL);
			}
			else if (notify_child_exit == 2 && time(NULL) - tm_notify_child_exit >= WAIT_CHILD_PROCESS_KILL_TIMEOUT)
			{
				log_error("Main process prepare to exit without waiting for %d child process any longer\n", SYS_child_process_count);
				SYS_child_process_count = 0;
			}
		}

		if (SYS_conf_reload && !SYS_server_exit)
		{
			SYS_conf_reload = 0;
			sd_notify(0, "RELOADING=1");

			// Reload configuration
			if (load_conf(CONF_BBSD) < 0)
			{
				log_error("Reload conf failed\n");
			}

			if (load_menu(&bbs_menu_new, CONF_MENU) < 0)
			{
				unload_menu(&bbs_menu_new);
				log_error("Reload bbs menu failed\n");
			}
			else
			{
				unload_menu(&bbs_menu);
				memcpy(&bbs_menu, &bbs_menu_new, sizeof(bbs_menu_new));
				log_common("Reload bbs menu successfully\n");
			}

			if (load_menu(&top10_menu_new, CONF_TOP10_MENU) < 0)
			{
				unload_menu(&top10_menu_new);
				log_error("Reload top10 menu failed\n");
			}
			else
			{
				unload_menu(&top10_menu);
				top10_menu_new.allow_exit = 1;
				memcpy(&top10_menu, &top10_menu_new, sizeof(top10_menu_new));
				log_common("Reload top10 menu successfully\n");
			}

			for (int i = 0; i < data_files_load_startup_count; i++)
			{
				if (load_file(data_files_load_startup[i]) < 0)
				{
					log_error("load_file_mmap(%s) error\n", data_files_load_startup[i]);
				}
			}
			log_common("Reload data files successfully\n");

			// Load section config and gen_ex
			if (load_section_config_from_db(1) < 0)
			{
				log_error("load_section_config_from_db(1) error\n");
			}
			else
			{
				log_common("Reload section config and gen_ex successfully\n");
			}

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
			if (events[i].data.fd == socket_server[0] || events[i].data.fd == socket_server[1])
			{
				SSH_v2 = (events[i].data.fd == socket_server[1] ? 1 : 0);

				while (!SYS_server_exit) // Accept all incoming connections until error
				{
					addrlen = sizeof(sin);
					socket_client = accept(socket_server[SSH_v2], (struct sockaddr *)&sin, &addrlen);
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

					log_common("Accept %s connection from %s:%d\n", (SSH_v2 ? "SSH" : "telnet"), hostaddr_client, port_client);

					if (SYS_child_process_count - 1 < BBS_max_client)
					{
						j = 0;
						for (i = 0; i < BBS_max_client; i++)
						{
							if (process_sockaddr_pool[i].pid != 0 && process_sockaddr_pool[i].s_addr == sin.sin_addr.s_addr)
							{
								j++;
								if (j >= BBS_max_client_per_ip)
								{
									log_common("Too many client connections (%d) from %s\n", j, hostaddr_client);
									break;
								}
							}
						}

						if (j < BBS_max_client_per_ip)
						{
							if ((pid = fork_server()) < 0)
							{
								log_error("fork_server() error\n");
							}
							else if (pid > 0)
							{
								i = 0;
								for (; i < BBS_max_client; i++)
								{
									if (process_sockaddr_pool[i].pid == 0)
									{
										break;
									}
								}

								if (i >= BBS_max_client)
								{
									log_error("Process sockaddr pool depleted\n");
								}
								else
								{
									process_sockaddr_pool[i].pid = pid;
									process_sockaddr_pool[i].s_addr = sin.sin_addr.s_addr;
								}
							}
						}
					}
					else
					{
						log_error("Rejected client connection over limit (%d)\n", SYS_child_process_count - 1);
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

	for (i = 0; i < 2; i++)
	{
		fcntl(socket_server[i], F_SETFL, flags[i]);

		if (close(socket_server[i]) == -1)
		{
			log_error("Close server socket failed\n");
		}
	}

	ssh_bind_free(sshbind);
	ssh_finalize();

	return 0;
}
