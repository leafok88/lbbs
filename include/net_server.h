/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * net_server
 *   - network server with SSH support
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _NET_SERVER_H_
#define _NET_SERVER_H_

#include <netinet/in.h>

extern int net_server(const char *hostaddr, in_port_t port[]);

#endif //_NET_SERVER_H_
