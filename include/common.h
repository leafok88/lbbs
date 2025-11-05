/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * common
 *   - common definitions
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stddef.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

enum common_constant_t
{
    LINE_BUFFER_LEN = 1024,
    FILE_PATH_LEN = 4096,

    // Screen
    SCREEN_ROWS = 24,
    SCREEN_COLS = 80,

    // Network
    MAX_CLIENT_LIMIT = 2000,
    MAX_CLIENT_PER_IP_LIMIT = 100,
    IP_ADDR_LEN = 50,
    MAX_EVENTS = 10,
};

// Version
#define APP_INFO (PACKAGE_STRING " build on " __DATE__ " " __TIME__)

// Enviroment
extern const char CONF_BBSD[];
extern const char CONF_MENU[];
extern const char CONF_BBSNET[];
extern const char CONF_TOP10_MENU[];
extern const char SSH_HOST_KEYFILE[];

extern const char LOG_FILE_INFO[];
extern const char LOG_FILE_ERROR[];

extern const char DATA_WELCOME[];
extern const char DATA_REGISTER[];
extern const char DATA_GOODBYE[];
extern const char DATA_LICENSE[];
extern const char DATA_COPYRIGHT[];
extern const char DATA_VERSION[];
extern const char DATA_LOGIN_ERROR[];
extern const char DATA_ACTIVE_BOARD[];
extern const char DATA_READ_HELP[];
extern const char DATA_EDITOR_HELP[];

extern const char VAR_BBS_TOP[];

extern const char VAR_ARTICLE_BLOCK_SHM[];
extern const char VAR_SECTION_LIST_SHM[];
extern const char VAR_TRIE_DICT_SHM[];
extern const char VAR_USER_LIST_SHM[];

extern const char VAR_ARTICLE_CACHE_DIR[];
extern const char VAR_GEN_EX_MENU_DIR[];
extern const char VAR_SECTION_AID_LOC_DIR[];

// File loader
extern const char *data_files_load_startup[];
extern const int data_files_load_startup_count;

// Network
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
