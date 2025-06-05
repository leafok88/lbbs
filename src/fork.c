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
#include "bbs.h"
#include "log.h"
#include "io.h"
#include "fork.h"
#include "menu.h"
#include "database.h"
#include "login.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>

#define SSH_AUTH_MAX_DURATION 60 // seconds

struct ssl_server_cb_data_t
{
	int tries;
	int error;
};

static int auth_password(ssh_session session, const char *user,
						 const char *password, void *userdata)
{
	MYSQL *db;
	struct ssl_server_cb_data_t *p_data = userdata;
	int ret;

	if ((db = db_open()) == NULL)
	{
		return SSH_AUTH_ERROR;
	}

	if (strcmp(user, "guest") == 0)
	{
		ret = load_guest_info(db);
	}
	else
	{
		ret = check_user(db, user, password);
	}

	mysql_close(db);

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

int fork_server(ssh_bind sshbind)
{
	ssh_event event;
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
	if (close(socket_server) == -1)
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

		if (ssh_handle_key_exchange(SSH_session))
		{
			log_error("ssh_handle_key_exchange() error: %s\n", ssh_get_error(SSH_session));
			goto cleanup;
		}
		ssh_set_auth_methods(SSH_session, SSH_AUTH_METHOD_PASSWORD);

		event = ssh_event_new();
		ssh_event_add_session(event, SSH_session);

		for (i = 0; i < SSH_AUTH_MAX_DURATION && !SYS_server_exit && !cb_data.error && SSH_channel == NULL; i++)
		{
			ret = ssh_event_dopoll(event, 1000); // 1 second
			if (ret == SSH_ERROR)
			{
				log_error("ssh_event_dopoll() error: %s\n", ssh_get_error(SSH_session));
				goto cleanup;
			}
		}

		if (cb_data.error)
		{
			log_error("SSH auth error, tried %d times\n", cb_data.tries);
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
