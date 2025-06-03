#include "log.h"
#include <stdio.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>

static int ssh_auth_password_cb(ssh_session session, const char *user, const char *password, void *userdata)
{
	if (strcmp(user, "test") == 0 && strcmp(password, "123456") == 0)
	{
		return SSH_AUTH_SUCCESS;
	}

	log_std("Debug: SSH Auth OK\n");
	return SSH_AUTH_DENIED;
}

int ssh_server(const char *hostaddr, unsigned int port)
{
	ssh_bind sshbind;
	ssh_session session;
	ssh_channel channel;
	int ssh_log_level = SSH_LOG_PROTOCOL;
	struct ssh_server_callbacks_struct cb = {
		.userdata = NULL,
		.auth_password_function = ssh_auth_password_cb};

	ssh_init();

	sshbind = ssh_bind_new();

	if (ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, hostaddr) < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &port) < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, "ssh_host_rsa_key") < 0 ||
		ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &ssh_log_level) < 0)
	{
		log_error("Error setting SSH bind options: %s\n", ssh_get_error(sshbind));
		return -1;
	}

	if (ssh_bind_listen(sshbind) < 0)
	{
		log_error("Error listening at SSH server port: %s\n", ssh_get_error(sshbind));
		return -1;
	}

	while (1)
	{
		session = ssh_new();

		if (ssh_bind_accept(sshbind, session) != SSH_OK)
		{
			log_error("Error accept SSH connection: %s\n", ssh_get_error(sshbind));
		}

		ssh_callbacks_init(&cb);
		if (ssh_set_server_callbacks(session, &cb) != SSH_OK)
		{
			log_error("Error set SSH callback: %s\n", ssh_get_error(sshbind));
		}

		ssh_set_auth_methods(session, SSH_AUTH_METHOD_PASSWORD);

		if (ssh_handle_key_exchange(session) != SSH_OK)
		{
			log_error("Error exchanging SSH keys: %s\n", ssh_get_error(sshbind));
			ssh_disconnect(session);
			continue;
		}

		channel = ssh_channel_new(session);
		if (channel == NULL || ssh_channel_open_session(channel) != SSH_OK)
		{
			log_error("Error opening SSH channel: %s\n", ssh_get_error(sshbind));
			ssh_channel_free(channel);
			continue;
		}

		log_std("Debug: #\n");

		ssh_channel_close(channel);
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

	log_std_redirect(STDOUT_FILENO);
	log_err_redirect(STDERR_FILENO);

	ssh_server("0.0.0.0", 2322);

	log_end();

	return 0;
}
