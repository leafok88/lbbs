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
#include "bbs_cmd.h"
#include <stdint.h>
#include <sys/shm.h>

#define MAX_MENU_NAME_LENGTH 30
#define MAX_ITEMS_PER_MENU 256
#define MAX_MENUITEM_NAME_LENGTH 256
#define MAX_MENUITEM_TEXT_LENGTH 100
#define MAX_MENUITEM_ACTION_LENGTH 30
#define MAX_MENUTITLE_TEXT_LENGTH 50
#define MAX_MENU_SCR_NAME_LENGTH 20
#define MAX_MENU_SCR_BUF_LENGTH 2000
#define MAX_MENUS 256
#define MAX_MENUITEMS 5120
#define MAX_MENU_DEPTH 50

typedef uint64_t MENU_ID;
typedef uint64_t MENU_ITEM_ID;
typedef uint64_t MENU_SCREEN_ID;

struct menu_item_t
{
	int16_t row, col;
	char action[MAX_MENUITEM_ACTION_LENGTH];
	MENU_ID action_menu_id;
	bbs_cmd_handler action_cmd_handler;
	int8_t submenu;
	int priv, level;
	char name[MAX_MENUITEM_NAME_LENGTH];
	char text[MAX_MENUITEM_TEXT_LENGTH];
};
typedef struct menu_item_t MENU_ITEM;

struct menu_title_t
{
	int16_t row, col;
	int8_t show;
	char text[MAX_MENUTITLE_TEXT_LENGTH];
};
typedef struct menu_title_t MENU_TITLE;

struct menu_screen_t
{
	char name[MAX_MENU_SCR_NAME_LENGTH];
	int64_t buf_offset;
	int64_t buf_length;
};
typedef struct menu_screen_t MENU_SCREEN;

struct menu_t
{
	char name[MAX_MENU_NAME_LENGTH];
	MENU_TITLE title;
	char screen_name[MAX_MENU_SCR_NAME_LENGTH];
	MENU_SCREEN_ID screen_id;
	int8_t screen_show;
	int16_t screen_row, screen_col;
	MENU_ITEM_ID items[MAX_ITEMS_PER_MENU];
	int16_t item_count;
	int16_t page_row, page_col;
	int16_t page_item_limit;
};
typedef struct menu_t MENU;

struct menu_set_t
{
	int shmid;
	void *p_reserved;
	void *p_menu_pool;
	void *p_menu_item_pool;
	void *p_menu_screen_pool;
	char *p_menu_screen_buf;
	char *p_menu_screen_buf_free;
	int16_t menu_count;
	int16_t menu_item_count;
	int16_t menu_screen_count;
	TRIE_NODE *p_menu_name_dict;
	TRIE_NODE *p_menu_screen_dict;
	MENU_ID menu_id_path[MAX_MENU_DEPTH];
	int16_t menu_item_pos[MAX_MENU_DEPTH];
	int16_t choose_step;
	int8_t menu_item_display[MAX_ITEMS_PER_MENU];
	int16_t menu_item_r_row[MAX_ITEMS_PER_MENU];
	int16_t menu_item_r_col[MAX_ITEMS_PER_MENU];
	int16_t menu_item_page_id[MAX_ITEMS_PER_MENU];
};
typedef struct menu_set_t MENU_SET;

extern MENU_SET *p_bbs_menu;

extern int load_menu(MENU_SET *p_menu_set, const char *conf_file);
extern int unload_menu(MENU_SET *p_menu_set);

extern int set_menu_shm_readonly(MENU_SET *p_menu_set);
extern int detach_menu_shm(MENU_SET *p_menu_set);

extern int display_menu_cursor(MENU_SET *p_menu_set, int show);
extern int menu_control(MENU_SET *p_menu_set, int key);
extern int display_menu(MENU_SET *p_menu_set);

inline MENU *get_menu(MENU_SET *p_menu_set, const char *menu_name)
{
	MENU *value = NULL;

	trie_dict_get(p_menu_set->p_menu_name_dict, menu_name, (int64_t *)&value);

	return ((MENU *)value);
}

inline MENU *get_menu_by_id(MENU_SET *p_menu_set, MENU_ID menu_id)
{
	if (menu_id < 0 || menu_id >= p_menu_set->menu_count)
	{
		return NULL;
	}

	return (p_menu_set->p_menu_pool + sizeof(MENU) * menu_id);
}

inline MENU_ITEM *get_menu_item_by_id(MENU_SET *p_menu_set, MENU_ITEM_ID menu_item_id)
{
	if (menu_item_id < 0 || menu_item_id >= p_menu_set->menu_item_count)
	{
		return NULL;
	}

	return (p_menu_set->p_menu_item_pool + sizeof(MENU_ITEM) * menu_item_id);
}

inline MENU_SCREEN *get_menu_screen_by_id(MENU_SET *p_menu_set, MENU_SCREEN_ID menu_screen_id)
{
	if (menu_screen_id < 0 || menu_screen_id >= p_menu_set->menu_screen_count)
	{
		return NULL;
	}

	return (p_menu_set->p_menu_screen_pool + sizeof(MENU_ITEM) * menu_screen_id);
}

#endif //_MENU_H_
