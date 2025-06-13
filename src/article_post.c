/***************************************************************************
						article_post.c  -  description
							 -------------------
	copyright            : (C) 2004-2025 by Leaflet
	email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "article_post.h"
#include "article_cache.h"
#include "editor.h"
#include "screen.h"
#include "log.h"
#include "io.h"
#include <ctype.h>
#include <string.h>

#define STR_INPUT_LEN 74

int article_post(SECTION_LIST *p_section)
{
	EDITOR_DATA *p_editor_data;
	char title[BBS_article_title_max_len + 1] = "";
	char title_input[STR_INPUT_LEN + 1] = "";
	int sign_id = 0;
	long len;
	int ch;
	char *p, *q;

	if (p_section == NULL)
	{
		log_error("NULL pointer error\n");
	}

	p_editor_data = editor_data_load("");
	if (p_editor_data == NULL)
	{
		log_error("editor_data_load() error\n");
		return -2;
	}

	// Set title and sign
	for (ch = 'T'; !SYS_server_exit;)
	{
		clearscr();
		moveto(21, 1);
		prints("·¢±íÎÄÕÂÓÚ %s[%s] ÌÖÂÛÇø", p_section->stitle, p_section->sname);
		moveto(22, 1);
		prints("Ê¹ÓÃ±êÌâ: %s", (title[0] == '\0' ? "[ÕýÔÚÉè¶¨Ö÷Ìâ]" : title));
		moveto(23, 1);
		prints("Ê¹ÓÃµÚ %d ¸öÇ©Ãû", sign_id);

		if (toupper(ch) != 'T')
		{
			prints("    °´[1;32m0[0;37m~[1;32m3[0;37mÑ¡Ç©Ãûµµ[m");

			moveto(24, 1);
			prints("[1;32mT[0;37m¸Ä±êÌâ, [1;32mC[0;37mÈ¡Ïû, [1;32mEnter[0;37m¼ÌÐø: [m");
			iflush();
			ch = 0;
		}

		for (; !SYS_server_exit; ch = igetch_t(MAX_DELAY_TIME))
		{
			switch (toupper(ch))
			{
			case CR:
				igetch_reset();
				break;
			case 'T':
				moveto(24, 1);
				clrtoeol();
				len = get_data(24, 1, "±êÌâ£º", title_input, STR_INPUT_LEN, 1);
				for (p = title_input; *p == ' '; p++)
					;
				for (q = title_input + len; q > p && *(q - 1) == ' '; q--)
					;
				*q = '\0';
				len = q - p;

				if (*p != '\0')
				{
					memcpy(title, p, (size_t)len + 1);
					memcpy(title_input, title, (size_t)len + 1);
				}
				if (title[0] != '\0') // title is valid
				{
					ch = 0;
				}
				break;
			case 'C':
				clearscr();
				moveto(1, 1);
				prints("È¡Ïû...");
				press_any_key();
				goto cleanup;
			case '0':
			case '1':
			case '2':
			case '3':
				sign_id = ch - '0';
				break;
			default: // Invalid selection
				continue;
			}

			break;
		}

		if (ch != CR)
		{
			continue;
		}

		for (ch = 'E'; !SYS_server_exit && toupper(ch) == 'E';)
		{
			editor_display(p_editor_data);

			clearscr();
			moveto(1, 1);
			prints("(S)·¢ËÍ, (C)È¡Ïû, (T)¸ü¸Ä±êÌâ or (E)ÔÙ±à¼­? [S]: ");
			iflush();

			for (ch = 0; !SYS_server_exit; ch = igetch_t(MAX_DELAY_TIME))
			{
				switch (toupper(ch))
				{
				case CR:
					igetch_reset();
				case 'S':
					break;
				case 'C':
					clearscr();
					moveto(1, 1);
					prints("È¡Ïû...");
					press_any_key();
					goto cleanup;
				case 'T':
					break;
				case 'E':
					break;
				default: // Invalid selection
					continue;
				}

				break;
			}
		}
	}

	// editor_data_save(p_editor_data, p_data_new, data_new_len);
	log_common("Debug: post article\n");

cleanup:
	editor_data_cleanup(p_editor_data);

	return 0;
}

int article_modify(SECTION_LIST *p_section, ARTICLE *p_article)
{
	ARTICLE_CACHE cache;
	EDITOR_DATA *p_editor_data;

	if (p_section == NULL || p_article == NULL)
	{
		log_error("NULL pointer error\n");
	}

	if (article_cache_load(&cache, VAR_ARTICLE_CACHE_DIR, p_article) < 0)
	{
		log_error("article_cache_load(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
		return -2;
	}
	p_editor_data = editor_data_load(cache.p_data);
	if (p_editor_data == NULL)
	{
		log_error("editor_data_load(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
		return -2;
	}
	if (article_cache_unload(&cache) < 0)
	{
		log_error("article_cache_unload(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
		return -2;
	}

	editor_display(p_editor_data);

	// editor_data_save(p_editor_data, p_data_new, data_new_len);

	editor_data_cleanup(p_editor_data);

	return 0;
}

int article_reply(SECTION_LIST *p_section, ARTICLE *p_article)
{
	ARTICLE_CACHE cache;
	EDITOR_DATA *p_editor_data;

	if (p_section == NULL || p_article == NULL)
	{
		log_error("NULL pointer error\n");
	}

	if (article_cache_load(&cache, VAR_ARTICLE_CACHE_DIR, p_article) < 0)
	{
		log_error("article_cache_load(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
		return -2;
	}
	p_editor_data = editor_data_load(cache.p_data);
	if (p_editor_data == NULL)
	{
		log_error("editor_data_load(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
		return -2;
	}
	if (article_cache_unload(&cache) < 0)
	{
		log_error("article_cache_unload(aid=%d, cid=%d) error\n", p_article->aid, p_article->cid);
		return -2;
	}

	editor_display(p_editor_data);

	// editor_data_save(p_editor_data, p_data_new, data_new_len);

	editor_data_cleanup(p_editor_data);

	return 0;
}
