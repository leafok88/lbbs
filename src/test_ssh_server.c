/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * test_ssh_server
 *   - tester for network server with SSH support
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

// This test was written based on libssh example/proxy.c

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "log.h"
#include <stdio.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

enum test_ssh_server_constant_t
{
	BUF_SIZE = 2048,
};

static const char SSH_HOST_RSA_KEYFILE[] = "../conf/ssh_host_rsa_key";

static const char USER[] = "test";
static const char PASSWORD[] = "123456";

static ssh_channel SSH_channel;
static int authenticated = 0;
static int tries = 0;
static int error = 0;

static int auth_password(ssh_session session, const char *user,
						 const char *password, void *userdata)
{
	(void)userdata;

	log_common("Authenticating user %s pwd %s\n", user, password);
	if (strcmp(user, USER) == 0 && strcmp(password, PASSWORD) == 0)
	{
		authenticated = 1;
		log_common("Authenticated\n");
		return SSH_AUTH_SUCCESS;
	}
	if (tries >= 3)
	{
		log_error("Too many authentication tries\n");
		ssh_disconnect(session);
		error = 1;
		return SSH_AUTH_DENIED;
	}
	tries++;
	return SSH_AUTH_DENIED;
}

static int pty_request(ssh_session session, ssh_channel channel, const char *term,
					   int x, int y, int px, int py, void *userdata)
{
	(void)session;
	(void)channel;
	(void)term;
	(void)x;
	(void)y;
	(void)px;
	(void)py;
	(void)userdata;
	log_common("Allocated terminal\n");
	return 0;
}

static int shell_request(ssh_session session, ssh_channel channel, void *userdata)
{
	(void)session;
	(void)channel;
	(void)userdata;
	log_common("Allocated shell\n");
	return 0;
}

struct ssh_channel_callbacks_struct channel_cb = {
	.channel_pty_request_function = pty_request,
	.channel_shell_request_function = shell_request};

static ssh_channel channel_open(ssh_session session, void *userdata)
{
	(void)session;
	(void)userdata;

	if (SSH_channel != NULL)
		return NULL;

	log_common("Allocated session channel\n");
	SSH_channel = ssh_channel_new(session);
	ssh_callbacks_init(&channel_cb);
	ssh_set_channel_callbacks(SSH_channel, &channel_cb);

	return SSH_channel;
}

int ssh_server(const char *hostaddr, unsigned int port)
{
	ssh_bind sshbind;
	ssh_session session;
	ssh_event event;

	struct ssh_server_callbacks_struct cb = {
		.userdata = NULL,
		.auth_password_function = auth_password,
		.channel_open_request_session_function = channel_open};

	long int ssh_timeout = 0;

	char buf[BUF_SIZE];
	char host[128] = "";
	int i, r;

	int ssh_log_level = SSH_LOG_PROTOCOL;

	ssh_init();

	sshbind = ssh_bind_new();

	if (ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, hostaddr) < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &port) < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, SSH_HOST_RSA_KEYFILE) < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY_ALGORITHMS, "ssh-rsa,rsa-sha2-512,rsa-sha2-256,ecdsa-sha2-nistp256") < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_PUBKEY_ACCEPTED_KEY_TYPES, "ssh-rsa,rsa-sha2-512,rsa-sha2-256,ecdsa-sha2-nistp256") < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_KEY_EXCHANGE, "curve25519-sha256@libssh.org,ecdh-sha2-nistp256,ecdh-sha2-nistp384,ecdh-sha2-nistp521,diffie-hellman-group-exchange-sha256,diffie-hellman-group16-sha512,diffie-hellman-group18-sha512,diffie-hellman-group14-sha256,diffie-hellman-group14-sha1") < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HMAC_C_S, "umac-64-etm@openssh.com,umac-128-etm@openssh.com,hmac-sha2-256-etm@openssh.com,hmac-sha2-512-etm@openssh.com,hmac-sha1-etm@openssh.com,umac-64@openssh.com,umac-128@openssh.com,hmac-sha2-256,hmac-sha2-512,hmac-sha1") < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HMAC_S_C, "umac-64-etm@openssh.com,umac-128-etm@openssh.com,hmac-sha2-256-etm@openssh.com,hmac-sha2-512-etm@openssh.com,hmac-sha1-etm@openssh.com,umac-64@openssh.com,umac-128@openssh.com,hmac-sha2-256,hmac-sha2-512,hmac-sha1") < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_CIPHERS_C_S, "chacha20-poly1305@openssh.com,aes128-ctr,aes192-ctr,aes256-ctr,aes128-gcm@openssh.com,aes256-gcm@openssh.com") < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_CIPHERS_S_C, "chacha20-poly1305@openssh.com,aes128-ctr,aes192-ctr,aes256-ctr,aes128-gcm@openssh.com,aes256-gcm@openssh.com") < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &ssh_log_level) < 0)
	{
		log_error("Error setting SSH bind options: %s\n", ssh_get_error(sshbind));
		ssh_bind_free(sshbind);
		return -1;
	}

	if (ssh_bind_listen(sshbind) < 0)
	{
		log_error("Error listening at SSH server port: %s\n", ssh_get_error(sshbind));
		ssh_bind_free(sshbind);
		return -1;
	}

	while (1)
	{
		session = ssh_new();

		if (ssh_bind_accept(sshbind, session) == SSH_OK)
		{
			pid_t pid = fork();
			switch (pid)
			{
			case 0:
				ssh_bind_free(sshbind);

				ssh_callbacks_init(&cb);
				ssh_set_server_callbacks(session, &cb);

				ssh_timeout = 60; // second
				if (ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &ssh_timeout) < 0)
				{
					log_error("Error setting SSH options: %s\n", ssh_get_error(session));
					ssh_disconnect(session);
					_exit(1);
				}

				if (ssh_handle_key_exchange(session))
				{
					log_error("ssh_handle_key_exchange: %s\n", ssh_get_error(session));
					ssh_disconnect(session);
					_exit(1);
				}
				ssh_set_auth_methods(session, SSH_AUTH_METHOD_PASSWORD | SSH_AUTH_METHOD_GSSAPI_MIC);

				event = ssh_event_new();
				ssh_event_add_session(event, session);

				while (!(authenticated && SSH_channel != NULL))
				{
					if (error)
						break;
					r = ssh_event_dopoll(event, -1);
					if (r == SSH_ERROR)
					{
						log_error("Error : %s\n", ssh_get_error(session));
						ssh_disconnect(session);
						_exit(1);
					}
				}

				if (error)
				{
					log_error("Error, exiting loop\n");
					_exit(1);
				}
				else
				{
					log_common("Authenticated and got a channel\n");
				}

				ssh_timeout = 0;
				if (ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &ssh_timeout) < 0)
				{
					log_error("Error setting SSH options: %s\n", ssh_get_error(session));
					ssh_disconnect(session);
					_exit(1);
				}

				snprintf(buf, sizeof(buf), "Hello, welcome to the Sample SSH proxy.\r\nPlease select your destination: ");
				ssh_channel_write(SSH_channel, buf, (uint32_t)strlen(buf));
				do
				{
					i = ssh_channel_read(SSH_channel, buf, sizeof(buf), 0);
					if (i > 0)
					{
						ssh_channel_write(SSH_channel, buf, (uint32_t)i);
						if (strlen(host) + (size_t)i < sizeof(host))
						{
							strncat(host, buf, (size_t)i);
						}
						if (strchr(host, '\x0d'))
						{
							*strchr(host, '\x0d') = '\0';
							ssh_channel_write(SSH_channel, "\n", 1);
							break;
						}
					}
					else
					{
						log_error("Error: %s\n", ssh_get_error(session));
						_exit(1);
					}
				} while (i > 0);
				snprintf(buf, sizeof(buf), "Trying to connect to \"%s\"\r\n", host);
				ssh_channel_write(SSH_channel, buf, (uint32_t)strlen(buf));
				log_common("%s", buf);

				ssh_disconnect(session);
				ssh_free(session);

				_exit(0);
			case -1:
				log_error("Failed to fork\n");
				break;
			}
		}
		else
		{
			log_error("%s\n", ssh_get_error(sshbind));
		}

		/* Since the session has been passed to a child fork, do some cleaning
		 * up at the parent process. */
		ssh_disconnect(session);
		ssh_free(session);
	}

	ssh_bind_free(sshbind);
	ssh_finalize();

	return 0;
}

int main(int argc, char *argv[])
{
	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_common_redir(STDOUT_FILENO);
	log_error_redir(STDERR_FILENO);

	ssh_server("0.0.0.0", 2322);

	log_end();

	return 0;
}
