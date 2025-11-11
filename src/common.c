/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * common
 *   - common definitions
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"

// Version
const char APP_INFO[] = PACKAGE_STRING " build on " __DATE__ " " __TIME__;

// Enviroment
const char CONF_BBSD[] = "conf/bbsd.conf";
const char CONF_MENU[] = "var/menu_merged.conf";
const char CONF_BBSNET[] = "conf/bbsnet.conf";
const char CONF_BWF[] = "conf/badwords.conf";
const char CONF_TOP10_MENU[] = "var/bbs_top_menu.conf";
const char SSH_HOST_KEYFILE[] = "conf/ssh_host_rsa_key";

const char LOG_FILE_INFO[] = "log/bbsd.log";
const char LOG_FILE_ERROR[] = "log/error.log";

const char DATA_WELCOME[] = "data/welcome.txt";
const char DATA_REGISTER[] = "data/register.txt";
const char DATA_GOODBYE[] = "data/goodbye.txt";
const char DATA_LICENSE[] = "data/license.txt";
const char DATA_COPYRIGHT[] = "data/copyright.txt";
const char DATA_VERSION[] = "data/version.txt";
const char DATA_LOGIN_ERROR[] = "data/login_error.txt";
const char DATA_ACTIVE_BOARD[] = "data/active_board.txt";
const char DATA_READ_HELP[] = "data/read_help.txt";
const char DATA_EDITOR_HELP[] = "data/editor_help.txt";

const char VAR_BBS_TOP[] = "var/bbs_top.txt";

const char VAR_ARTICLE_BLOCK_SHM[] = "var/article_block_shm.~";
const char VAR_SECTION_LIST_SHM[] = "var/section_list_shm.~";
const char VAR_TRIE_DICT_SHM[] = "var/trie_dict_shm.~";
const char VAR_USER_LIST_SHM[] = "var/user_list_shm.~";

const char VAR_ARTICLE_CACHE_DIR[] = "var/articles/";
const char VAR_GEN_EX_MENU_DIR[] = "var/gen_ex/";
const char VAR_SECTION_AID_LOC_DIR[] = "var/section_aid_loc/";

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
