/***************************************************************************
						  menu.h  -  description
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

#ifndef _MENU_H_
#define _MENU_H_

#include "common.h"
#include "trie_dict.h"

#define MAX_MENUITEM_LENGTH 50
#define MAX_MENUITEMS 30
#define MAX_MENUNAME_LENGTH 256
#define MAX_MENUACTION_LENGTH 20
#define MAX_MENUTITLE_LENGTH 50
#define MAX_MENUS 256
#define MAX_MENU_DEPTH 50

struct menu_item_t
{
	int row, col, r_row, r_col;
	char action[MAX_MENUACTION_LENGTH];
	int submenu;
	int priv, level, display;
	char name[MAX_MENUNAME_LENGTH];
	char text[MAX_MENUITEM_LENGTH];
};
typedef struct menu_item_t MENU_ITEM;

struct menu_title_t
{
	int row, col, show;
	char text[MAX_MENUTITLE_LENGTH];
};
typedef struct menu_title_t MENU_TITLE;

struct menu_screen_t
{
	int row, col, show;
	char filename[FILE_PATH_LEN];
};
typedef struct menu_screen_t MENU_SCREEN;

struct menu_t
{
	char name[MAX_MENUNAME_LENGTH];
	MENU_TITLE title;
	MENU_SCREEN screen;
	MENU_ITEM *items[MAX_MENUITEMS];
	int item_count, item_cur_pos;
};
typedef struct menu_t MENU;

struct menu_set_t
{
	char conf_file[FILE_PATH_LEN];
	MENU *p_menu[MAX_MENUS];
	MENU *p_menu_select[MAX_MENU_DEPTH];
	int menu_count;
	TRIE_NODE *p_menu_name_dict;
	int menu_select_depth;
};
typedef struct menu_set_t MENU_SET;

extern MENU_SET bbs_menu;

extern int load_menu(MENU_SET *p_menu_set, const char *conf_file);

extern void unload_menu(MENU_SET *p_menu_set);

extern int reload_menu(MENU_SET *p_menu_set);

extern int menu_control(MENU_SET *p_menu_set, int key);

extern int display_menu(MENU *p_menu);

extern int display_current_menu(MENU_SET *p_menu_set);

extern MENU *get_menu(MENU_SET *p_menu_set, const char *menu_name);

#endif //_MENU_H_
