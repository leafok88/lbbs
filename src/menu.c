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
#include "bbs_cmd.h"
#include "common.h"
#include "io.h"
#include "log.h"
#include "menu.h"
#include "screen.h"
#include "user_priv.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MENU_SCREEN_PATH_PREFIX "var/MENU_SCR_"
#define MENU_CONF_DELIM_WITH_SPACE " ,\t\r\n"
#define MENU_CONF_DELIM_WITHOUT_SPACE "\r\n"

#define MENU_SET_RESERVED_LENGTH (sizeof(int16_t) * 4)

MENU_SET *p_bbs_menu;

int load_menu(MENU_SET *p_menu_set, const char *conf_file)
{
	FILE *fin;
	int fin_line = 0;
	char buffer[LINE_BUFFER_LEN];
	char temp[LINE_BUFFER_LEN];
	char *p = NULL;
	char *q = NULL;
	char *r = NULL;
	char *saveptr = NULL;
	MENU *p_menu = NULL;
	MENU_ITEM *p_menu_item = NULL;
	MENU_SCREEN *p_screen = NULL;
	MENU_ID menu_id;
	MENU_ITEM_ID menu_item_id;
	MENU_SCREEN_ID screen_id;
	int proj_id;
	key_t key;
	size_t size;

	// Use trie_dict to search menu_id by menu name
	p_menu_set->p_menu_name_dict = trie_dict_create();
	if (p_menu_set->p_menu_name_dict == NULL)
	{
		log_error("trie_dict_create() error\n");
		return -3;
	}

	// Use trie_dict to search screen_id by menu screen name
	p_menu_set->p_menu_screen_dict = trie_dict_create();
	if (p_menu_set->p_menu_screen_dict == NULL)
	{
		log_error("trie_dict_create() error\n");
		return -3;
	}

	if ((fin = fopen(conf_file, "r")) == NULL)
	{
		log_error("Open %s failed", conf_file);
		return -2;
	}

	// Allocate shared memory
	proj_id = (int)(time(NULL) % getpid());
	key = ftok(conf_file, proj_id);
	if (key == -1)
	{
		log_error("ftok(%s %d) error (%d)\n", conf_file, proj_id, errno);
		return -2;
	}

	size = MENU_SET_RESERVED_LENGTH +
		   sizeof(MENU) * MAX_MENUS +
		   sizeof(MENU_ITEM) * MAX_MENUITEMS +
		   sizeof(MENU_SCREEN) * MAX_MENUS +
		   MAX_MENU_SCR_BUF_LENGTH * MAX_MENUS;
	p_menu_set->shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
	if (p_menu_set->shmid == -1)
	{
		log_error("shmget(size = %d) error (%d)\n", size, errno);
		return -3;
	}
	p_menu_set->p_reserved = shmat(p_menu_set->shmid, NULL, 0);
	if (p_menu_set->p_reserved == (void *)-1)
	{
		log_error("shmat() error (%d)\n", errno);
		return -3;
	}
	p_menu_set->p_menu_pool = p_menu_set->p_reserved + MENU_SET_RESERVED_LENGTH;
	p_menu_set->p_menu_item_pool = p_menu_set->p_menu_pool + sizeof(MENU) * MAX_MENUS;
	p_menu_set->p_menu_screen_pool = p_menu_set->p_menu_item_pool + sizeof(MENU_ITEM) * MAX_MENUITEMS;
	p_menu_set->p_menu_screen_buf = p_menu_set->p_menu_screen_pool + sizeof(MENU_SCREEN) * MAX_MENUS;
	p_menu_set->p_menu_screen_buf_free = p_menu_set->p_menu_screen_buf;

	p_menu_set->menu_count = 0;
	p_menu_set->menu_item_count = 0;
	p_menu_set->menu_screen_count = 0;
	p_menu_set->choose_step = 0;
	p_menu_set->menu_id_path[0] = 0;

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
					log_error("Incomplete menu definition in menu config line %d\n", fin_line);
					return -1;
				}

				if (p_menu_set->menu_count >= MAX_MENUS)
				{
					log_error("Menu count (%d) exceed limit (%d)\n", p_menu_set->menu_count, MAX_MENUS);
					return -3;
				}
				menu_id = (MENU_ID)p_menu_set->menu_count;
				p_menu_set->menu_count++;

				p_menu = get_menu_by_id(p_menu_set, menu_id);

				p_menu->item_count = 0;
				p_menu->title.show = 0;
				p_menu->screen_show = 0;
				p_menu->page_item_limit = 0;
				p_menu->use_filter = 0;
				p_menu->filter_handler = NULL;

				q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
				if (q == NULL)
				{
					log_error("Error menu name in menu config line %d\n", fin_line);
					return -1;
				}
				p = q;
				while (isalnum(*q) || *q == '_' || *q == '-')
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

				if (trie_dict_set(p_menu_set->p_menu_name_dict, p_menu->name, (int64_t)menu_id) != 1)
				{
					log_error("Error set menu dict [%s]\n", p_menu->name);
				}

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
						if (p_menu->item_count >= MAX_ITEMS_PER_MENU)
						{
							log_error("Menuitem count per menu (%d) exceed limit (%d)\n", p_menu->item_count, MAX_ITEMS_PER_MENU);
							return -1;
						}
						if (p_menu_set->menu_item_count >= MAX_MENUITEMS)
						{
							log_error("Menuitem count (%d) exceed limit (%d)\n", p_menu_set->menu_item_count, MAX_MENUITEMS);
							return -3;
						}
						menu_item_id = (MENU_ITEM_ID)p_menu_set->menu_item_count;
						p_menu_set->menu_item_count++;

						p_menu->items[p_menu->item_count] = menu_item_id;
						p_menu->item_count++;

						p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);

						p_menu_item->submenu = (*p == '!' ? 1 : 0);

						// Menu item action
						p++;
						if (strcmp(p, "..") == 0) // Return to parent menu
						{
							q = p + 2; // strlen("..")
						}
						else
						{
							q = p;
							while (isalnum(*q) || *q == '_' || *q == '-')
							{
								q++;
							}
							if (*q != '\0')
							{
								log_error("Error menu item action in menu config line %d\n", fin_line);
								return -1;
							}
						}

						if (q - p > sizeof(p_menu_item->action) - 1)
						{
							log_error("Too longer menu action in menu config line %d\n", fin_line);
							return -1;
						}
						strncpy(p_menu_item->action, p, sizeof(p_menu_item->action) - 1);
						p_menu_item->action[sizeof(p_menu_item->action) - 1] = '\0';

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
						p_menu_item->row = (int16_t)atoi(p);

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
						p_menu_item->col = (int16_t)atoi(p);

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
						p_menu_item->priv = atoi(p);

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
						p_menu_item->level = atoi(p);

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
							if (*q == '\\')
							{
								r = q;
								while (*r != '\0')
								{
									*r = *(r + 1);
									r++;
								}
							}
							q++;
						}
						if (*q != '\"' || *(q + 1) != '\0')
						{
							log_error("Error menu item name in menu config line %d\n", fin_line);
							return -1;
						}
						*q = '\0';

						if (q - p > sizeof(p_menu_item->name) - 1)
						{
							log_error("Too longer menu name in menu config line %d\n", fin_line);
							return -1;
						}
						strncpy(p_menu_item->name, p, sizeof(p_menu_item->name) - 1);
						p_menu_item->name[sizeof(p_menu_item->name) - 1] = '\0';

						// Menu item text
						q = strtok_r(NULL, MENU_CONF_DELIM_WITHOUT_SPACE, &saveptr);
						if (q == NULL || (q = strchr(q, '\"')) == NULL)
						{
							log_error("Error menu item text in menu config line %d\n", fin_line);
							return -1;
						}
						q++;
						p = q;
						while (*q != '\0' && *q != '\"')
						{
							if (*q == '\\')
							{
								r = q;
								while (*r != '\0')
								{
									*r = *(r + 1);
									r++;
								}
							}
							q++;
						}
						if (*q != '\"')
						{
							log_error("Error menu item text in menu config line %d\n", fin_line);
							return -1;
						}
						*q = '\0';

						if (q - p > sizeof(p_menu_item->text) - 1)
						{
							log_error("Too longer menu item text in menu config line %d\n", fin_line);
							return -1;
						}
						strncpy(p_menu_item->text, p, sizeof(p_menu_item->text) - 1);
						p_menu_item->text[sizeof(p_menu_item->text) - 1] = '\0';

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
						p_menu->title.row = (int16_t)atoi(p);

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
						p_menu->title.col = (int16_t)atoi(p);

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
							if (*q == '\\')
							{
								r = q;
								while (*r != '\0')
								{
									*r = *(r + 1);
									r++;
								}
							}
							q++;
						}
						if (*q != '\"')
						{
							log_error("Error menu title text in menu config line %d\n", fin_line);
							return -1;
						}
						*q = '\0';

						if (q - p > sizeof(p_menu->title.text) - 1)
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
						p_menu->screen_show = 1;

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
						p_menu->screen_row = (int16_t)atoi(p);

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
						p_menu->screen_col = (int16_t)atoi(p);

						// Menu screen name
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu screen name in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isalnum(*q) || *q == '_' || *q == '-')
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu screen name in menu config line %d\n", fin_line);
							return -1;
						}
						strncpy(p_menu->screen_name, p, sizeof(p_menu->screen_name) - 1);
						p_menu->screen_name[sizeof(p_menu->screen_name) - 1] = '\0';

						// Check syntax
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q != NULL)
						{
							log_error("Unknown extra content in menu config line %d\n", fin_line);
							return -1;
						}
					}
					else if (strcmp(p, "page") == 0)
					{
						// Menu page row
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu page row in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu page row in menu config line %d\n", fin_line);
							return -1;
						}
						p_menu->page_row = (int16_t)atoi(p);

						// Menu page col
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu page col in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu page col in menu config line %d\n", fin_line);
							return -1;
						}
						p_menu->page_col = (int16_t)atoi(p);

						// Menu page item limit
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q == NULL)
						{
							log_error("Error menu page item limit in menu config line %d\n", fin_line);
							return -1;
						}
						p = q;
						while (isdigit(*q))
						{
							q++;
						}
						if (*q != '\0')
						{
							log_error("Error menu page item limit in menu config line %d\n", fin_line);
							return -1;
						}
						p_menu->page_item_limit = (int16_t)atoi(p);

						// Check syntax
						q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
						if (q != NULL)
						{
							log_error("Unknown extra content in menu config line %d\n", fin_line);
							return -1;
						}
					}
					else if (strcmp(p, "use_filter") == 0)
					{
						p_menu->use_filter = 1;

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
				if (p_menu_set->menu_item_count >= MAX_MENUS)
				{
					log_error("Menu screen count (%d) exceed limit (%d)\n", p_menu_set->menu_screen_count, MAX_MENUS);
					return -3;
				}
				screen_id = (MENU_SCREEN_ID)p_menu_set->menu_screen_count;
				p_menu_set->menu_screen_count++;

				p_screen = get_menu_screen_by_id(p_menu_set, screen_id);

				q = p;
				while (isalnum(*q) || *q == '_' || *q == '-')
				{
					q++;
				}
				if (*q != '\0')
				{
					log_error("Error menu screen name in menu config line %d\n", fin_line);
					return -1;
				}
				strncpy(p_screen->name, p, sizeof(p_screen->name) - 1);
				p_screen->name[sizeof(p_screen->name) - 1] = '\0';

				if (trie_dict_set(p_menu_set->p_menu_screen_dict, p_screen->name, (int64_t)screen_id) != 1)
				{
					log_error("Error set menu screen dict [%s]\n", p_screen->name);
				}

				// Check syntax
				q = strtok_r(NULL, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
				if (q != NULL)
				{
					log_error("Unknown extra content in menu config line %d\n", fin_line);
					return -1;
				}

				p_screen->buf_offset = p_menu_set->p_menu_screen_buf_free - p_menu_set->p_menu_screen_buf;
				p_screen->buf_length = -1;

				// safety appending boundary
				q = p_menu_set->p_menu_screen_buf + MAX_MENU_SCR_BUF_LENGTH * MAX_MENUS - 1;

				while (fgets(buffer, sizeof(buffer), fin))
				{
					fin_line++;

					strncpy(temp, buffer, sizeof(temp) - 1); // Duplicate line for strtok_r
					temp[sizeof(temp) - 1] = '\0';
					p = strtok_r(temp, MENU_CONF_DELIM_WITH_SPACE, &saveptr);
					if (p != NULL && *p == '%') // END of menu screen
					{
						if (p_menu_set->p_menu_screen_buf_free + 1 > q)
						{
							log_error("Menu screen buffer depleted (%p + 1 > %p)\n", p_menu_set->p_menu_screen_buf_free, q);
							return -3;
						}

						*(p_menu_set->p_menu_screen_buf_free) = '\0';
						p_menu_set->p_menu_screen_buf_free++;
						p_screen->buf_length = p_menu_set->p_menu_screen_buf_free - p_menu_set->p_menu_screen_buf - p_screen->buf_offset;
						break;
					}

					// Clear line
					if (p_menu_set->p_menu_screen_buf_free + strlen(CTRL_SEQ_CLR_LINE) > q)
					{
						log_error("Menu screen buffer depleted (%p + %d > %p)\n", p_menu_set->p_menu_screen_buf_free, q, strlen(CTRL_SEQ_CLR_LINE));
						return -3;
					}
					p_menu_set->p_menu_screen_buf_free = stpcpy(p_menu_set->p_menu_screen_buf_free, CTRL_SEQ_CLR_LINE);

					p = buffer;
					while (*p != '\0')
					{
						if (p_menu_set->p_menu_screen_buf_free + 2 > q)
						{
							log_error("Menu screen buffer depleted (%p + 2 > %p)\n", p_menu_set->p_menu_screen_buf_free, q);
							return -3;
						}

						if (*p == '\n' && p > buffer && *(p - 1) != '\r')
						{
							*(p_menu_set->p_menu_screen_buf_free) = '\r';
							p_menu_set->p_menu_screen_buf_free++;
						}

						*(p_menu_set->p_menu_screen_buf_free) = *p;
						p++;
						p_menu_set->p_menu_screen_buf_free++;
					}
				}

				if (p_screen->buf_length == -1)
				{
					log_error("End of menu screen [%s] not found\n", p_screen->name);
				}
			}
		}
		else // Invalid prefix
		{
			log_error("Error in menu config line %d\n", fin_line);
			return -1;
		}
	}
	fclose(fin);

	for (menu_id = 0; menu_id < p_menu_set->menu_count; menu_id++)
	{
		p_menu = get_menu_by_id(p_menu_set, menu_id);

		if (trie_dict_get(p_menu_set->p_menu_screen_dict, p_menu->screen_name, (int64_t *)(&(p_menu->screen_id))) != 1)
		{
			log_error("Undefined menu screen [%s]\n", p);
			return -1;
		}

		// Set menu->filter_handler of each menu pointing to filter
		if (p_menu->use_filter == 1)
		{
			if ((p_menu->filter_handler = get_cmd_handler(p_menu->name)) == NULL)
			{
				log_error("Undefined menu filter handler [%s]\n", p_menu->name);
				return -1;
			}
		}
	}

	for (menu_item_id = 0; menu_item_id < p_menu_set->menu_item_count; menu_item_id++)
	{
		p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);

		// Set menu_item->action_cmd_handler of each menu item pointing to bbs_cmd
		if (p_menu_item->submenu == 0)
		{
			if ((p_menu_item->action_cmd_handler = get_cmd_handler(p_menu_item->action)) == NULL)
			{
				log_error("Undefined menu action cmd handler [%s]\n", p_menu_item->action);
				return -1;
			}
		}
		// Set menu_item->action_menu_id of each menu item pointing to a submenu to the menu_id of the corresponding submenu
		else if (strcmp(p_menu_item->action, "..") != 0)
		{
			if (trie_dict_get(p_menu_set->p_menu_name_dict, p_menu_item->action, (int64_t *)&menu_id) != 1)
			{
				log_error("Undefined sub menu id [%s]\n", p_menu_item->action);
				return -1;
			}
			p_menu_item->action_menu_id = menu_id;
		}
	}

	// Save status varaibles into reserved memory area
	*((int16_t *)p_menu_set->p_reserved) = p_menu_set->menu_count;
	*(((int16_t *)p_menu_set->p_reserved) + 1) = p_menu_set->menu_item_count;
	*(((int16_t *)p_menu_set->p_reserved) + 2) = p_menu_set->menu_screen_count;

	return 0;
}

int display_menu_cursor(MENU_SET *p_menu_set, int show)
{
	MENU_ID menu_id;
	MENU_ITEM_ID menu_item_id;
	MENU *p_menu;
	MENU_ITEM *p_menu_item;
	int16_t menu_item_pos;

	menu_id = p_menu_set->menu_id_path[p_menu_set->choose_step];
	p_menu = get_menu_by_id(p_menu_set, menu_id);
	if (p_menu == NULL)
	{
		log_error("get_menu_by_id(%d) return NULL pointer\n", menu_id);
		return -1;
	}

	menu_item_pos = p_menu_set->menu_item_pos[p_menu_set->choose_step];
	menu_item_id = p_menu->items[menu_item_pos];
	p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);
	if (p_menu_item == NULL)
	{
		log_error("get_menu_item_by_id(%d) return NULL pointer\n", menu_item_id);
		return -1;
	}

	moveto(p_menu_set->menu_item_r_row[menu_item_pos], p_menu_set->menu_item_r_col[menu_item_pos] - 2);
	outc(show ? '>' : ' ');

	return 0;
}

static int display_menu_current_page(MENU_SET *p_menu_set)
{
	int16_t row = 0;
	int16_t col = 0;
	MENU_ID menu_id;
	MENU_ITEM_ID menu_item_id;
	MENU *p_menu;
	MENU_ITEM *p_menu_item;
	int16_t menu_item_pos;
	int16_t page_id = 0;
	MENU_SCREEN *p_menu_screen;

	menu_id = p_menu_set->menu_id_path[p_menu_set->choose_step];
	p_menu = get_menu_by_id(p_menu_set, menu_id);
	if (p_menu == NULL)
	{
		log_error("get_menu_by_id(%d) return NULL pointer\n", menu_id);
		return -1;
	}

	clrline(p_menu->page_row, p_menu->page_row + p_menu->page_item_limit - 1);

	if (p_menu->title.show)
	{
		if (p_menu->title.row == 0 && p_menu->title.col == 0)
		{
			show_top(p_menu->title.text, BBS_name, "");
		}
		else
		{
			moveto(p_menu->title.row, p_menu->title.col);
			prints("%s", p_menu->title.text);
		}
	}

	if (p_menu->screen_show)
	{
		p_menu_screen = get_menu_screen_by_id(p_menu_set, p_menu->screen_id);
		if (p_menu_screen == NULL)
		{
			log_error("get_menu_screen_by_id(%d) return NULL pointer\n", p_menu->screen_id);
			return -1;
		}

		row = p_menu->screen_row;
		col = p_menu->screen_col;

		moveto(row, col);
		prints("%s", p_menu_set->p_menu_screen_buf + p_menu_screen->buf_offset);
	}

	menu_item_pos = p_menu_set->menu_item_pos[p_menu_set->choose_step];
	page_id = p_menu_set->menu_item_page_id[menu_item_pos];

	while (menu_item_pos >= 0)
	{
		if (p_menu_set->menu_item_page_id[menu_item_pos] != page_id)
		{
			menu_item_pos++;
			break;
		}

		if (menu_item_pos == 0)
		{
			break;
		}

		menu_item_pos--;
	}

	for (; menu_item_pos < p_menu->item_count; menu_item_pos++)
	{
		if (p_menu_set->menu_item_page_id[menu_item_pos] != page_id)
		{
			break;
		}

		menu_item_id = p_menu->items[menu_item_pos];
		p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);

		if (p_menu_set->menu_item_display[menu_item_pos] == 0)
		{
			continue;
		}

		row = p_menu_set->menu_item_r_row[menu_item_pos];
		col = p_menu_set->menu_item_r_col[menu_item_pos];

		moveto(row, col);
		prints("%s", p_menu_item->text);
	}

	return 0;
}

int display_menu(MENU_SET *p_menu_set)
{
	int16_t row = 0;
	int16_t col = 0;
	int menu_selectable = 0;
	MENU_ID menu_id;
	MENU_ITEM_ID menu_item_id;
	MENU *p_menu;
	MENU_ITEM *p_menu_item;
	int16_t menu_item_pos;
	int16_t page_id = 0;
	int page_item_count = 0;

	menu_id = p_menu_set->menu_id_path[p_menu_set->choose_step];
	p_menu = get_menu_by_id(p_menu_set, menu_id);
	if (p_menu == NULL)
	{
		log_error("get_menu_by_id(%d) return NULL pointer\n", menu_id);
		if (p_menu_set->choose_step > 0)
		{
			p_menu_set->choose_step--;
			return REDRAW;
		}
		return EXITBBS;
	}

	menu_item_pos = p_menu_set->menu_item_pos[p_menu_set->choose_step];
	menu_item_id = p_menu->items[menu_item_pos];
	p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);
	if (p_menu_item == NULL)
	{
		log_error("get_menu_item_by_id(%d) return NULL pointer\n", menu_item_id);
		menu_item_pos = 0;
	}

	if (menu_item_pos > 0 &&
		!(p_menu->use_filter ? (p_menu->filter_handler((void *)p_menu_item) == 0)
							 : (checkpriv(&BBS_priv, 0, p_menu_item->priv) == 0 ||
								checklevel2(&BBS_priv, p_menu_item->level) == 0)))
	{
		menu_selectable = 1;
	}

	for (menu_item_pos = 0; menu_item_pos < p_menu->item_count; menu_item_pos++)
	{
		menu_item_id = p_menu->items[menu_item_pos];
		p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);

		if (p_menu_item->row != 0)
		{
			row = p_menu_item->row;
		}
		if (p_menu_item->col != 0)
		{
			col = p_menu_item->col;
		}

		p_menu_set->menu_item_page_id[menu_item_pos] = page_id;

		if (p_menu->use_filter ? (p_menu->filter_handler((void *)p_menu_item) == 0)
							   : (checkpriv(&BBS_priv, 0, p_menu_item->priv) == 0 ||
								  checklevel2(&BBS_priv, p_menu_item->level) == 0))
		{
			p_menu_set->menu_item_display[menu_item_pos] = 0;
			p_menu_set->menu_item_r_row[menu_item_pos] = 0;
			p_menu_set->menu_item_r_col[menu_item_pos] = 0;
		}
		else
		{
			p_menu_set->menu_item_display[menu_item_pos] = 1;

			if (!menu_selectable)
			{
				p_menu_set->menu_item_pos[p_menu_set->choose_step] = menu_item_pos;
				menu_selectable = 1;
			}

			p_menu_set->menu_item_r_row[menu_item_pos] = row;
			p_menu_set->menu_item_r_col[menu_item_pos] = col;

			row++;

			page_item_count++;
			if (p_menu->page_item_limit > 0 && page_item_count >= p_menu->page_item_limit)
			{
				page_id++;
				page_item_count = 0;
				row = p_menu->page_row;
				col = p_menu->page_col;
			}
		}
	}

	if (!menu_selectable)
	{
		moveto(p_menu->screen_row, p_menu->screen_col);
		clrtoeol();
		prints("没有可选项");
		press_any_key();
		return -1;
	}

	if (display_menu_current_page(p_menu_set) != 0)
	{
		return -1;
	}

	display_menu_cursor(p_menu_set, 1);

	return 0;
}

int menu_control(MENU_SET *p_menu_set, int key)
{
	MENU_ID menu_id;
	MENU_ITEM_ID menu_item_id;
	MENU *p_menu;
	MENU_ITEM *p_menu_item;
	int16_t menu_item_pos;
	int16_t page_id;
	int require_page_change = 0;

	if (p_menu_set->menu_count == 0)
	{
		log_error("Empty menu set\n");
		return EXITBBS;
	}

	menu_id = p_menu_set->menu_id_path[p_menu_set->choose_step];
	p_menu = get_menu_by_id(p_menu_set, menu_id);
	if (p_menu == NULL)
	{
		log_error("get_menu_by_id(%d) return NULL pointer\n", menu_id);
		if (p_menu_set->choose_step > 0)
		{
			p_menu_set->choose_step--;
			return REDRAW;
		}
		return EXITBBS;
	}

	if (p_menu->item_count == 0)
	{
		log_error("Empty menu (%s)\n", p_menu->name);
		if (p_menu_set->choose_step > 0)
		{
			p_menu_set->choose_step--;
			return REDRAW;
		}
		return EXITBBS;
	}

	menu_item_pos = p_menu_set->menu_item_pos[p_menu_set->choose_step];
	page_id = p_menu_set->menu_item_page_id[menu_item_pos];

	menu_item_id = p_menu->items[menu_item_pos];
	p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);
	if (p_menu_item == NULL)
	{
		log_error("get_menu_item_by_id(%d) return NULL pointer\n", menu_item_id);
		p_menu_set->menu_item_pos[p_menu_set->choose_step] = 0;
		return REDRAW;
	}

	switch (key)
	{
	case CR:
		igetch_reset();
	case KEY_RIGHT:
		if (p_menu_item->submenu)
		{
			if (strcmp(p_menu_item->action, "..") == 0)
			{
				return menu_control(p_menu_set, KEY_LEFT);
			}
			p_menu_set->choose_step++;
			p_menu_set->menu_id_path[p_menu_set->choose_step] = p_menu_item->action_menu_id;
			p_menu_set->menu_item_pos[p_menu_set->choose_step] = 0;

			if (display_menu(p_menu_set) != 0)
			{
				return menu_control(p_menu_set, KEY_LEFT);
			}
		}
		else
		{
			return ((*(p_menu_item->action_cmd_handler))((void *)(p_menu_item->name)));
		}
		break;
	case KEY_ESC:
	case KEY_LEFT:
		if (p_menu_set->choose_step > 0)
		{
			p_menu_set->choose_step--;
			if (p_menu_set->choose_step == 0)
			{
				return REDRAW;
			}
			if (display_menu(p_menu_set) != 0)
			{
				return menu_control(p_menu_set, KEY_LEFT);
			}
		}
		else
		{
			display_menu_cursor(p_menu_set, 0);
			menu_item_pos = p_menu->item_count - 1;
			while (menu_item_pos >= 0)
			{
				menu_item_id = p_menu->items[menu_item_pos];
				p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);
				if (p_menu_item == NULL)
				{
					log_error("get_menu_item_by_id(%d) return NULL pointer\n", menu_item_id);
					return -1;
				}

				if (!p_menu_set->menu_item_display[menu_item_pos] || p_menu_item->priv != 0 || p_menu_item->level != 0)
				{
					menu_item_pos--;
				}
				else
				{
					break;
				}
			}
			p_menu_set->menu_item_pos[p_menu_set->choose_step] = menu_item_pos;
			display_menu_cursor(p_menu_set, 1);
		}
		break;
	case KEY_PGUP:
		require_page_change = 1;
	case KEY_UP:
		display_menu_cursor(p_menu_set, 0);
		do
		{
			menu_item_pos--;
			if (menu_item_pos < 0)
			{
				menu_item_pos = p_menu->item_count - 1;
				require_page_change = 0;
			}
			menu_item_id = p_menu->items[menu_item_pos];
			p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);
			if (p_menu_item == NULL)
			{
				log_error("get_menu_item_by_id(%d) return NULL pointer\n", menu_item_id);
				return -1;
			}
			if (require_page_change && p_menu_set->menu_item_page_id[menu_item_pos] != page_id)
			{
				require_page_change = 0;
			}
		} while (require_page_change || !p_menu_set->menu_item_display[menu_item_pos]);
		p_menu_set->menu_item_pos[p_menu_set->choose_step] = menu_item_pos;
		if (p_menu_set->menu_item_page_id[menu_item_pos] != page_id)
		{
			display_menu_current_page(p_menu_set);
		}
		display_menu_cursor(p_menu_set, 1);
		break;
	case KEY_PGDN:
		require_page_change = 1;
	case KEY_DOWN:
		display_menu_cursor(p_menu_set, 0);
		do
		{
			menu_item_pos++;
			if (menu_item_pos >= p_menu->item_count)
			{
				menu_item_pos = 0;
				require_page_change = 0;
			}
			menu_item_id = p_menu->items[menu_item_pos];
			p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);
			if (p_menu_item == NULL)
			{
				log_error("get_menu_item_by_id(%d) return NULL pointer\n", menu_item_id);
				return -1;
			}
			if (require_page_change && p_menu_set->menu_item_page_id[menu_item_pos] != page_id)
			{
				require_page_change = 0;
			}
		} while (require_page_change || !p_menu_set->menu_item_display[menu_item_pos]);
		p_menu_set->menu_item_pos[p_menu_set->choose_step] = menu_item_pos;
		if (p_menu_set->menu_item_page_id[menu_item_pos] != page_id)
		{
			display_menu_current_page(p_menu_set);
		}
		display_menu_cursor(p_menu_set, 1);
		break;
	case KEY_HOME:
		display_menu_cursor(p_menu_set, 0);
		menu_item_pos = 0;
		while (menu_item_pos < p_menu->item_count - 1)
		{
			menu_item_id = p_menu->items[menu_item_pos];
			p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);
			if (p_menu_item == NULL)
			{
				log_error("get_menu_item_by_id(%d) return NULL pointer\n", menu_item_id);
				return -1;
			}

			if (p_menu_set->menu_item_display[menu_item_pos])
			{
				break;
			}

			menu_item_pos++;
		}
		p_menu_set->menu_item_pos[p_menu_set->choose_step] = menu_item_pos;
		if (p_menu_set->menu_item_page_id[menu_item_pos] != page_id)
		{
			display_menu_current_page(p_menu_set);
		}
		display_menu_cursor(p_menu_set, 1);
		break;
	case KEY_END:
		display_menu_cursor(p_menu_set, 0);
		menu_item_pos = p_menu->item_count - 1;
		while (menu_item_pos > 0)
		{
			menu_item_id = p_menu->items[menu_item_pos];
			p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);
			if (p_menu_item == NULL)
			{
				log_error("get_menu_item_by_id(%d) return NULL pointer\n", menu_item_id);
				return -1;
			}

			if (p_menu_set->menu_item_display[menu_item_pos])
			{
				break;
			}

			menu_item_pos--;
		}
		p_menu_set->menu_item_pos[p_menu_set->choose_step] = menu_item_pos;
		if (p_menu_set->menu_item_page_id[menu_item_pos] != page_id)
		{
			display_menu_current_page(p_menu_set);
		}
		display_menu_cursor(p_menu_set, 1);
		break;
	default:
		if (isalnum(key))
		{
			for (menu_item_pos = 0; menu_item_pos < p_menu->item_count; menu_item_pos++)
			{
				menu_item_id = p_menu->items[menu_item_pos];
				p_menu_item = get_menu_item_by_id(p_menu_set, menu_item_id);
				if (p_menu_item == NULL)
				{
					log_error("get_menu_item_by_id(%d) return NULL pointer\n", menu_item_id);
					return -1;
				}

				if (toupper(key) == toupper(p_menu_item->name[0]) && p_menu_set->menu_item_display[menu_item_pos])
				{
					display_menu_cursor(p_menu_set, 0);
					p_menu_set->menu_item_pos[p_menu_set->choose_step] = menu_item_pos;
					if (p_menu_set->menu_item_page_id[menu_item_pos] != page_id)
					{
						display_menu_current_page(p_menu_set);
					}
					display_menu_cursor(p_menu_set, 1);
					break;
				}
			}
		}
		break;
	}

	return NOREDRAW;
}

int unload_menu(MENU_SET *p_menu_set)
{
	int shmid;

	if (p_menu_set == NULL)
	{
		return -1;
	}

	if (p_menu_set->p_menu_name_dict != NULL)
	{
		trie_dict_destroy(p_menu_set->p_menu_name_dict);
		p_menu_set->p_menu_name_dict = NULL;
	}

	if (p_menu_set->p_menu_screen_dict != NULL)
	{
		trie_dict_destroy(p_menu_set->p_menu_screen_dict);
		p_menu_set->p_menu_screen_dict = NULL;
	}

	shmid = p_menu_set->shmid;

	detach_menu_shm(p_menu_set);

	if (shmctl(shmid, IPC_RMID, NULL) == -1)
	{
		log_error("shmctl(shmid=%d, IPC_RMID) error (%d)\n", shmid, errno);
		return -1;
	}

	return 0;
}

int set_menu_shm_readonly(MENU_SET *p_menu_set)
{
	void *p_shm;

	// Remap shared memory in read-only mode
	p_shm = shmat(p_menu_set->shmid, p_menu_set->p_reserved, SHM_RDONLY | SHM_REMAP);
	if (p_shm == (void *)-1)
	{
		log_error("shmat(menu_shm shmid = %d) error (%d)\n", p_menu_set->shmid, errno);
		return -1;
	}

	p_menu_set->p_reserved = p_shm;

	return 0;
}

int detach_menu_shm(MENU_SET *p_menu_set)
{
	p_menu_set->menu_count = 0;
	p_menu_set->menu_item_count = 0;
	p_menu_set->menu_screen_count = 0;
	p_menu_set->choose_step = 0;

	p_menu_set->p_menu_pool = NULL;
	p_menu_set->p_menu_item_pool = NULL;
	p_menu_set->p_menu_screen_pool = NULL;
	p_menu_set->p_menu_screen_buf = NULL;
	p_menu_set->p_menu_screen_buf_free = NULL;

	p_menu_set->p_menu_name_dict = NULL;
	p_menu_set->p_menu_screen_dict = NULL;

	if (p_menu_set->p_reserved != NULL && shmdt(p_menu_set->p_reserved) == -1)
	{
		log_error("shmdt() error (%d)\n", errno);
		return -1;
	}

	p_menu_set->p_reserved = NULL;

	return 0;
}
