/***************************************************************************
					file_section_list.c  -  description
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

#include "section_list.h"
#include "bbs.h"
#include "log.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

const char *sname[] = {
	"Test",
	"ABCDEFG",
	"_1234_"};

const char *stitle[] = {
	" Test Section ",
	"×ÖÄ¸×éºÏABC",
	"_Êý×Ö_123"};

const char *master_name[] = {
	"sysadm",
	"SYSOP",
	""};

int section_conf_count = 3;
int section_count = BBS_max_section;

int main(int argc, char *argv[])
{
	SECTION_DATA *p_section[BBS_max_section];
	ARTICLE *p_article;
	ARTICLE article;
	int block_count;
	int i, j;
	int last_aid;
	int group_count;
	int article_count;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_std_redirect(STDOUT_FILENO);
	log_err_redirect(STDERR_FILENO);

	// block_count = BBS_article_block_limit_per_section * BBS_max_section; // This statement is correct
	// - 1 to make blocks allocated is less than required
	// Some error will be triggered while invoking section_data_append_article()
	block_count = BBS_article_block_limit_per_section * BBS_max_section - 1;

	if (section_data_pool_init("../conf/menu.conf", block_count) < 0)
	{
		log_error("section_data_pool_init() error\n");
		return -2;
	}

	printf("Testing #1 ...\n");

	last_aid = 0;

	for (i = 0; i < section_count; i++)
	{
		p_section[i] = section_data_create(sname[i % section_conf_count],
										   stitle[i % section_conf_count],
										   master_name[i % section_conf_count]);
		if (p_section[i] == NULL)
		{
			log_error("section_data_create(i=%d) error\n", i);
			return -3;
		}

		for (j = 0; j < BBS_article_limit_per_block * BBS_article_block_limit_per_section; j++)
		{
			last_aid++;

			// Set article data
			article.aid = last_aid;
			article.cid = article.aid;
			article.tid = 0;
			article.uid = 1; // TODO: randomize
			article.visible = 1;
			article.excerption = 0;
			article.ontop = 0;
			article.lock = 0;

			if (section_data_append_article(p_section[i], &article) < 0)
			{
				printf("append article (aid = %d) error\n", article.aid);
				break;
			}
		}

		printf("Load %d articles into section %d\n", p_section[i]->article_count, i);
	}

	last_aid = 0;

	for (i = 0; i < section_count; i++)
	{
		if (p_section[i]->article_count == 0)
		{
			continue;
		}

		for (j = 0; j < p_section[i]->article_count; j++)
		{
			last_aid++;

			p_article = section_data_find_article_by_index(p_section[i], j);
			if (p_article == NULL || p_article->aid != last_aid)
			{
				printf("Inconsistent aid at index %d != %d\n", j, last_aid);
			}

			if (section_data_mark_del_article(p_section[i], p_article->aid) != 1)
			{
				printf("section_data_mark_del_article(aid = %d) error\n", p_article->aid);
			}
		}

		printf("Verify %d articles in section %d\n", p_section[i]->article_count, i);
		printf("Delete %d articles in section %d\n", p_section[i]->delete_count, i);
	}

	printf("Testing #2 ...\n");

	group_count = 100;

	for (i = 0; i < section_count; i++)
	{
		if (section_data_free_block(p_section[i]) != 0)
		{
			log_error("section_data_free_block(i=%d) error\n", i);
			return -4;
		}

		last_aid = 0;

		for (j = 0; j < BBS_article_limit_per_block * BBS_article_block_limit_per_section; j++)
		{
			last_aid++;

			// Set article data
			article.aid = last_aid;
			article.cid = article.aid;
			article.tid = (article.aid <= group_count ? 0 : (article.aid - 1) % group_count + 1); // Group articles into group_count topics
			article.uid = 1;																	  // TODO: randomize
			article.visible = 1;
			article.excerption = 0;
			article.ontop = 0;
			article.lock = 0;

			if (section_data_append_article(p_section[i], &article) < 0)
			{
				printf("append article (aid = %d) error\n", article.aid);
				break;
			}
		}

		printf("Load %d articles into section %d\n", p_section[i]->article_count, i);
	}

	for (i = 0; i < section_count; i++)
	{
		if (p_section[i]->article_count == 0)
		{
			continue;
		}

		for (j = 0; j < group_count; j++)
		{
			p_article = section_data_find_article_by_index(p_section[i], j);
			if (p_article == NULL)
			{
				printf("NULL p_article at index %d\n", j);
				break;
			}
			if (p_article->aid != j + 1)
			{
				printf("Inconsistent aid at index %d != %d\n", j, j + 1);
				break;
			}

			article_count = 1;

			do
			{
				if (p_article->next_aid <= p_article->aid && p_article->next_aid != p_article->tid)
				{
					printf("Non-ascending aid found %d >= %d\n", p_article->aid, p_article->next_aid);
					break;
				}

				last_aid = p_article->next_aid;
				p_article = section_data_find_article_by_aid(p_section[i], last_aid);
				if (p_article == NULL)
				{
					printf("NULL p_article at aid %d\n", last_aid);
					break;
				}
				if (p_article->tid == 0) // loop
				{
					break;
				}
				if (p_article->tid != j + 1)
				{
					printf("Inconsistent tid at aid %d != %d\n", last_aid, j + 1);
					break;
				}
				article_count++;
			} while (1);

			if (article_count != p_section[i]->article_count / group_count)
			{
				printf("Count of articles in topic %d is less than expected %d < %d\n",
					   j + 1, article_count, p_section[i]->article_count / group_count);
			}
		}

		printf("Verify %d topics in section %d\n", group_count, i);
	}

	printf("Testing #3 ...\n");

	for (i = 0; i < section_conf_count; i++)
	{
		if (section_data_find_section_by_name(sname[i]) == NULL)
		{
			printf("section_data_find_section_by_name(%s) error\n", sname[i]);
		}
	}

	printf("Press ENTER to exit...");
	getchar();
	
	section_data_pool_cleanup();

	log_end();

	return 0;
}
