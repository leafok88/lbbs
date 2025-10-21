/***************************************************************************
						  common.h  -  description
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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stddef.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

#define LINE_BUFFER_LEN 1024
#define FILE_PATH_LEN 4096

// Version
#define APP_INFO (PACKAGE_STRING " build on " __DATE__ " " __TIME__)

// Enviroment
#define CONF_BBSD "conf/bbsd.conf"
#define CONF_MENU "var/menu_merged.conf"
#define CONF_BBSNET "conf/bbsnet.conf"
#define CONF_TOP10_MENU "var/bbs_top_menu.conf"
#define SSH_HOST_KEYFILE "conf/ssh_host_rsa_key"

#define LOG_FILE_INFO "log/bbsd.log"
#define LOG_FILE_ERROR "log/error.log"

#define DATA_WELCOME "data/welcome.txt"
#define DATA_REGISTER "data/register.txt"
#define DATA_GOODBYE "data/goodbye.txt"
#define DATA_LICENSE "data/license.txt"
#define DATA_COPYRIGHT "data/copyright.txt"
#define DATA_VERSION "data/version.txt"
#define DATA_LOGIN_ERROR "data/login_error.txt"
#define DATA_ACTIVE_BOARD "data/active_board.txt"
#define DATA_READ_HELP "data/read_help.txt"
#define DATA_EDITOR_HELP "data/editor_help.txt"

#define VAR_BBS_TOP "var/bbs_top.txt"

#define VAR_ARTICLE_BLOCK_SHM "var/article_block_shm.~"
#define VAR_SECTION_LIST_SHM "var/section_list_shm.~"
#define VAR_TRIE_DICT_SHM "var/trie_dict_shm.~"
#define VAR_USER_LIST_SHM "var/user_list_shm.~"

#define VAR_ARTICLE_CACHE_DIR "var/articles/"
#define VAR_GEN_EX_MENU_DIR "var/gen_ex/"

// File loader
extern const char *data_files_load_startup[];
extern int data_files_load_startup_count;

// Screen
#define SCREEN_ROWS 24
#define SCREEN_COLS 80

// Network
#define MAX_CLIENT_LIMIT 2000
#define MAX_CLIENT_PER_IP_LIMIT 100
#define IP_ADDR_LEN 50
#define MAX_EVENTS 10

extern int socket_server[2];
extern int socket_client;
extern char hostaddr_client[IP_ADDR_LEN];
extern int port_client;

// SSHv2
extern int SSH_v2;
extern ssh_bind sshbind;
extern ssh_session SSH_session;
extern ssh_channel SSH_channel;

// Signal handler
extern void sig_hup_handler(int);
extern void sig_term_handler(int);
extern void sig_chld_handler(int);

// System
extern volatile int SYS_server_exit;
extern volatile int SYS_child_process_count;
extern volatile int SYS_child_exit;
extern volatile int SYS_conf_reload;

#endif //_COMMON_H_
