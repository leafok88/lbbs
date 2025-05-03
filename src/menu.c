/***************************************************************************
						  menu.c  -  description
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

#include "bbs.h"
#include "bbs_cmd.h"
#include "user_priv.h"
#include "reg_ex.h"
#include "bbs_cmd.h"
#include "menu.h"
#include "log.h"
#include "io.h"
#include "screen.h"
#include "common.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <regex.h>
#include <stdlib.h>

MENU_SET bbs_menu;

int load_menu(MENU_SET *p_menu_set, const char *conf_file)
{
	FILE *fin, *fout;
	int i = 0, j;
	char buffer[LINE_BUFFER_LEN];
	char screen_filename[LINE_BUFFER_LEN];
	char temp[LINE_BUFFER_LEN];
	regmatch_t pmatch[10];

	if ((fin = fopen(conf_file, "r")) == NULL)
	{
		log_error("Open %s failed", conf_file);
		return -1;
	}

	strcpy(p_menu_set->conf_file, conf_file);

	while (fgets(buffer, sizeof(buffer), fin))
	{
		switch (buffer[0])
		{
		case '#':
			break;
		case '%':
			if (ireg("^%S_([A-Za-z0-9_]+)", buffer, 2, pmatch) == 0)
			{
				strncpy(temp, buffer + pmatch[1].rm_so,
						pmatch[1].rm_eo - pmatch[1].rm_so);
				temp[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';
				sprintf(screen_filename, "%sMENU_SCR_%s", app_temp_dir, temp);

				if ((fout = fopen(screen_filename, "w")) == NULL)
				{
					log_error("Open %s failed", screen_filename);
					return -2;
				}

				while (fgets(buffer, sizeof(buffer), fin))
				{
					if (buffer[0] != '%')
						fputs(buffer, fout);
					else
						break;
				}

				fclose(fout);
				break;
			}

			if (ireg("^%menu ([A-Za-z0-9_]+)", buffer, 2, pmatch) == 0)
			{
				p_menu_set->p_menu[i] = malloc(sizeof(MENU));
				p_menu_set->p_menu[i]->title.show = 0;
				p_menu_set->p_menu[i]->screen.show = 0;

				strncpy(p_menu_set->p_menu[i]->name,
						buffer + pmatch[1].rm_so,
						pmatch[1].rm_eo - pmatch[1].rm_so);
				p_menu_set->p_menu[i]->name[pmatch[1].rm_eo - pmatch[1].rm_so] =
					'\0';

				j = 0;

				while (fgets(buffer, sizeof(buffer), fin))
				{
					if (buffer[0] == '#')
					{
						continue;
					}
					if (buffer[0] == '%')
					{
						p_menu_set->p_menu[i]->item_count = j;
						p_menu_set->p_menu[i]->item_cur_pos = 0;
						i++;
						break;
					}
					if (ireg("^!([A-Za-z0-9_.]+)[[:space:]]*([0-9]+),"
							 "[[:space:]]*([0-9]+),[[:space:]]*([0-9]+),"
							 "[[:space:]]*([0-9]+),[[:space:]]*\"([^\"]+)\","
							 "[[:space:]]*\"([^\"]+)\"",
							 buffer, 8, pmatch) == 0)
					{
						p_menu_set->p_menu[i]->items[j] =
							malloc(sizeof(MENU_ITEM));
						p_menu_set->p_menu[i]->items[j]->submenu = 1;
						strncpy(p_menu_set->p_menu[i]->items[j]->action,
								buffer + pmatch[1].rm_so,
								pmatch[1].rm_eo - pmatch[1].rm_so);
						p_menu_set->p_menu[i]->items[j]->action[pmatch[1].rm_eo -
																pmatch[1].rm_so] = '\0';
						strncpy(temp, buffer + pmatch[2].rm_so,
								pmatch[2].rm_eo - pmatch[2].rm_so);
						temp[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';
						p_menu_set->p_menu[i]->items[j]->row = atoi(temp);
						strncpy(temp,
								buffer + pmatch[3].rm_so,
								pmatch[3].rm_eo - pmatch[3].rm_so);
						temp[pmatch[3].rm_eo - pmatch[3].rm_so] = '\0';
						p_menu_set->p_menu[i]->items[j]->col = atoi(temp);
						strncpy(temp,
								buffer + pmatch[4].rm_so,
								pmatch[4].rm_eo - pmatch[4].rm_so);
						temp[pmatch[4].rm_eo - pmatch[4].rm_so] = '\0';
						p_menu_set->p_menu[i]->items[j]->priv = atoi(temp);
						strncpy(temp,
								buffer + pmatch[5].rm_so,
								pmatch[5].rm_eo - pmatch[5].rm_so);
						temp[pmatch[5].rm_eo - pmatch[5].rm_so] = '\0';
						p_menu_set->p_menu[i]->items[j]->level = atoi(temp);
						strncpy(p_menu_set->p_menu[i]->items[j]->name,
								buffer + pmatch[6].rm_so,
								pmatch[6].rm_eo - pmatch[6].rm_so);
						p_menu_set->p_menu[i]->items[j]->name[pmatch[6].rm_eo -
															  pmatch[6].rm_so] =
							'\0';
						strncpy(p_menu_set->p_menu[i]->items[j]->text,
								buffer + pmatch[7].rm_so,
								pmatch[7].rm_eo - pmatch[7].rm_so);
						p_menu_set->p_menu[i]->items[j]->text[pmatch[7].rm_eo -
															  pmatch[7].rm_so] =
							'\0';
						j++;
						continue;
					}
					if (ireg("^@([A-Za-z0-9_]+)[[:space:]]*([0-9]+),"
							 "[[:space:]]*([0-9]+),[[:space:]]*([0-9]+),"
							 "[[:space:]]*([0-9]+),[[:space:]]*\"([^\"]+)\","
							 "[[:space:]]*\"([^\"]+)\"",
							 buffer, 8, pmatch) == 0)
					{
						p_menu_set->p_menu[i]->items[j] =
							malloc(sizeof(MENU_ITEM));
						p_menu_set->p_menu[i]->items[j]->submenu = 0;
						strncpy(p_menu_set->p_menu[i]->items[j]->action,
								buffer + pmatch[1].rm_so,
								pmatch[1].rm_eo - pmatch[1].rm_so);
						p_menu_set->p_menu[i]->items[j]->action[pmatch[1].rm_eo -
																pmatch[1].rm_so] = '\0';
						strncpy(temp, buffer + pmatch[2].rm_so,
								pmatch[2].rm_eo - pmatch[2].rm_so);
						temp[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';
						p_menu_set->p_menu[i]->items[j]->row = atoi(temp);
						strncpy(temp,
								buffer + pmatch[3].rm_so,
								pmatch[3].rm_eo - pmatch[3].rm_so);
						temp[pmatch[3].rm_eo - pmatch[3].rm_so] = '\0';
						p_menu_set->p_menu[i]->items[j]->col = atoi(temp);
						strncpy(temp,
								buffer + pmatch[4].rm_so,
								pmatch[4].rm_eo - pmatch[4].rm_so);
						temp[pmatch[4].rm_eo - pmatch[4].rm_so] = '\0';
						p_menu_set->p_menu[i]->items[j]->priv = atoi(temp);
						strncpy(temp,
								buffer + pmatch[5].rm_so,
								pmatch[5].rm_eo - pmatch[5].rm_so);
						temp[pmatch[5].rm_eo - pmatch[5].rm_so] = '\0';
						p_menu_set->p_menu[i]->items[j]->level = atoi(temp);
						strncpy(p_menu_set->p_menu[i]->items[j]->name,
								buffer + pmatch[6].rm_so,
								pmatch[6].rm_eo - pmatch[6].rm_so);
						p_menu_set->p_menu[i]->items[j]->name[pmatch[6].rm_eo -
															  pmatch[6].rm_so] =
							'\0';
						strncpy(p_menu_set->p_menu[i]->items[j]->text,
								buffer + pmatch[7].rm_so,
								pmatch[7].rm_eo - pmatch[7].rm_so);
						p_menu_set->p_menu[i]->items[j]->text[pmatch[7].rm_eo -
															  pmatch[7].rm_so] =
							'\0';
						j++;
						continue;
					}
					if (ireg("^title[[:space:]]*([0-9]+),"
							 "[[:space:]]*([0-9]+),[[:space:]]*\"([^\"]+)\"",
							 buffer, 4, pmatch) == 0)
					{
						p_menu_set->p_menu[i]->title.show = 1;
						strncpy(temp,
								buffer + pmatch[1].rm_so,
								pmatch[1].rm_eo - pmatch[1].rm_so);
						temp[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';
						p_menu_set->p_menu[i]->title.row = atoi(temp);
						strncpy(temp,
								buffer + pmatch[2].rm_so,
								pmatch[2].rm_eo - pmatch[2].rm_so);
						temp[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';
						p_menu_set->p_menu[i]->title.col = atoi(temp);
						strncpy(p_menu_set->p_menu[i]->title.text,
								buffer + pmatch[3].rm_so,
								pmatch[3].rm_eo - pmatch[3].rm_so);
						p_menu_set->p_menu[i]->title.text[pmatch[3].rm_eo -
														  pmatch[3].rm_so] =
							'\0';
						continue;
					}
					if (ireg("^screen[[:space:]]*([0-9]+),"
							 "[[:space:]]*([0-9]+),[[:space:]]*S_([A-Za-z0-9_]+)",
							 buffer, 4, pmatch) == 0)
					{
						p_menu_set->p_menu[i]->screen.show = 1;
						strncpy(temp,
								buffer + pmatch[1].rm_so,
								pmatch[1].rm_eo - pmatch[1].rm_so);
						temp[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';
						p_menu_set->p_menu[i]->screen.row = atoi(temp);
						strncpy(temp,
								buffer + pmatch[2].rm_so,
								pmatch[2].rm_eo - pmatch[2].rm_so);
						temp[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';
						p_menu_set->p_menu[i]->screen.col = atoi(temp);
						strncpy(temp,
								buffer + pmatch[3].rm_so,
								pmatch[3].rm_eo - pmatch[3].rm_so);
						temp[pmatch[3].rm_eo - pmatch[3].rm_so] = '\0';
						sprintf(p_menu_set->p_menu[i]->screen.filename,
								"%sMENU_SCR_%s", app_temp_dir, temp);
						continue;
					}
				}
			}
			break;
		}
	}
	fclose(fin);

	p_menu_set->menu_count = i;
	p_menu_set->menu_select_depth = 0;
	p_menu_set->p_menu_select[p_menu_set->menu_select_depth] = (i == 0 ? NULL : p_menu_set->p_menu[0]);

	return 0;
}

MENU *
get_menu(MENU_SET *p_menu_set, const char *menu_name)
{
	int i;

	for (i = 0; i < p_menu_set->menu_count; i++)
	{
		if (strcmp(p_menu_set->p_menu[i]->name, menu_name) == 0)
		{
			return p_menu_set->p_menu[i];
		}
	}

	return NULL;
}

static void display_menu_cursor(MENU *p_menu, int show)
{
	moveto((p_menu->items[p_menu->item_cur_pos])->r_row,
		   (p_menu->items[p_menu->item_cur_pos])->r_col - 2);
	prints(show ? ">" : " ");
	iflush();
}

int display_menu(MENU *p_menu)
{
	int i, row, col, menu_selectable = 0;

	if (p_menu == NULL)
		return -1;

	if (p_menu->title.show)
		show_top(p_menu->title.text);

	if (p_menu->screen.show)
	{
		moveto(p_menu->screen.row, p_menu->screen.col);
		if (display_file(p_menu->screen.filename) != 0)
			log_error("Display menu screen <%s> failed!\n",
					  p_menu->screen.filename);
	}

	row = p_menu->items[0]->row;
	col = p_menu->items[0]->col;

	for (i = 0; i < p_menu->item_count; i++)
	{
		if (checkpriv(&BBS_priv, 0, p_menu->items[i]->priv) == 0 || checklevel(&BBS_priv, p_menu->items[i]->level) == 0)
		{
			p_menu->items[i]->display = 0;
		}
		else
		{
			p_menu->items[i]->display = 1;

			menu_selectable = 1;

			if (p_menu->items[i]->row != 0)
				row = p_menu->items[i]->row;
			else
				row++;
			p_menu->items[i]->r_row = row;
			if (p_menu->items[i]->col != 0)
				col = p_menu->items[i]->col;
			p_menu->items[i]->r_col = col;
			moveto(row, col);
			prints(p_menu->items[i]->text);
			iflush();
		}
	}

	if (!menu_selectable)
		return -1;

	display_menu_cursor(p_menu, 1);

	return 0;
}

int display_current_menu(MENU_SET *p_menu_set)
{
	MENU *p_menu;

	p_menu = p_menu_set->p_menu_select[p_menu_set->menu_select_depth];

	return display_menu(p_menu);
}

int menu_control(MENU_SET *p_menu_set, int key)
{
	int i;
	MENU *p_menu;

	if (p_menu_set->menu_count == 0)
		return 0;

	p_menu = p_menu_set->p_menu_select[p_menu_set->menu_select_depth];

	switch (key)
	{
	case CR:
		igetch(1); // Cleanup remaining '\n' in the buffer
	case KEY_RIGHT:
		if (p_menu->items[p_menu->item_cur_pos]->submenu)
		{
			if (strcmp(p_menu->items[p_menu->item_cur_pos]->action, "..") == 0)
				return menu_control(p_menu_set, KEY_LEFT);
			p_menu_set->menu_select_depth++;
			p_menu =
				p_menu_set->p_menu_select[p_menu_set->menu_select_depth] =
					get_menu(p_menu_set,
							 p_menu->items[p_menu->item_cur_pos]->action);
			if (display_menu(p_menu) != 0)
				return menu_control(p_menu_set, KEY_LEFT);
			break;
		}
		else
		{
			return (exec_cmd(p_menu->items[p_menu->item_cur_pos]->action,
							 p_menu->items[p_menu->item_cur_pos]->name));
		}
	case KEY_LEFT:
		if (p_menu_set->menu_select_depth > 0)
		{
			p_menu_set->menu_select_depth--;
			if (display_current_menu(p_menu_set) != 0)
				return menu_control(p_menu_set, KEY_LEFT);
			break;
		}
		else
		{
			display_menu_cursor(p_menu, 0);
			p_menu->item_cur_pos = p_menu->item_count - 1;
			while (!p_menu->items[p_menu->item_cur_pos]->display || p_menu->items[p_menu->item_cur_pos]->priv != 0 || p_menu->items[p_menu->item_cur_pos]->level != 0)
				p_menu->item_cur_pos--;
			display_menu_cursor(p_menu, 1);
			break;
		}
	case KEY_UP:
		display_menu_cursor(p_menu, 0);
		do
		{
			p_menu->item_cur_pos--;
			if (p_menu->item_cur_pos < 0)
				p_menu->item_cur_pos = p_menu->item_count - 1;
		} while (!p_menu->items[p_menu->item_cur_pos]->display);
		display_menu_cursor(p_menu, 1);
		break;
	case KEY_DOWN:
		display_menu_cursor(p_menu, 0);
		do
		{
			p_menu->item_cur_pos++;
			if (p_menu->item_cur_pos >= p_menu->item_count)
				p_menu->item_cur_pos = 0;
		} while (!p_menu->items[p_menu->item_cur_pos]->display);
		display_menu_cursor(p_menu, 1);
		break;
	default:
		for (i = 0; i < p_menu->item_count; i++)
		{
			if (key == p_menu->items[i]->name[0] &&
				p_menu->items[i]->display)
			{
				display_menu_cursor(p_menu, 0);
				p_menu->item_cur_pos = i;
				display_menu_cursor(p_menu, 1);
				return 0;
			}
		}
		if (isalpha(key))
		{
			for (i = 0; i < p_menu->item_count; i++)
			{
				if (toupper(key) == toupper(p_menu->items[i]->name[0]) &&
					p_menu->items[i]->display)
				{
					display_menu_cursor(p_menu, 0);
					p_menu->item_cur_pos = i;
					display_menu_cursor(p_menu, 1);
					return 0;
				}
			}
		}
	}

	return 0;
}

void unload_menu(MENU_SET *p_menu_set)
{
	MENU *p_menu;
	MENU_ITEM *p_menuitem;
	int i, j;

	for (i = 0; i < p_menu_set->menu_count; i++)
	{
		p_menu = p_menu_set->p_menu[i];
		for (j = 0; j < p_menu->item_count; j++)
		{
			free(p_menu->items[j]);
		}
		remove(p_menu->screen.filename);
		free(p_menu);
	}

	p_menu_set->menu_count = 0;
	p_menu_set->menu_select_depth = 0;
}

int reload_menu(MENU_SET *p_menu_set)
{
	int result;
	char conf_file[FILE_PATH_LEN];

	strncpy(conf_file, p_menu_set->conf_file, sizeof(conf_file));
	unload_menu(p_menu_set);
	result = load_menu(p_menu_set, conf_file);

	return result;
}
