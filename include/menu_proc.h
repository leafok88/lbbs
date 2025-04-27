/***************************************************************************
                          menu_proc.c  -  description
                             -------------------
    begin                : Fri May 6 2005
    copyright            : (C) 2005 by Leaflet
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

#ifndef _MENU_PROC_H_
#define _MENU_PROC_H_

extern int exec_mbem(const char *);
extern int exitbbs (const char *);
extern int bbsnet (const char *);
extern int license (const char *);
extern int copyright (const char *);
extern int reloadbbsmenu (const char *);
extern int shutdownbbs (const char *);

#endif //_MENU_PROC_H_
