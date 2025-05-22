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
	SECTION_LIST *p_section[BBS_max_section];
	ARTICLE *p_article;
	ARTICLE article;
	int block_count;
	int i, j;
	int last_aid;
	int current_tid;
	int section_first_aid;
	int group_count;
	int article_count;
	int step;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_std_redirect(STDOUT_FILENO);
	log_err_redirect(STDERR_FILENO);

	// - 1 to make blocks allocated is less than required, to trigger error handling
	block_count = BBS_article_limit_per_section * BBS_max_section / ARTICLE_PER_BLOCK - 1;

	if (article_block_init("../conf/menu.conf", block_count) < 0)
	{
		log_error("section_data_pool_init() error\n");
		return -2;
	}

	printf("Testing #1 ...\n");

	last_aid = 0;

	for (i = 0; i < section_count; i++)
	{
		p_section[i] = section_list_create(sname[i % section_conf_count],
										   stitle[i % section_conf_count],
										   master_name[i % section_conf_count]);
		if (p_section[i] == NULL)
		{
			log_error("section_data_create(i=%d) error\n", i);
			return -3;
		}
	}

	for (i = 0; i < section_conf_count; i++)
	{
		if (section_list_find_by_name(sname[i]) == NULL)
		{
			printf("section_data_find_section_by_name(%s) error\n", sname[i]);
			return -3;
		}
	}

	for (j = 0; j < BBS_article_limit_per_section; j++)
	{
		for (i = 0; i < section_count; i++)
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

			if (section_list_append_article(p_section[i], &article) < 0)
			{
				printf("append article (aid = %d) error at section %d index %d\n", article.aid, i, j);
				break;
			}
		}
	}

	for (i = 0; i < section_count; i++)
	{
		// printf("Loaded %d articles into section %d\n", p_section[i]->article_count, i);
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

			p_article = article_block_find_by_aid(last_aid);
			if (p_article == NULL || p_article->aid != last_aid)
			{
				printf("article_block_find_by_aid() at section %d index %d, %d != %d\n", i, j, p_article->aid, last_aid);
			}

			p_article = article_block_find_by_index(last_aid - 1);
			if (p_article == NULL || p_article->aid != last_aid)
			{
				printf("article_block_find_by_index() at section %d index %d, %d != %d\n", i, j, p_article->aid, last_aid);
			}

			if (section_list_set_article_visible(p_section[i], p_article->aid, 0) != 1)
			{
				printf("section_list_set_article_visible(aid = %d) error\n", p_article->aid);
			}
		}

		// printf("Verified %d articles in section %d\n", p_section[i]->article_count, i);
	}

	printf("Testing #2 ...\n");

	if (article_block_reset() != 0)
	{
		log_error("section_data_free_block(i=%d) error\n", i);
		return -4;
	}

	for (i = 0; i < section_count; i++)
	{
		section_list_reset_articles(p_section[i]);
	}

	group_count = 100;
	last_aid = 0;

	for (i = 0; i < section_count; i++)
	{
		section_first_aid = last_aid + 1;

		for (j = 0; j < BBS_article_limit_per_section; j++)
		{
			last_aid++;

			// Set article data
			article.aid = last_aid;
			article.cid = article.aid;
			// Group articles into group_count topics
			article.tid = ((article.aid < section_first_aid + group_count) ? 0 : (section_first_aid + j % group_count));
			article.uid = 1; // TODO: randomize
			article.visible = 1;
			article.excerption = 0;
			article.ontop = 0;
			article.lock = 0;

			if (section_list_append_article(p_section[i], &article) < 0)
			{
				printf("append article (aid = %d) error at section %d index %d\n", article.aid, i, j);
				break;
			}
		}

		// printf("Loaded %d articles into section %d\n", p_section[i]->article_count, i);
	}

	for (i = 0; i < section_count; i++)
	{
		if (p_section[i]->article_count == 0)
		{
			continue;
		}

		article_count = 0;
		last_aid = 0;

		p_article = p_section[i]->p_article_head;

		do
		{
			article_count++;

			if (p_article->aid <= last_aid)
			{
				printf("Non-ascending aid found %d <= %d at article\n", p_article->aid, last_aid);
			}
			last_aid = p_article->aid;

			p_article = p_article->p_next;
		} while (p_article != p_section[i]->p_article_head);

		if (article_count != p_section[i]->article_count)
		{
			printf("Count of articles in section %d is different from expected %d != %d\n",
				   i, article_count, p_section[i]->article_count);
			break;
		}

		// printf("Verified %d articles in section %d\n", group_count, i);
	}

	for (i = 0; i < section_count; i++)
	{
		if (p_section[i]->article_count == 0)
		{
			continue;
		}

		for (j = 0; j < group_count; j++)
		{
			last_aid = p_section[i]->p_article_head->aid + j;

			p_article = article_block_find_by_aid(last_aid);
			if (p_article == NULL)
			{
				printf("NULL p_article at section %d index %d\n", i, j);
				break;
			}
			if (p_article->aid != last_aid)
			{
				printf("Inconsistent aid at section %d index %d, %d != %d\n", i, j, p_article->aid, last_aid);
				break;
			}

			article_count = 1;
			last_aid = 0;
			current_tid = p_article->aid;

			do
			{
				if (p_article->aid <= last_aid)
				{
					printf("Non-ascending aid found %d <= %d\n", p_article->aid, last_aid);
				}
				last_aid = p_article->aid;

				p_article = p_article->p_topic_next;

				if (p_article == NULL)
				{
					printf("NULL p_article found\n");
					break;
				}
				if (p_article->tid == 0) // loop
				{
					break;
				}
				if (p_article->tid != current_tid)
				{
					printf("Inconsistent tid %d != %d\n", p_article->tid, current_tid);
					break;
				}

				article_count++;
			} while (1);

			if (article_count != p_section[i]->article_count / group_count)
			{
				printf("Count of articles in topic %d is different from expected %d != %d\n",
					   j + 1, article_count, p_section[i]->article_count / group_count);
				break;
			}
		}

		// printf("Verified %d topics in section %d\n", group_count, i);
	}

	printf("Testing #3 ...\n");

	for (i = 0; i < section_count; i++)
	{
		last_aid = 0;

		for (j = 0; j < p_section[i]->page_count; j++)
		{
			if (p_section[i]->p_page_first_article[j]->aid <= last_aid)
			{
				printf("Non-ascending aid found at section %d page %d, %d <= %d\n", i, j, p_section[i]->p_page_first_article[j]->aid, last_aid);
			}

			if (j > 0)
			{
				step = 0;

				for (p_article = p_section[i]->p_page_first_article[j];
					 p_article != p_section[i]->p_page_first_article[j - 1];
					 p_article = p_article->p_prior)
				{
					step++;
				}

				if (step != BBS_article_limit_per_page)
				{
					printf("Incorrect page size %d at section %d page %d\n", step, i, j - 1);
				}
			}
		}
	}

	printf("Press ENTER to exit...");
	getchar();

	article_block_cleanup();

	log_end();

	return 0;
}
