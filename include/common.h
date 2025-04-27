/***************************************************************************
                          common.h  -  description
                             -------------------
    begin                : Mon Oct 18 2004
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

#ifndef _COMMON_H_
#define _COMMON_H_

//Version
extern char app_version[256];

//Enviroment
extern char app_home_dir[256];
extern char app_temp_dir[256];

//Network
extern int socket_server;
extern int socket_client;
extern char hostaddr_server[50];
extern char hostaddr_client[50];
extern int port_server;
extern int port_client;

//Database
extern char DB_host[256];
extern char DB_username[50];
extern char DB_password[50];
extern char DB_database[50];

//Signal
#define SIG_RELOAD_MENU	0x22

//Signal handler
extern void reload_bbs_menu (int);
extern void system_exit (int);
extern void child_exit (int);

//System
extern int SYS_exit;
extern int SYS_child_process_count;

#endif //_COMMON_H_
