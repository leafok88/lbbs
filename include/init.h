/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * init
 *   - initializer of server daemon
 *
 * Copyright (C) 2004-2026  Leaflet <leaflet@leafok.com>
 */

#ifndef _INIT_H_
#define _INIT_H_

extern int init_daemon(void);

extern int load_conf(const char *conf_file);

#endif //_INIT_H_
