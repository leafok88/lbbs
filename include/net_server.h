/***************************************************************************
						net_server.h  -  description
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

#ifndef _NET_SERVER_H_
#define _NET_SERVER_H_

#include <netinet/in.h>

extern int net_server(const char *hostaddr, in_port_t port);

#endif //_NET_SERVER_H_
