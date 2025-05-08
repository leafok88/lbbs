/***************************************************************************
						  menu.c  -  description
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
#include <stdlib.h>

#define MENU_SCREEN_PATH_PREFIX "var/MENU_SCR_"
#define MENU_CONF_DELIM_WITH_SPACE " ,\t\r\n"
#define MENU_CONF_DELIM_WITHOUT_SPACE "\r\n"

MENU_SET bbs_menu;

int load_menu(MENU_SET *p_menu_set, const char *conf_file)
{
	FILE *fin, *fout;
	int fin_line = 0;
	int i = 0;
	int j = 0;
	char buffer[LINE_BUFFER_LEN];
	char screen_filename[FILE_PATH_LEN];
	char *p = NULL;
	char *q = NULL;
	char *saveptr = NULL;
	MENU *p_menu = NULL;
	MENU_ITEM *p_item = NULL;

	p_menu_set->menu_count = 0;

	if ((fin = fopen(conf_file, "r")) == NULL)
	{
		log_error("Open %s failed", conf_file);
		return -2;
	}

	strncpy(p_menu_set->conf_file, conf_file, sizeof(p_menu_set->conf_file) - 1);
	p_menu_set->conf_file[sizeof(p_menu_set->conf_file) - 1] = '\0';

	while (fgets(buffer, sizeof(buffer), fin))
	{
		fin_line++;

		p = strtok_r(buffer, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
		if (p == NULL) // Blank line
		{
			continue;
		}

		if (*p == '#' || *p == '\r' || *p == '\n') // Comment or blank line
		{
			continue;
		}

		if (*p == '%')
		{
			p++;

			if (strcmp(p, "menu") == 0) // BEGIN of sub-menu
			{
				if (p_menu != NULL)
				{
					log_error("Begin new menu without end the prior one, in menu config line %d\n", fin_line);
					return -1;
				}
				p_menu = (MENU *)malloc(sizeof(MENU));
				if (p_menu == NULL)
				{
					log_error("Unable to allocate memory for menu\n");
					return -3;
				}
				p_menu_set->p_menu[i] = p_menu;
				i++;
				p_menu_set->menu_count = i;

				j = 0; // Menu item counter
				p_menu->item_count = 0;
				p_menu->item_cur_pos = 0;
				p_menu->title.show = 0;
				p_menu->screen.show = 0;

				q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
				if (q == NULL)
				{
					log_error("Error menu name in menu config line %d\n", fin_line);
					return -1;
				}
				p = q;
				while (isalnum(*q) || *q == '_')
				{
					q++;
				}
				if (*q != '\0')
				{
					log_error("Error menu name in menu config line %d\n", fin_line);
					return -1;
				}

				if (q - p > sizeof(p_menu->name) - 1)
				{
					log_error("Too longer menu name in menu config line %d\n", fin_line);
					return -1;
				}
				strncpy(p_menu->name, p, sizeof(p_menu->name) - 1);
				p_menu->name[sizeof(p_menu->name) - 1] = '\0';

				// Check syntax
				q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
				if (q != NULL)
				{
					log_error("Unknown extra content in menu config line %d\n", fin_line);
					return -1;
				}

				while (fgets(buffer, sizeof(buffer), fin))
				{
					fin_line++;

					p = strtok_r(buffer, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
					if (p == NULL) // Blank line
					{
						continue;
					}

					if (*p == '#' || *p == '\r' || *p == '\n') // Comment or blank line
					{
						continue;
					}

					if (*p == '%') // END of sub-menu
					{
						p_menu = NULL;
						break;
					}
					else if (*p == '!' || *p == '@')
					{
						// BEGIN of menu item
						p_item = (MENU_ITEM *)malloc(sizeof(MENU_ITEM));
						if (p_item == NULL)
						{
							log_error("Unable to allocate memory for menu item\n");
							return -3;
						}
						p_menu->items[j] = p_item;
						j++;
						p_menu->item_count = j;

						p_item->submenu = (*p == '!' ? 1 : 0);

						// Menu item action
						p++;
						if (strcmp(p, "..") == 0) // Return to parent menu
						{
							q = p + 2; // strlen("..")
						}
						else
						{
							q = p;
							while (isalnum(*q) || *q == '_')
							{
								q++;
							}
							if (*q != '\0')
							{
								log_error("Error menu item action in menu config line %d\n", fin_line);
								return -1;
							}
						}

						if (q - p > sizeof(p_item->action) - 1)
						{
							log_error("Too longer menu action in menu config line %d\n", fin_line);
							return -1;
						}
						strncpy(p_item->action, p, sizeof(p_item->action) - 1);
						p_item->action[sizeof(p_item->action) - 1] = '\0';

						// Menu item row
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu item row in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu item row in menu config line %d\n", fin_line);
							return -1;
						}
						p_item->row = atoi(p);

						// Menu item col
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu item col in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu item col in menu config line %d\n", fin_line);
							return -1;
						}
						p_item->col = atoi(p);

						// Menu item priv
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu item priv in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu item priv in menu config line %d\n", fin_line);
							return -1;
						}
						p_item->priv = atoi(p);

						// Menu item level
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu item level in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu item level in menu config line %d\n", fin_line);
							return -1;
						}
						p_item->level = atoi(p);

						// Menu item name
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL || *q != '\"')
						{
							log_error("Error menu item name in menu config line %d\n", fin_line);
							return -1;
						}
						q++;
						p = q;
						while (*q != '\0' && *q != '\"')
						{
							q++;
						}
						if (*q != '\"' || *(q + 1) != '\0')
						{
							log_error("Error menu item name in menu config line %d\n", fin_line);
							return -1;
						}
						*q = '\0';

						if (q - p > sizeof(p_item->name) - 1)
						{
							log_error("Too longer menu name in menu config line %d\n", fin_line);
							return -1;
						}
						strncpy(p_item->name, p, sizeof(p_item->name) - 1);
						p_item->name[sizeof(p_item->name) - 1] = '\0';

						// Menu item text
						q = strtok_r(NULL, MENU_CONF_DELIM_WITHOUT_SPACE, &saveptr);
						if (q == NULL || (q = strchr(q, '\"')) == NULL)
						{
							log_error("Error #1 menu item text in menu config line %d\n", fin_line);
							return -1;
						}
						q++;
						p = q;
						while (*q != '\0' && *q != '\"')
						{
							q++;
						}
						if (*q != '\"')
						{
							log_error("Error menu item text in menu config line %d\n", fin_line);
							return -1;
						}
						*q = '\0';

						if (q - p > sizeof(p_item->text) - 1)
						{
							log_error("Too longer menu item text in menu config line %d\n", fin_line);
							return -1;
						}
						strncpy(p_item->text, p, sizeof(p_item->text) - 1);
						p_item->text[sizeof(p_item->text) - 1] = '\0';

						// Check syntax
						q = strtok_r(q + 1, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q != NULL)
						{
							log_error("Unknown extra content in menu config line %d\n", fin_line);
							return -1;
						}
					}
					else if (strcmp(p, "title") == 0)
					{
						p_menu->title.show = 1;

						// Menu title row
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu title row in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu title row in menu config line %d\n", fin_line);
							return -1;
						}
						p_menu->title.row = atoi(p);

						// Menu title col
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu title col in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu title col in menu config line %d\n", fin_line);
							return -1;
						}
						p_menu->title.col = atoi(p);

						// Menu title text
						q = strtok_r(NULL, MENU_CONF_DELIM_WITHOUT_SPACE, &saveptr);
						if (q == NULL || (q = strchr(q, '\"')) == NULL)
						{
							log_error("Error menu title text in menu config line %d\n", fin_line);
							return -1;
						}
						q++;
						p = q;
						while (*q != '\0' && *q != '\"')
						{
							q++;
						}
						if (*q != '\"')
						{
							log_error("Error menu title text in menu config line %d\n", fin_line);
							return -1;
						}
						*q = '\0';

						if (q - p > sizeof(p_item->text) - 1)
						{
							log_error("Too longer menu title text in menu config line %d\n", fin_line);
							return -1;
						}
						strncpy(p_menu->title.text, p, sizeof(p_menu->title.text) - 1);
						p_menu->title.text[sizeof(p_menu->title.text) - 1] = '\0';

						// Check syntax
						q = strtok_r(q + 1, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q != NULL)
						{
							log_error("Unknown extra content in menu config line %d\n", fin_line);
							return -1;
						}
					}
					else if (strcmp(p, "screen") == 0)
					{
						p_menu->screen.show = 1;

						// Menu screen row
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu screen row in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu screen row in menu config line %d\n", fin_line);
							return -1;
						}
						p_menu->screen.row = atoi(p);

						// Menu screen col
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu screen col in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu screen col in menu config line %d\n", fin_line);
							return -1;
						}
						p_menu->screen.col = atoi(p);

						// Menu screen name
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu screen name in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isalnum(*q) || *q == '_')
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu screen name in menu config line %d\n", fin_line);
							return -1;
						}

						snprintf(p_menu->screen.filename, sizeof(p_menu->screen.filename), "%s%s", MENU_SCREEN_PATH_PREFIX, p);

						// Check syntax
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q != NULL)
						{
							log_error("Unknown extra content in menu config line %d\n", fin_line);
							return -1;
						}
					}
				}
			}
			else // BEGIN of menu screen
			{
				q = p;
				while (isalnum(*q) || *q == '_')
				{
					q++;
				}
				if (*q != '\0')
				{
					log_error("Error menu screen name in menu config line %d\n", fin_line);
					return -1;
				}

				snprintf(screen_filename, sizeof(screen_filename), "%s%s", MENU_SCREEN_PATH_PREFIX, p);

				// Check syntax
				q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
				if (q != NULL)
				{
					log_error("Unknown extra content in menu config line %d\n", fin_line);
					return -1;
				}

				if ((fout = fopen(screen_filename, "w")) == NULL)
				{
					log_error("Open %s failed", screen_filename);
					return -2;
				}

				while (fgets(buffer, sizeof(buffer), fin))
				{
					fin_line++;

					p = strtok_r(buffer, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
					if (p != NULL && *p == '%') // END of menu screen
					{
						break;
					}

					if (fputs(buffer, fout) < 0)
					{
						log_error("Write %s failed", screen_filename);
						return -2;
					}
				}

				fclose(fout);
			}
		}
		else // Invalid prefix
		{
			log_error("Error in menu config line %d\n", fin_line);
			return -1;
		}
	}
	fclose(fin);

	p_menu_set->menu_count = i;
	p_menu_set->menu_select_depth = 0;
	p_menu_set->p_menu_select[p_menu_set->menu_select_depth] = (i == 0 ? NULL : p_menu_set->p_menu[0]);

	return 0;
}

MENU *get_menu(MENU_SET *p_menu_set, const char *menu_name)
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
	outc(show ? '>' : ' ');
	iflush();
}

int display_menu(MENU *p_menu)
{
	int row = 0;
	int col = 0;
	int menu_selectable = 0;

	if (p_menu == NULL)
	{
		return -1;
	}

	if (p_menu->title.show)
	{
		show_top(p_menu->title.text);
	}

	if (p_menu->screen.show)
	{
		moveto(p_menu->screen.row, p_menu->screen.col);
		if (display_file(p_menu->screen.filename) != 0)
		{
			log_error("Display menu screen <%s> failed!\n",
					  p_menu->screen.filename);
		}
	}

	for (int i = 0; i < p_menu->item_count; i++)
	{
		if (p_menu->items[i]->row != 0)
		{
			row = p_menu->items[i]->row;
		}
		if (p_menu->items[i]->col != 0)
		{
			col = p_menu->items[i]->col;
		}

		if (checkpriv(&BBS_priv, 0, p_menu->items[i]->priv) == 0 || checklevel(&BBS_priv, p_menu->items[i]->level) == 0)
		{
			p_menu->items[i]->display = 0;
			p_menu->items[i]->r_row = 0;
			p_menu->items[i]->r_col = 0;
		}
		else
		{
			p_menu->items[i]->display = 1;

			if (!menu_selectable)
			{
				p_menu->item_cur_pos = i;
				menu_selectable = 1;
			}

			p_menu->items[i]->r_row = row;
			p_menu->items[i]->r_col = col;

			moveto(row, col);
			prints("%s", p_menu->items[i]->text);

			row++;
		}
	}

	if (!menu_selectable)
	{
		return -1;
	}

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
	{
		return 0;
	}

	p_menu = p_menu_set->p_menu_select[p_menu_set->menu_select_depth];

	switch (key)
	{
	case CR:
		igetch(1); // Cleanup remaining '\n' in the buffer
	case KEY_RIGHT:
		if (p_menu->items[p_menu->item_cur_pos]->submenu)
		{
			if (strcmp(p_menu->items[p_menu->item_cur_pos]->action, "..") == 0)
			{
				return menu_control(p_menu_set, KEY_LEFT);
			}
			p_menu_set->menu_select_depth++;
			p_menu = get_menu(p_menu_set, p_menu->items[p_menu->item_cur_pos]->action);
			p_menu_set->p_menu_select[p_menu_set->menu_select_depth] = p_menu;

			if (display_menu(p_menu) != 0)
			{
				return menu_control(p_menu_set, KEY_LEFT);
			}
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
			{
				return menu_control(p_menu_set, KEY_LEFT);
			}
			break;
		}
		else
		{
			display_menu_cursor(p_menu, 0);
			p_menu->item_cur_pos = p_menu->item_count - 1;
			while (!p_menu->items[p_menu->item_cur_pos]->display ||
				   p_menu->items[p_menu->item_cur_pos]->priv != 0 ||
				   p_menu->items[p_menu->item_cur_pos]->level != 0)
			{
				p_menu->item_cur_pos--;
			}
			display_menu_cursor(p_menu, 1);
			break;
		}
	case KEY_UP:
		display_menu_cursor(p_menu, 0);
		do
		{
			p_menu->item_cur_pos--;
			if (p_menu->item_cur_pos < 0)
			{
				p_menu->item_cur_pos = p_menu->item_count - 1;
			}
		} while (!p_menu->items[p_menu->item_cur_pos]->display);
		display_menu_cursor(p_menu, 1);
		break;
	case KEY_DOWN:
		display_menu_cursor(p_menu, 0);
		do
		{
			p_menu->item_cur_pos++;
			if (p_menu->item_cur_pos >= p_menu->item_count)
			{
				p_menu->item_cur_pos = 0;
			}
		} while (!p_menu->items[p_menu->item_cur_pos]->display);
		display_menu_cursor(p_menu, 1);
		break;
	default:
		if (isalnum(key))
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
	int i, j;

	for (i = 0; i < p_menu_set->menu_count; i++)
	{
		p_menu = p_menu_set->p_menu[i];
		for (j = 0; j < p_menu->item_count; j++)
		{
			free(p_menu->items[j]);
		}
		free(p_menu);
	}

	p_menu_set->menu_count = 0;
	p_menu_set->menu_select_depth = 0;
}

int reload_menu(MENU_SET *p_menu_set)
{
	int result;
	char conf_file[FILE_PATH_LEN];

	strncpy(conf_file, p_menu_set->conf_file, sizeof(conf_file) - 1);
	conf_file[sizeof(conf_file) - 1] = '\0';

	unload_menu(p_menu_set);
	result = load_menu(p_menu_set, conf_file);

	return result;
}
