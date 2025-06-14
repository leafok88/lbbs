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
#include "lml.h"
#include "database.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#define TITLE_INPUT_MAX_LEN 74
#define ARTICLE_CONTENT_MAX_LEN 1024 * 1024 * 4 // 4MB
#define ARTICLE_QUOTE_MAX_LINES 20
#define ARTICLE_QUOTE_LINE_MAX_LEN 76

int article_post(SECTION_LIST *p_section)
{
	EDITOR_DATA *p_editor_data = NULL;
	char title[BBS_article_title_max_len + 1];
	char title_input[TITLE_INPUT_MAX_LEN + 1];
	int sign_id = 0;
	long len;
	int ch;
	char *p, *q;

	if (p_section == NULL)
	{
		log_error("NULL pointer error\n");
	}

	title[0] = '\0';
	title_input[0] = '\0';

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
		prints("±êÌâ: %s", (title[0] == '\0' ? "[ÎÞ]" : title));
		moveto(23, 1);
		prints("Ê¹ÓÃµÚ [1;32m%d[m ¸öÇ©Ãû", sign_id);

		if (toupper(ch) != 'T')
		{
			prints("    °´[1;32m0[m~[1;32m3[mÑ¡Ç©Ãûµµ(0±íÊ¾²»Ê¹ÓÃ)");

			moveto(24, 1);
			prints("[1;32mT[m¸Ä±êÌâ, [1;32mC[mÈ¡Ïû, [1;32mEnter[m¼ÌÐø: ");
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
				len = get_data(24, 1, "±êÌâ: ", title_input, TITLE_INPUT_MAX_LEN, 1);
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
				ch = 0;
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

		if (ch != CR || title[0] == '\0')
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

		if (toupper(ch) != 'T')
		{
			break;
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
	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	int ch;

	EDITOR_DATA *p_editor_data = NULL;

	if (p_section == NULL || p_article == NULL)
	{
		log_error("NULL pointer error\n");
	}

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		return -1;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT bbs_content.CID, bbs_content.content "
			 "FROM bbs INNER JOIN bbs_content ON bbs.CID = bbs_content.CID "
			 "WHERE bbs.AID = %d",
			 p_article->aid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article content error: %s\n", mysql_error(db));
		return -2;
	}
	if ((rs = mysql_use_result(db)) == NULL)
	{
		log_error("Get article content data failed\n");
		return -2;
	}

	if ((row = mysql_fetch_row(rs)))
	{
		p_editor_data = editor_data_load(row[1]);
		if (p_editor_data == NULL)
		{
			log_error("editor_data_load(aid=%d, cid=%d) error\n", p_article->aid, atoi(row[0]));
			mysql_free_result(rs);
			mysql_close(db);
			return -3;
		}
	}
	mysql_free_result(rs);
	mysql_close(db);

	for (ch = 'E'; !SYS_server_exit && toupper(ch) == 'E';)
	{
		editor_display(p_editor_data);

		clearscr();
		moveto(1, 1);
		prints("(S)±£´æ, (C)È¡Ïû or (E)ÔÙ±à¼­? [S]: ");
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
			case 'E':
				break;
			default: // Invalid selection
				continue;
			}

			break;
		}
	}

	// editor_data_save(p_editor_data, p_data_new, data_new_len);
	log_common("Debug: modify article\n");

cleanup:
	editor_data_cleanup(p_editor_data);

	return 0;
}

int article_reply(SECTION_LIST *p_section, ARTICLE *p_article)
{
	MYSQL *db;
	MYSQL_RES *rs;
	MYSQL_ROW row;
	char sql[SQL_BUFFER_LEN];
	char content[LINE_BUFFER_LEN * ARTICLE_QUOTE_MAX_LINES + 1];
	char content_f[ARTICLE_CONTENT_MAX_LEN];
	long line_offsets[ARTICLE_QUOTE_MAX_LINES + 1];
	EDITOR_DATA *p_editor_data = NULL;
	char title[BBS_article_title_max_len + 1];
	char title_input[BBS_article_title_max_len + 1 + 4]; // + "Re: "
	int sign_id = 0;
	long len;
	int ch;
	char *p, *q;
	int eol;
	int display_len;
	long quote_content_lines;
	long i;

	if (p_section == NULL || p_article == NULL)
	{
		log_error("NULL pointer error\n");
	}

	title[0] = '\0';
	snprintf(title_input, sizeof(title_input), "Re: %s", p_article->title);
	len = split_line(title_input, TITLE_INPUT_MAX_LEN, &eol, &display_len);
	title_input[len] = '\0';

	db = db_open();
	if (db == NULL)
	{
		log_error("db_open() error: %s\n", mysql_error(db));
		return -1;
	}

	snprintf(sql, sizeof(sql),
			 "SELECT bbs_content.CID, bbs_content.content "
			 "FROM bbs INNER JOIN bbs_content ON bbs.CID = bbs_content.CID "
			 "WHERE bbs.AID = %d",
			 p_article->aid);

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query article content error: %s\n", mysql_error(db));
		return -2;
	}
	if ((rs = mysql_use_result(db)) == NULL)
	{
		log_error("Get article content data failed\n");
		return -2;
	}

	if ((row = mysql_fetch_row(rs)))
	{
		// Apply LML render to content body
		len = lml_plain(row[1], content_f, ARTICLE_CONTENT_MAX_LEN);
		content_f[len] = '\0';

		// Remove control sequence
		len = ctrl_seq_filter(content_f);

		len = snprintf(content, sizeof(content), "\n\n¡¾ ÔÚ %s (%s) µÄ´ó×÷ÖÐÌáµ½: ¡¿\n", p_article->username, p_article->nickname);

		quote_content_lines = split_data_lines(content_f, ARTICLE_QUOTE_LINE_MAX_LEN, line_offsets, ARTICLE_QUOTE_MAX_LINES + 1);
		for (i = 0; i < quote_content_lines; i++)
		{
			memcpy(content + len, ": ", 2); // quote line prefix
			len += 2;
			memcpy(content + len, content_f + line_offsets[i], (size_t)(line_offsets[i + 1] - line_offsets[i]));
			len += (line_offsets[i + 1] - line_offsets[i]);
		}
		content[len] = '\0';
	}
	mysql_free_result(rs);
	mysql_close(db);

	p_editor_data = editor_data_load(content);
	if (p_editor_data == NULL)
	{
		log_error("editor_data_load(aid=%d, cid=%d) error\n", p_article->aid, atoi(row[0]));
		return -3;
	}

	// Set title and sign
	for (ch = 'T'; !SYS_server_exit;)
	{
		clearscr();
		moveto(21, 1);
		prints("»Ø¸´ÎÄÕÂÓÚ %s[%s] ÌÖÂÛÇø", p_section->stitle, p_section->sname);
		moveto(22, 1);
		prints("±êÌâ: %s", (title[0] == '\0' ? "[ÎÞ]" : title));
		moveto(23, 1);
		prints("Ê¹ÓÃµÚ [1;32m%d[m ¸öÇ©Ãû", sign_id);

		if (toupper(ch) != 'T')
		{
			prints("    °´[1;32m0[m~[1;32m3[mÑ¡Ç©Ãûµµ(0±íÊ¾²»Ê¹ÓÃ)");

			moveto(24, 1);
			prints("[1;32mT[m¸Ä±êÌâ, [1;32mC[mÈ¡Ïû, [1;32mEnter[m¼ÌÐø: ");
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
				len = get_data(24, 1, "±êÌâ: ", title_input, TITLE_INPUT_MAX_LEN, 1);
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
				ch = 0;
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

		if (ch != CR || title[0] == '\0')
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

		if (toupper(ch) != 'T')
		{
			break;
		}
	}

	// editor_data_save(p_editor_data, p_data_new, data_new_len);
	log_common("Debug: reply article\n");

cleanup:
	editor_data_cleanup(p_editor_data);

	return 0;
}
