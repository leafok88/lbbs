/***************************************************************************
						  menu.h  -  description
							 -------------------
	begin                : Wed Mar 16 2004
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

#ifndef _MENU_H_
#define _MENU_H_

#define MAX_MENUITEM_LENGTH 50
#define MAX_MENUITEMS 30
#define MAX_MENUNAME_LENGTH 256
#define MAX_MENUACTION_LENGTH 20
#define MAX_MENUTITLE_LENGTH 50
#define MAX_MENUS 256
#define MAX_MENU_DEPTH 50

struct _menu_item
{
	int row, col, r_row, r_col;
	char action[MAX_MENUACTION_LENGTH];
	int submenu;
	int priv, level, display;
	char name[MAX_MENUNAME_LENGTH];
	char text[MAX_MENUITEM_LENGTH];
};
typedef struct _menu_item MENU_ITEM;

struct _menu_title
{
	int row, col, show;
	char text[MAX_MENUTITLE_LENGTH];
};
typedef struct _menu_title MENU_TITLE;

struct _menu_screen
{
	int row, col, show;
	char filename[256];
};
typedef struct _menu_screen MENU_SCREEN;

struct _menu
{
	char name[MAX_MENUNAME_LENGTH];
	MENU_TITLE title;
	MENU_SCREEN screen;
	MENU_ITEM *items[MAX_MENUITEMS];
	int item_count, item_cur_pos;
};
typedef struct _menu MENU;

struct _menu_set
{
	char conf_file[256];
	MENU *p_menu[MAX_MENUS];
	MENU *p_menu_select[MAX_MENU_DEPTH];
	int menu_count;
	int menu_select_depth;
};
typedef struct _menu_set MENU_SET;

extern MENU_SET bbs_menu;

extern int load_menu(MENU_SET *p_menu_set, const char *conf_file);

extern void unload_menu(MENU_SET *p_menu_set);

extern int reload_menu(MENU_SET *p_menu_set);

extern int menu_control(MENU_SET *p_menu_set, int key);

extern int display_menu(MENU *p_menu);

extern int display_current_menu(MENU_SET *p_menu_set);

extern MENU *get_menu(MENU_SET *p_menu_set, const char *menu_name);

#endif //_MENU_H_
