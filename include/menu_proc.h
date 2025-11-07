/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * menu_proc
 *   - handler functions of menu commands
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _MENU_PROC_H_
#define _MENU_PROC_H_

extern int list_section(void *param);
extern int exec_mbem(void *param);
extern int exit_bbs(void *param);
extern int license(void *param);
extern int copyright(void *param);
extern int version(void *param);
extern int reload_bbs_conf(void *param);
extern int shutdown_bbs(void *param);
extern int favor_section_filter(void *param);
extern int view_ex_article(void *param);
extern int list_ex_section(void *param);
extern int show_top10_menu(void *param);
extern int locate_article(void *param);
extern int favor_topic(void *param);
extern int list_user(void *param);
extern int list_online_user(void *param);
extern int edit_intro(void *param);
extern int edit_sign(void *param);

#endif //_MENU_PROC_H_
