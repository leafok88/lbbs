/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * net_server
 *   - network server with SSH support
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bbs.h"
#include "bbs_main.h"
#include "bwf.h"
#include "common.h"
#include "database.h"
#include "file_loader.h"
#include "hash_dict.h"
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
#include <pty.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <arpa/inet.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#else
#include <poll.h>
#endif

#ifdef HAVE_SYSTEMD_SD_DAEMON_H
#include <systemd/sd-daemon.h>
#endif

enum _net_server_constant_t
{
	WAIT_CHILD_PROCESS_EXIT_TIMEOUT = 5, // second
	WAIT_CHILD_PROCESS_KILL_TIMEOUT = 1, // second

	SSH_AUTH_MAX_DURATION = 60 * 1000, // milliseconds
};

/* A userdata struct for session. */
struct session_data_struct
{
	int tries;
	int error;
};

/* A userdata struct for channel. */
struct channel_data_struct
{
	/* pid of the child process the channel will spawn. */
	pid_t pid;
	/* For PTY allocation */
	socket_t pty_master;
	socket_t pty_slave;
	/* For communication with the child process. */
	socket_t child_stdin;
	socket_t child_stdout;
	/* Only used for subsystem and exec requests. */
	socket_t child_stderr;
	/* Event which is used to poll the above descriptors. */
	ssh_event event;
	/* Terminal size struct. */
	struct winsize *winsize;
};

static int socket_server[2];
static int socket_client;

#ifdef HAVE_SYS_EPOLL_H
static int epollfd_server = -1;
#endif

static ssh_bind sshbind;

static HASH_DICT *hash_dict_pid_sockaddr = NULL;
static HASH_DICT *hash_dict_sockaddr_count = NULL;

static const char SFTP_SERVER_PATH[] = "/usr/lib/sftp-server";

static int auth_password(ssh_session session, const char *user,
						 const char *password, void *userdata)
{
	struct session_data_struct *sdata = (struct session_data_struct *)userdata;
	int ret;

	if (strcmp(user, "guest") == 0)
	{
		ret = load_guest_info();
	}
	else
	{
		ret = check_user(user, password);
		if (ret == 2) // Enforce update user agreement
		{
			BBS_update_eula = 1;
			ret = 0;
		}
	}

	if (ret == 0)
	{
		return SSH_AUTH_SUCCESS;
	}

	if ((++(sdata->tries)) >= BBS_login_retry_times)
	{
		sdata->error = 1;
	}

	return SSH_AUTH_DENIED;
}

static int pty_request(ssh_session session, ssh_channel channel, const char *term,
					   int cols, int rows, int px, int py, void *userdata)
{
	struct channel_data_struct *cdata = (struct channel_data_struct *)userdata;
	int rc;

	(void)session;
	(void)channel;
	(void)term;

	cdata->winsize->ws_row = (unsigned short int)rows;
	cdata->winsize->ws_col = (unsigned short int)cols;
	cdata->winsize->ws_xpixel = (unsigned short int)px;
	cdata->winsize->ws_ypixel = (unsigned short int)py;

	rc = openpty(&cdata->pty_master, &cdata->pty_slave, NULL, NULL, cdata->winsize);
	if (rc != 0)
	{
		log_error("Failed to open pty\n");
		return SSH_ERROR;
	}

	return SSH_OK;
}

static int pty_resize(ssh_session session, ssh_channel channel, int cols, int rows,
					  int py, int px, void *userdata)
{
	struct channel_data_struct *cdata = (struct channel_data_struct *)userdata;

	(void)session;
	(void)channel;

	cdata->winsize->ws_row = (unsigned short int)rows;
	cdata->winsize->ws_col = (unsigned short int)cols;
	cdata->winsize->ws_xpixel = (unsigned short int)px;
	cdata->winsize->ws_ypixel = (unsigned short int)py;

	if (cdata->pty_master != -1)
	{
		return ioctl(cdata->pty_master, TIOCSWINSZ, cdata->winsize);
	}

	return SSH_ERROR;
}

static int exec_pty(const char *mode, const char *command, struct channel_data_struct *cdata)
{
	(void)cdata;

	if (command != NULL)
	{
		log_error("Forbid exec /bin/sh %s %s)\n", mode, command);
	}

	return SSH_OK;
}

static int exec_nopty(const char *command, struct channel_data_struct *cdata)
{
	(void)cdata;

	if (command != NULL)
	{
		log_error("Forbid exec /bin/sh -c %s)\n", command);
	}

	return SSH_OK;
}

static int exec_request(ssh_session session, ssh_channel channel, const char *command, void *userdata)
{
	struct channel_data_struct *cdata = (struct channel_data_struct *)userdata;

	(void)session;
	(void)channel;

	if (cdata->pid > 0)
	{
		return SSH_ERROR;
	}

	if (cdata->pty_master != -1 && cdata->pty_slave != -1)
	{
		return exec_pty("-c", command, cdata);
	}
	return exec_nopty(command, cdata);
}

static int shell_request(ssh_session session, ssh_channel channel, void *userdata)
{
	struct channel_data_struct *cdata = (struct channel_data_struct *)userdata;

	(void)session;
	(void)channel;

	if (cdata->pid > 0)
	{
		return SSH_ERROR;
	}

	if (cdata->pty_master != -1 && cdata->pty_slave != -1)
	{
		return exec_pty("-l", NULL, cdata);
	}
	/* Client requested a shell without a pty, let's pretend we allow that */
	return SSH_OK;
}

static int subsystem_request(ssh_session session, ssh_channel channel, const char *subsystem, void *userdata)
{
	(void)session;
	(void)channel;

	log_error("subsystem_request(subsystem=%s)\n", subsystem);

	/* subsystem requests behave similarly to exec requests. */
	if (strcmp(subsystem, "sftp") == 0)
	{
		return exec_request(session, channel, SFTP_SERVER_PATH, userdata);
	}
	return SSH_ERROR;
}

static ssh_channel channel_open(ssh_session session, void *userdata)
{
	(void)userdata;

	if (SSH_channel != NULL)
	{
		return NULL;
	}

	SSH_channel = ssh_channel_new(session);

	return SSH_channel;
}

static int fork_server(void)
{
	ssh_event event;
	long int ssh_timeout = 0;
	int pid;
	int i;
	int ret;

	/* Structure for storing the pty size. */
	struct winsize wsize = {
		.ws_row = 0,
		.ws_col = 0,
		.ws_xpixel = 0,
		.ws_ypixel = 0};

	/* Our struct holding information about the channel. */
	struct channel_data_struct cdata = {
		.pid = 0,
		.pty_master = -1,
		.pty_slave = -1,
		.child_stdin = -1,
		.child_stdout = -1,
		.child_stderr = -1,
		.event = NULL,
		.winsize = &wsize};

	struct session_data_struct cb_data = {
		.tries = 0,
		.error = 0,
	};

	struct ssh_channel_callbacks_struct channel_cb = {
		.userdata = &cdata,
		.channel_pty_request_function = pty_request,
		.channel_pty_window_change_function = pty_resize,
		.channel_shell_request_function = shell_request,
		.channel_exec_request_function = exec_request,
		.channel_subsystem_request_function = subsystem_request};

	struct ssh_server_callbacks_struct server_cb = {
		.userdata = &cb_data,
		.auth_password_function = auth_password,
		.channel_open_request_session_function = channel_open,
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
#ifdef HAVE_SYS_EPOLL_H
	if (close(epollfd_server) < 0)
	{
		log_error("close(epollfd_server) error (%d)\n");
	}
#endif

	for (i = 0; i < 2; i++)
	{
		if (close(socket_server[i]) == -1)
		{
			log_error("Close server socket failed\n");
		}
	}

	hash_dict_destroy(hash_dict_pid_sockaddr);
	hash_dict_destroy(hash_dict_sockaddr_count);

	SSH_session = ssh_new();

	if (SSH_v2)
	{
		if (ssh_bind_accept_fd(sshbind, SSH_session, socket_client) != SSH_OK)
		{
			log_error("ssh_bind_accept_fd() error: %s\n", ssh_get_error(SSH_session));
			goto cleanup;
		}

		ssh_bind_free(sshbind);

		ssh_timeout = 60; // second
		if (ssh_options_set(SSH_session, SSH_OPTIONS_TIMEOUT, &ssh_timeout) < 0)
		{
			log_error("Error setting SSH options: %s\n", ssh_get_error(SSH_session));
			goto cleanup;
		}

		ssh_set_auth_methods(SSH_session, SSH_AUTH_METHOD_PASSWORD);

		ssh_callbacks_init(&server_cb);
		ssh_callbacks_init(&channel_cb);

		ssh_set_server_callbacks(SSH_session, &server_cb);

		if (ssh_handle_key_exchange(SSH_session))
		{
			log_error("ssh_handle_key_exchange() error: %s\n", ssh_get_error(SSH_session));
			goto cleanup;
		}

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

		ssh_set_channel_callbacks(SSH_channel, &channel_cb);

		do
		{
			ret = ssh_event_dopoll(event, 100); // 0.1 second
			if (ret == SSH_ERROR)
			{
				ssh_channel_close(SSH_channel);
			}

			if (ret == SSH_AGAIN) // loop until SSH connection is fully established
			{
				/* Executed only once, once the child process starts. */
				cdata.event = event;
				break;
			}
		} while (ssh_channel_is_open(SSH_channel));

		ssh_timeout = 0;
		if (ssh_options_set(SSH_session, SSH_OPTIONS_TIMEOUT, &ssh_timeout) < 0)
		{
			log_error("Error setting SSH options: %s\n", ssh_get_error(SSH_session));
			goto cleanup;
		}

		ssh_set_blocking(SSH_session, 0);
	}

	// Redirect Input
	if (dup2(socket_client, STDIN_FILENO) == -1)
	{
		log_error("Redirect stdin to client socket failed\n");
		goto cleanup;
	}

	// Redirect Output
	if (dup2(socket_client, STDOUT_FILENO) == -1)
	{
		log_error("Redirect stdout to client socket failed\n");
		goto cleanup;
	}

	if (io_init() < 0)
	{
		log_error("io_init() error\n");
		goto cleanup;
	}

	SYS_child_process_count = 0;

	// BWF compile
	if (bwf_compile() < 0)
	{
		log_error("bwf_compile() error\n");
		goto cleanup;
	}

	bbs_main();

cleanup:
	// Child process exit
	SYS_server_exit = 1;

	if (SSH_v2)
	{
		if (cdata.pty_master != -1)
		{
			close(cdata.pty_master);
		}
		if (cdata.child_stdin != -1)
		{
			close(cdata.child_stdin);
		}
		if (cdata.child_stdout != -1)
		{
			close(cdata.child_stdout);
		}
		if (cdata.child_stderr != -1)
		{
			close(cdata.child_stderr);
		}

		ssh_channel_free(SSH_channel);
		ssh_disconnect(SSH_session);
	}
	else if (close(socket_client) == -1)
	{
		log_error("Close client socket failed\n");
	}

	ssh_free(SSH_session);
	ssh_finalize();

	// BWF cleanup
	bwf_cleanup();

	// Close Input and Output for client
	io_cleanup();
	close(STDIN_FILENO);
	close(STDOUT_FILENO);

	log_common("Process exit normally\n");
	log_end();

	_exit(0);

	return 0;
}

int net_server(const char *hostaddr, in_port_t port[])
{
	struct stat file_stat;
	unsigned int addrlen;
	int ret;
	int flags_server[2];
	struct sockaddr_in sin;

#ifdef HAVE_SYS_EPOLL_H
	struct epoll_event ev, events[MAX_EVENTS];
#else
	struct pollfd pfds[2];
#endif

	int nfds;
	int notify_child_exit = 0;
	time_t tm_notify_child_exit = time(NULL);
	int i, j;
	pid_t pid;
	int ssh_key_valid = 0;
	int ssh_log_level = SSH_LOG_NOLOG;

#ifdef HAVE_SYSTEMD_SD_DAEMON_H
	int sd_notify_stopping = 0;
#endif

	ssh_init();

	sshbind = ssh_bind_new();

	if (ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, SSH_HOST_RSA_KEY_FILE) < 0)
	{
		log_error("Error loading SSH RSA key: %s\n", SSH_HOST_RSA_KEY_FILE);
	}
	else
	{
		ssh_key_valid = 1;
	}
	if (ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, SSH_HOST_ED25519_KEY_FILE) < 0)
	{
		log_error("Error loading SSH ED25519 key: %s\n", SSH_HOST_ED25519_KEY_FILE);
	}
	else
	{
		ssh_key_valid = 1;
	}
	if (ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, SSH_HOST_ECDSA_KEY_FILE) < 0)
	{
		log_error("Error loading SSH ECDSA key: %s\n", SSH_HOST_ECDSA_KEY_FILE);
	}
	else
	{
		ssh_key_valid = 1;
	}

	if (!ssh_key_valid)
	{
		log_error("Error: no valid SSH host key\n");
		ssh_bind_free(sshbind);
		return -1;
	}

	if (ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, hostaddr) < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &port) < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY_ALGORITHMS, "+ssh-rsa") < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &ssh_log_level) < 0)
	{
		log_error("Error setting SSH bind options: %s\n", ssh_get_error(sshbind));
		ssh_bind_free(sshbind);
		return -1;
	}

#ifdef HAVE_SYS_EPOLL_H
	epollfd_server = epoll_create1(0);
	if (epollfd_server == -1)
	{
		log_error("epoll_create1() error (%d)\n", errno);
		return -1;
	}
#endif

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
		flags_server[i] = 1;
		if (setsockopt(socket_server[i], SOL_SOCKET, SO_REUSEADDR, &flags_server[i], sizeof(flags_server[i])) < 0)
		{
			log_error("setsockopt SO_REUSEADDR error (%d)\n", errno);
		}
#if defined(SO_REUSEPORT)
		if (setsockopt(socket_server[i], SOL_SOCKET, SO_REUSEPORT, &flags_server[i], sizeof(flags_server[i])) < 0)
		{
			log_error("setsockopt SO_REUSEPORT error (%d)\n", errno);
		}
#endif

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

#ifdef HAVE_SYS_EPOLL_H
		ev.events = EPOLLIN;
		ev.data.fd = socket_server[i];
		if (epoll_ctl(epollfd_server, EPOLL_CTL_ADD, socket_server[i], &ev) == -1)
		{
			log_error("epoll_ctl(socket_server[%d]) error (%d)\n", i, errno);
			if (close(epollfd_server) < 0)
			{
				log_error("close(epoll) error (%d)\n");
			}
			return -1;
		}
#endif

		flags_server[i] = fcntl(socket_server[i], F_GETFL, 0);
		fcntl(socket_server[i], F_SETFL, flags_server[i] | O_NONBLOCK);
	}

	ssh_bind_set_blocking(sshbind, 0);

	hash_dict_pid_sockaddr = hash_dict_create(MAX_CLIENT_LIMIT);
	if (hash_dict_pid_sockaddr == NULL)
	{
		log_error("hash_dict_create(hash_dict_pid_sockaddr) error\n");
		return -1;
	}
	hash_dict_sockaddr_count = hash_dict_create(MAX_CLIENT_LIMIT);
	if (hash_dict_sockaddr_count == NULL)
	{
		log_error("hash_dict_create(hash_dict_sockaddr_count) error\n");
		return -1;
	}

	// Startup complete
#ifdef HAVE_SYSTEMD_SD_DAEMON_H
	sd_notifyf(0, "READY=1\n"
				  "STATUS=Listening at %s:%d (Telnet) and %s:%d (SSH2)\n"
				  "MAINPID=%d",
			   hostaddr, port[0], hostaddr, port[1], getpid());
#endif

	while (!SYS_server_exit || SYS_child_process_count > 0)
	{
#ifdef HAVE_SYSTEMD_SD_DAEMON_H
		if (SYS_server_exit && !sd_notify_stopping)
		{
			sd_notify(0, "STOPPING=1");
			sd_notify_stopping = 1;
		}
#endif

		while ((SYS_child_exit || SYS_server_exit) && SYS_child_process_count > 0)
		{
			SYS_child_exit = 0;

			pid = waitpid(-1, &ret, WNOHANG);
			if (pid > 0)
			{
				SYS_child_exit = 1; // Retry waitid
				SYS_child_process_count--;

				if (WIFEXITED(ret))
				{
					log_common("Child process (%d) exited, status=%d\n", pid, WEXITSTATUS(ret));
				}
				else if (WIFSIGNALED(ret))
				{
					log_common("Child process (%d) is killed, status=%d\n", pid, WTERMSIG(ret));
				}
				else
				{
					log_common("Child process (%d) exited abnormally, status=%d\n", pid, ret);
				}

				if (pid != section_list_loader_pid)
				{
					j = 0;
					ret = hash_dict_get(hash_dict_pid_sockaddr, (uint64_t)pid, (int64_t *)&j);
					if (ret < 0)
					{
						log_error("hash_dict_get(hash_dict_pid_sockaddr, %d) error\n", pid);
					}
					else
					{
						ret = hash_dict_inc(hash_dict_sockaddr_count, (uint64_t)j, -1);
						if (ret < 0)
						{
							log_error("hash_dict_inc(hash_dict_sockaddr_count, %d, -1) error\n", j);
						}

						ret = hash_dict_del(hash_dict_pid_sockaddr, (uint64_t)pid);
						if (ret < 0)
						{
							log_error("hash_dict_del(hash_dict_pid_sockaddr, %d) error\n", pid);
						}
					}
				}
			}
			else if (pid == 0)
			{
				break;
			}
			else if (pid < 0)
			{
				log_error("Error in waitpid(): %d\n", errno);
				break;
			}
		}

		if (SYS_server_exit && !SYS_child_exit && SYS_child_process_count > 0)
		{
			if (notify_child_exit == 0)
			{
#ifdef HAVE_SYSTEMD_SD_DAEMON_H
				sd_notifyf(0, "STATUS=Notify %d child process to exit", SYS_child_process_count);
				log_common("Notify %d child process to exit\n", SYS_child_process_count);
#endif

				if (kill(0, SIGTERM) < 0)
				{
					log_error("Send SIGTERM signal failed (%d)\n", errno);
				}

				notify_child_exit = 1;
				tm_notify_child_exit = time(NULL);
			}
			else if (notify_child_exit == 1 && time(NULL) - tm_notify_child_exit >= WAIT_CHILD_PROCESS_EXIT_TIMEOUT)
			{
#ifdef HAVE_SYSTEMD_SD_DAEMON_H
				sd_notifyf(0, "STATUS=Kill %d child process", SYS_child_process_count);
#endif

				if (kill(0, SIGKILL) < 0)
				{
					log_error("Send SIGKILL signal failed (%d)\n", errno);
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

#ifdef HAVE_SYSTEMD_SD_DAEMON_H
			sd_notify(0, "RELOADING=1");
#endif

			// Restart log
			if (log_restart() < 0)
			{
				log_error("Restart logging failed\n");
			}

			// Reload configuration
			if (load_conf(CONF_BBSD) < 0)
			{
				log_error("Reload conf failed\n");
			}

			// Reload BWF config
			if (bwf_load(CONF_BWF) < 0)
			{
				log_error("Reload BWF conf failed\n");
			}

			// Get EULA modification tm
			if (stat(DATA_EULA, &file_stat) == -1)
			{
				log_error("stat(%s) error\n", DATA_EULA, errno);
			}
			else
			{
				BBS_eula_tm = file_stat.st_mtim.tv_sec;
			}

			if (detach_menu_shm(&bbs_menu) < 0)
			{
				log_error("detach_menu_shm(bbs_menu) error\n");
			}
			if (load_menu(&bbs_menu, CONF_MENU) < 0)
			{
				log_error("load_menu(bbs_menu) error\n");
				unload_menu(&bbs_menu);
			}

			if (detach_menu_shm(&top10_menu) < 0)
			{
				log_error("detach_menu_shm(top10_menu) error\n");
			}
			if (load_menu(&top10_menu, CONF_TOP10_MENU) < 0)
			{
				log_error("load_menu(top10_menu) error\n");
				unload_menu(&top10_menu);
			}
			top10_menu.allow_exit = 1;

			for (int i = 0; i < data_files_load_startup_count; i++)
			{
				if (load_file(data_files_load_startup[i]) < 0)
				{
					log_error("load_file(%s) error\n", data_files_load_startup[i]);
				}
			}

			// Load section config and gen_ex
			if (load_section_config_from_db(1) < 0)
			{
				log_error("load_section_config_from_db(1) error\n");
			}

			// Notify child processes to reload configuration
			if (kill(0, SIGUSR1) < 0)
			{
				log_error("Send SIGUSR1 signal failed (%d)\n", errno);
			}

#ifdef HAVE_SYSTEMD_SD_DAEMON_H
			sd_notify(0, "READY=1");
#endif
		}

#ifdef HAVE_SYS_EPOLL_H
		nfds = epoll_wait(epollfd_server, events, MAX_EVENTS, 100); // 0.1 second
		ret = nfds;
#else
		pfds[0].fd = socket_server[0];
		pfds[0].events = POLLIN;
		pfds[1].fd = socket_server[1];
		pfds[1].events = POLLIN;
		nfds = 2;
		ret = poll(pfds, (nfds_t)nfds, 100); // 0.1 second
#endif
		if (ret < 0)
		{
			if (errno != EINTR)
			{
#ifdef HAVE_SYS_EPOLL_H
				log_error("epoll_wait() error (%d)\n", errno);
#else
				log_error("poll() error (%d)\n", errno);
#endif
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
#ifdef HAVE_SYS_EPOLL_H
			if (events[i].data.fd == socket_server[0] || events[i].data.fd == socket_server[1])
#else
			if ((pfds[i].fd == socket_server[0] || pfds[i].fd == socket_server[1]) && (pfds[i].revents & POLLIN))
#endif
			{
#ifdef HAVE_SYS_EPOLL_H
				SSH_v2 = (events[i].data.fd == socket_server[1] ? 1 : 0);
#else
				SSH_v2 = (pfds[i].fd == socket_server[1] ? 1 : 0);
#endif

				while (!SYS_server_exit) // Accept all incoming connections until error
				{
					addrlen = sizeof(sin);
					socket_client = accept(socket_server[SSH_v2], (struct sockaddr *)&sin, (socklen_t *)&addrlen);
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

					if (SYS_child_process_count - 1 < BBS_max_client)
					{
						j = 0;
						ret = hash_dict_get(hash_dict_sockaddr_count, (uint64_t)sin.sin_addr.s_addr, (int64_t *)&j);
						if (ret < 0)
						{
							log_error("hash_dict_get(hash_dict_sockaddr_count, %s) error\n", hostaddr_client);
						}

						if (j < BBS_max_client_per_ip)
						{
							log_common("Accept %s connection from %s:%d, already have %d connections\n",
									   (SSH_v2 ? "SSH" : "telnet"), hostaddr_client, port_client, j);

							if ((pid = fork_server()) < 0)
							{
								log_error("fork_server() error\n");
							}
							else if (pid > 0)
							{
								ret = hash_dict_set(hash_dict_pid_sockaddr, (uint64_t)pid, sin.sin_addr.s_addr);
								if (ret < 0)
								{
									log_error("hash_dict_set(hash_dict_pid_sockaddr, %d, %s) error\n", pid, hostaddr_client);
								}

								ret = hash_dict_inc(hash_dict_sockaddr_count, (uint64_t)sin.sin_addr.s_addr, 1);
								if (ret < 0)
								{
									log_error("hash_dict_inc(hash_dict_sockaddr_count, %s, 1) error\n", hostaddr_client);
								}
							}
						}
						else
						{
							log_error("Rejected %s connection from %s:%d over limit per IP (%d)\n",
									  (SSH_v2 ? "SSH" : "telnet"), hostaddr_client, port_client, BBS_max_client_per_ip);
						}
					}
					else
					{
						log_error("Rejected %s connection from %s:%d over limit (%d)\n",
								  (SSH_v2 ? "SSH" : "telnet"), hostaddr_client, port_client, SYS_child_process_count - 1);
					}

					if (close(socket_client) == -1)
					{
						log_error("close(socket_lient) error (%d)\n", errno);
					}
				}
			}
		}
	}

#ifdef HAVE_SYS_EPOLL_H
	if (close(epollfd_server) < 0)
	{
		log_error("close(epollfd_server) error (%d)\n");
	}
#endif

	for (i = 0; i < 2; i++)
	{
		if (close(socket_server[i]) == -1)
		{
			log_error("Close server socket failed\n");
		}
	}

	hash_dict_destroy(hash_dict_pid_sockaddr);
	hash_dict_destroy(hash_dict_sockaddr_count);

	ssh_bind_free(sshbind);
	ssh_finalize();

	return 0;
}
