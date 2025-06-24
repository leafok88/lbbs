/***************************************************************************
						  menu_proc.c  -  description
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

#ifndef _MENU_PROC_H_
#define _MENU_PROC_H_

extern int list_section(void *param);
extern int exec_mbem(void *param);
extern int exit_bbs(void *param);
extern int bbsnet(void *param);
extern int license(void *param);
extern int copyright(void *param);
extern int reload_bbs_conf(void *param);
extern int shutdown_bbs(void *param);
extern int favour_section_filter(void *param);
extern int view_ex_article(void *param);
extern int list_ex_section(void *param);

#endif //_MENU_PROC_H_
