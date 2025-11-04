/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * common
 *   - common definitions
 *
 * Copyright (C) 2004-2025 by Leaflet <leaflet@leafok.com>
 */

#include "common.h"

// File loader
const char *data_files_load_startup[] = {
	DATA_WELCOME,
	DATA_REGISTER,
	DATA_GOODBYE,
	DATA_LICENSE,
	DATA_COPYRIGHT,
	DATA_VERSION,
	DATA_LOGIN_ERROR,
	DATA_ACTIVE_BOARD,
	DATA_READ_HELP,
	DATA_EDITOR_HELP,
	VAR_BBS_TOP};

const int data_files_load_startup_count = sizeof(data_files_load_startup) / sizeof(const char *);

// Global declaration for sockets
int socket_server[2];
int socket_client;
char hostaddr_client[IP_ADDR_LEN];
int port_client;

// SSHv2
int SSH_v2 = 0;
ssh_bind sshbind;
ssh_session SSH_session;
ssh_channel SSH_channel;

// Global declaration for system
volatile int SYS_server_exit = 0;
volatile int SYS_child_process_count = 0;
volatile int SYS_child_exit = 0;
volatile int SYS_conf_reload = 0;

// Common function
void sig_hup_handler(int i)
{
	SYS_conf_reload = 1;
}

void sig_term_handler(int i)
{
	SYS_server_exit = 1;
}

void sig_chld_handler(int i)
{
	SYS_child_exit = 1;
}
