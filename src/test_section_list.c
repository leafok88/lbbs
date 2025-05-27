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
#include "trie_dict.h"
#include "bbs.h"
#include "log.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#define ARTICLE_BLOCK_SHM_FILE "~article_block_shm.dat"
#define SECTION_LIST_SHM_FILE "~section_list_shm.dat"
#define TRIE_DICT_SHM_FILE "~trie_dict_shm.dat"

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
	ARTICLE *p_next;
	ARTICLE article;
	int block_count;
	int i, j;
	int sid;
	int last_aid;
	int current_tid;
	int section_first_aid;
	int group_count = 100;
	int article_count;
	int step;
	int32_t page;
	int32_t offset;
	int affected_count;
	FILE *fp;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_std_redirect(STDOUT_FILENO);
	log_err_redirect(STDERR_FILENO);

	// - 1 to make blocks allocated is less than required, to trigger error handling
	block_count = BBS_article_limit_per_section * BBS_max_section / ARTICLE_PER_BLOCK;

	if ((fp = fopen(ARTICLE_BLOCK_SHM_FILE, "w")) == NULL)
	{
		log_error("fopen(%s) error\n", ARTICLE_BLOCK_SHM_FILE);
		return -1;
	}
	fclose(fp);

	if ((fp = fopen(SECTION_LIST_SHM_FILE, "w")) == NULL)
	{
		log_error("fopen(%s) error\n", SECTION_LIST_SHM_FILE);
		return -1;
	}
	fclose(fp);

	if ((fp = fopen(TRIE_DICT_SHM_FILE, "w")) == NULL)
	{
		log_error("fopen(%s) error\n", TRIE_DICT_SHM_FILE);
		return -1;
	}
	fclose(fp);

	if (trie_dict_init(TRIE_DICT_SHM_FILE, TRIE_NODE_PER_POOL) < 0)
	{
		printf("trie_dict_init failed\n");
		return -1;
	}

	if (article_block_init(ARTICLE_BLOCK_SHM_FILE, block_count) < 0)
	{
		log_error("article_block_init(%s, %d) error\n", ARTICLE_BLOCK_SHM_FILE, block_count);
		return -2;
	}

	if (section_list_init(SECTION_LIST_SHM_FILE) < 0)
	{
		log_error("section_list_pool_init(%s) error\n", SECTION_LIST_SHM_FILE);
		return -2;
	}

	printf("Testing #1 ...\n");

	last_aid = 0;

	if (section_list_rw_lock(NULL) < 0)
	{
		printf("section_list_rw_lock(sid = %d) error\n", 0);
	}

	for (i = 0; i < section_count; i++)
	{
		sid = i * 3 + 1;
		p_section[i] = section_list_create(sid,
										   sname[i % section_conf_count],
										   stitle[i % section_conf_count],
										   master_name[i % section_conf_count]);
		if (p_section[i] == NULL)
		{
			printf("section_list_create(i = %d) error\n", i);
			return -3;
		}

		if (get_section_index(p_section[i]) != i)
		{
			printf("get_section_index(i = %d) error\n", i);
			return -3;
		}
	}

	for (i = 0; i < section_conf_count; i++)
	{
		if (section_list_find_by_name(sname[i]) == NULL)
		{
			printf("section_list_find_by_name(%s) error\n", sname[i]);
			return -3;
		}
	}

	for (i = 0; i < section_count; i++)
	{
		sid = i * 3 + 1;
		if (section_list_find_by_sid(sid) == NULL || section_list_find_by_sid(sid)->sid != sid)
		{
			printf("section_list_find_by_sid(%d) error\n", sid);
			return -3;
		}
	}

	if (section_list_rw_unlock(NULL) < 0)
	{
		printf("section_list_rw_unlock(sid = %d) error\n", 0);
	}

	for (j = 0; j < BBS_article_limit_per_section; j++)
	{
		for (i = 0; i < section_count; i++)
		{
			last_aid++;

			// Set article data
			article.aid = last_aid;
			article.tid = 0;
			article.sid = i * 3 + 1;
			article.cid = article.aid;
			article.uid = 1; // TODO: randomize
			article.visible = 1;
			article.excerption = 0;
			article.ontop = 0;
			article.lock = 0;
			article.transship = 0;

			if (section_list_rw_lock(p_section[i]) < 0)
			{
				printf("section_list_rw_lock(sid = %d) error\n", p_section[i]->sid);
				break;
			}

			if (section_list_append_article(p_section[i], &article) < 0)
			{
				printf("append article (aid = %d) error at section %d index %d\n", article.aid, i, j);
				break;
			}

			if (section_list_rw_unlock(p_section[i]) < 0)
			{
				printf("section_list_rw_unlock(sid = %d) error %d\n", p_section[i]->sid, errno);
				break;
			}
		}
	}

	for (i = 0; i < section_count; i++)
	{
		// printf("Loaded %d articles into section %d\n", p_section[i]->article_count, i);
	}

	if (last_aid != article_block_last_aid())
	{
		printf("last_aid != %d\n", article_block_last_aid());
	}

	if (article_block_article_count() != section_count * BBS_article_limit_per_section)
	{
		printf("article_block_article_count() error %d != %d * %d\n",
			   article_block_article_count(), section_count, BBS_article_limit_per_section);
	}

	last_aid = 0;

	for (j = 0; j < BBS_article_limit_per_section; j++)
	{
		for (i = 0; i < section_count; i++)
		{
			last_aid++;

			p_article = article_block_find_by_aid(last_aid);
			if (p_article == NULL || p_article->aid != last_aid)
			{
				printf("article_block_find_by_aid() at section %d index %d, %d != %d\n", i, j, p_article->aid, last_aid);
			}

			if (section_list_rw_lock(p_section[i]) < 0)
			{
				printf("section_list_rw_lock(sid = %d) error\n", p_section[i]->sid);
				break;
			}

			if (section_list_set_article_visible(p_section[i], p_article->aid, 0) != 1)
			{
				printf("section_list_set_article_visible(aid = %d) error\n", p_article->aid);
			}

			if (section_list_rw_unlock(p_section[i]) < 0)
			{
				printf("section_list_rw_unlock(sid = %d) error\n", p_section[i]->sid);
				break;
			}
		}

		// printf("Verified %d articles in section %d\n", p_section[i]->article_count, i);
	}

	printf("Testing #2 ...\n");

	if (section_list_rw_lock(NULL) < 0)
	{
		printf("section_list_rw_lock(sid = %d) error\n", 0);
	}

	if (article_block_reset() != 0)
	{
		log_error("article_block_reset() error\n");
		return -4;
	}

	for (i = 0; i < section_count; i++)
	{
		section_list_reset_articles(p_section[i]);
	}

	if (section_list_rw_unlock(NULL) < 0)
	{
		printf("section_list_rw_unlock(sid = %d) error\n", 0);
	}

	last_aid = 0;

	for (i = 0; i < section_count; i++)
	{
		section_first_aid = last_aid + 1;

		if (section_list_rw_lock(p_section[i]) < 0)
		{
			printf("section_list_rw_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		for (j = 0; j < BBS_article_limit_per_section; j++)
		{
			last_aid++;

			// Set article data
			article.aid = last_aid;
			// Group articles into group_count topics
			article.tid = ((article.aid < section_first_aid + group_count) ? 0 : (section_first_aid + j % group_count));
			article.sid = i * 3 + 1;
			article.cid = article.aid;
			article.uid = 1; // TODO: randomize
			article.visible = 1;
			article.excerption = 0;
			article.ontop = 0;
			article.lock = 0;
			article.transship = 0;

			if (section_list_append_article(p_section[i], &article) < 0)
			{
				printf("append article (aid = %d) error at section %d index %d\n", article.aid, i, j);
				break;
			}
		}

		if (section_list_rw_unlock(p_section[i]) < 0)
		{
			printf("section_list_rw_unlock(sid = %d) error\n", p_section[i]->sid);
			break;
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

		if (section_list_rd_lock(p_section[i]) < 0)
		{
			printf("section_list_rd_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

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

		if (section_list_rd_unlock(p_section[i]) < 0)
		{
			printf("section_list_rd_unlock(sid = %d) error\n", p_section[i]->sid);
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

		if (section_list_rd_lock(p_section[i]) < 0)
		{
			printf("section_list_rd_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		if (p_section[i]->topic_count != group_count)
		{
			printf("Inconsistent topic count in section %d, %d != %d\n", i, p_section[i]->topic_count, group_count);
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
				// break;
			}
		}

		if (section_list_rd_unlock(p_section[i]) < 0)
		{
			printf("section_list_rd_unlock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		// printf("Verified %d topics in section %d\n", group_count, i);
	}

	printf("Testing #3 ...\n");

	for (i = 0; i < section_count; i++)
	{
		last_aid = 0;

		if (section_list_rd_lock(p_section[i]) < 0)
		{
			printf("section_list_rd_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

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

		if (section_list_rd_unlock(p_section[i]) < 0)
		{
			printf("section_list_rd_unlock(sid = %d) error\n", p_section[i]->sid);
			break;
		}
	}

	printf("Testing #4 ...\n");

	for (i = 0; i < section_count; i++)
	{
		if (section_list_rw_lock(p_section[i]) < 0)
		{
			printf("section_list_rw_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		step = i % 10 + 1;
		for (j = group_count; j < BBS_article_limit_per_section; j += step)
		{
			last_aid = i * BBS_article_limit_per_section + j + 1;

			p_article = section_list_find_article_with_offset(p_section[i], last_aid, &page, &offset, &p_next);

			if (p_article == NULL)
			{
				printf("Error find article %d in section %d offset %d\n", last_aid, i, j);
				break;
			}

			if (p_article->aid != last_aid)
			{
				printf("Inconsistent article aid in section %d page %d offset %d %d != %d\n", i, page, offset, p_article->aid, last_aid);
				break;
			}

			if (section_list_set_article_visible(p_section[i], last_aid, 0) != 1)
			{
				printf("Error set article %d invisible in section %d offset %d\n", last_aid, i, j);
				break;
			}
		}

		last_aid = p_section[i]->p_article_head->aid;
		if (section_list_calculate_page(p_section[i], last_aid) < 0)
		{
			printf("section_list_calculate_page(aid = %d) error in section %d offset %d\n", last_aid, i, j);
			break;
		}

		if (p_section[i]->visible_article_count / BBS_article_limit_per_page +
				(p_section[i]->visible_article_count % BBS_article_limit_per_page ? 1 : 0) !=
			p_section[i]->page_count)
		{
			printf("Inconsistent page count in section %d offset %d, %d != %d, "
				   "visible_article_count = %d, last_page_visible_count = %d\n",
				   i, j,
				   p_section[i]->visible_article_count / BBS_article_limit_per_page +
					   (p_section[i]->visible_article_count % BBS_article_limit_per_page ? 1 : 0),
				   p_section[i]->page_count, p_section[i]->visible_article_count,
				   p_section[i]->last_page_visible_article_count);
			break;
		}

		affected_count = (BBS_article_limit_per_section - group_count) / step + ((BBS_article_limit_per_section - group_count) % step ? 1 : 0);
		if (p_section[i]->article_count - p_section[i]->visible_article_count != affected_count)
		{
			printf("Inconsistent set invisible count in section %d, %d != %d\n", i,
				   p_section[i]->article_count - p_section[i]->visible_article_count,
				   affected_count);
			break;
		}

		article_count = p_section[i]->visible_article_count;

		for (j = 0; j < group_count; j += 1)
		{
			last_aid = i * BBS_article_limit_per_section + j + 1;

			p_article = section_list_find_article_with_offset(p_section[i], last_aid, &page, &offset, &p_next);

			if (p_article == NULL)
			{
				printf("Error find article %d in section %d offset %d\n", last_aid, i, j);
				break;
			}

			if (p_article->aid != last_aid)
			{
				printf("Inconsistent article aid in section %d page %d offset %d %d != %d\n", i, page, offset, p_article->aid, last_aid);
				break;
			}

			if (p_article->tid != 0)
			{
				printf("Non-topic article aid in section %d page %d offset %d %d != 0\n", i, page, offset, p_article->tid);
				break;
			}

			if ((affected_count = section_list_set_article_visible(p_section[i], last_aid, 0)) <= 0)
			{
				printf("Error set topic %d invisible in section %d offset %d\n", last_aid, i, j);
				break;
			}

			article_count -= affected_count;
		}

		if (article_count != 0)
		{
			printf("Inconsistent total set invisible topic count in section %d, %d > 0\n", i, article_count);
			break;
		}

		if (p_section[i]->visible_article_count > 0)
		{
			printf("Inconsistent invisible article count in section %d, %d > 0\n", i, p_section[i]->visible_article_count);
			break;
		}

		if (p_section[i]->visible_topic_count > 0)
		{
			printf("Inconsistent invisible topic count in section %d, %d > 0\n", i, p_section[i]->visible_topic_count);
			break;
		}

		last_aid = p_section[i]->p_article_head->aid;
		if (section_list_calculate_page(p_section[i], last_aid) < 0)
		{
			printf("section_list_calculate_page(aid = %d) error in section %d offset %d\n", last_aid, i, j);
			break;
		}

		if (p_section[i]->visible_article_count / BBS_article_limit_per_page +
				(p_section[i]->visible_article_count % BBS_article_limit_per_page ? 1 : 0) !=
			p_section[i]->page_count)
		{
			printf("Inconsistent page count in section %d offset %d, %d != %d, "
				   "visible_article_count = %d, last_page_visible_count = %d\n",
				   i, j,
				   p_section[i]->visible_article_count / BBS_article_limit_per_page +
					   (p_section[i]->visible_article_count % BBS_article_limit_per_page ? 1 : 0),
				   p_section[i]->page_count, p_section[i]->visible_article_count,
				   p_section[i]->last_page_visible_article_count);
			break;
		}

		if (section_list_rw_unlock(p_section[i]) < 0)
		{
			printf("section_list_rw_unlock(sid = %d) error\n", p_section[i]->sid);
			break;
		}
	}

	for (i = 0; i < BBS_max_section; i++)
	{
		if (section_list_rw_lock(p_section[i]) < 0)
		{
			printf("section_list_rw_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		affected_count = 0;

		for (j = 0; j < BBS_article_limit_per_section; j += 1)
		{
			last_aid = i * BBS_article_limit_per_section + j + 1;

			if (section_list_set_article_visible(p_section[i], last_aid, 1) <= 0)
			{
				printf("Error set article %d visible in section %d offset %d\n", last_aid, i, j);
				break;
			}

			affected_count++;
		}

		if (affected_count != p_section[i]->article_count)
		{
			printf("Inconsistent total set visible article count in section %d, %d != %d\n", i, affected_count, p_section[i]->article_count);
			break;
		}

		if (p_section[i]->visible_article_count != p_section[i]->article_count)
		{
			printf("Inconsistent visible article count in section %d, %d != %d\n", i, p_section[i]->visible_article_count, p_section[i]->article_count);
			break;
		}

		if (p_section[i]->visible_topic_count != group_count)
		{
			printf("Inconsistent visible topic count in section %d, %d != %d\n", i, p_section[i]->visible_topic_count, group_count);
			break;
		}

		last_aid = p_section[i]->p_article_head->aid;
		if (section_list_calculate_page(p_section[i], last_aid) < 0)
		{
			printf("section_list_calculate_page(aid = %d) error in section %d offset %d\n", last_aid, i, j);
			break;
		}

		if (p_section[i]->visible_article_count / BBS_article_limit_per_page +
				(p_section[i]->visible_article_count % BBS_article_limit_per_page ? 1 : 0) !=
			p_section[i]->page_count)
		{
			printf("Inconsistent page count in section %d offset %d, %d != %d, "
				   "visible_article_count = %d, last_page_visible_count = %d\n",
				   i, j,
				   p_section[i]->visible_article_count / BBS_article_limit_per_page +
					   (p_section[i]->visible_article_count % BBS_article_limit_per_page ? 1 : 0),
				   p_section[i]->page_count, p_section[i]->visible_article_count,
				   p_section[i]->last_page_visible_article_count);
			break;
		}

		if (section_list_rw_unlock(p_section[i]) < 0)
		{
			printf("section_list_rw_unlock(sid = %d) error\n", p_section[i]->sid);
			break;
		}
	}

	printf("Testing #5 ...\n");

	if (section_list_rw_lock(NULL) < 0)
	{
		printf("section_list_rw_lock(sid = %d) error\n", 0);
	}

	if (article_block_reset() != 0)
	{
		log_error("article_block_reset() error\n");
		return -4;
	}

	for (i = 0; i < section_count; i++)
	{
		section_list_reset_articles(p_section[i]);
	}

	if (section_list_rw_unlock(NULL) < 0)
	{
		printf("section_list_rw_unlock(sid = %d) error\n", 0);
	}

	last_aid = 0;

	for (i = 0; i < section_count / 2; i++)
	{
		if (section_list_rw_lock(p_section[i]) < 0)
		{
			printf("section_list_rw_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		section_first_aid = last_aid + 1;

		for (j = 0; j < BBS_article_limit_per_section; j++)
		{
			last_aid++;

			// Set article data
			article.aid = last_aid;
			// Group articles into group_count topics
			article.tid = ((article.aid < section_first_aid + group_count) ? 0 : (section_first_aid + j % group_count));
			article.sid = i * 3 + 1;
			article.cid = article.aid;
			article.uid = 1; // TODO: randomize
			article.visible = 1;
			article.excerption = 0;
			article.ontop = 0;
			article.lock = 0;
			article.transship = 0;

			if (section_list_append_article(p_section[i], &article) < 0)
			{
				printf("append article (aid = %d) error at section %d index %d\n", article.aid, i, j);
				break;
			}
		}

		if (section_list_rw_unlock(p_section[i]) < 0)
		{
			printf("section_list_rw_unlock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		// printf("Loaded %d articles into section %d\n", p_section[i]->article_count, i);
	}

	for (i = 0; i < section_count / 2; i++)
	{
		if (section_list_rw_lock(p_section[i]) < 0)
		{
			printf("section_list_rw_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		section_first_aid = p_section[i]->p_article_head->aid;

		for (j = 0; j < group_count; j += 2)
		{
			p_article = section_list_find_article_with_offset(p_section[i], section_first_aid + j, &page, &offset, &p_next);
			if (p_article == NULL)
			{
				printf("section_list_find_article_with_offset(aid = %d) not found in section %d\n",
					   section_first_aid + j, i);
				break;
			}

			if (section_list_set_article_visible(p_section[i], p_article->aid, 0) != BBS_article_limit_per_section / group_count)
			{
				printf("section_list_set_article_visible(aid = %d) error\n", p_article->aid);
			}
		}

		if (section_list_rw_unlock(p_section[i]) < 0)
		{
			printf("section_list_rw_unlock(sid = %d) error\n", p_section[i]->sid);
			break;
		}
	}

	for (i = 0; i < section_count / 2; i++)
	{
		if (section_list_rw_lock(p_section[i]) < 0)
		{
			printf("section_list_rw_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		if (section_list_rw_lock(p_section[section_count / 2 + i]) < 0)
		{
			printf("section_list_rw_lock(sid = %d) error\n", p_section[section_count / 2 + i]->sid);

			if (section_list_rw_unlock(p_section[i]) < 0)
			{
				printf("section_list_rw_unlock(sid = %d) error\n", p_section[i]->sid);
			}
			break;
		}

		section_first_aid = p_section[i]->p_article_head->aid;

		for (j = 0; j < group_count; j++)
		{
			affected_count = section_list_move_topic(p_section[i], p_section[section_count / 2 + i], section_first_aid + j);

			if (affected_count < 0)
			{
				printf("move topic (aid = %d) error from section %d to section %d\n", section_first_aid + j, i, section_count / 2 + i);
				break;
			}

			if (affected_count != BBS_article_limit_per_section / group_count)
			{
				printf("move topic (aid = %d) affected article count %d != %d\n",
					   section_first_aid + j, affected_count,
					   BBS_article_limit_per_section / group_count);
				// break;
			}
		}

		if (section_list_rw_unlock(p_section[i]) < 0)
		{
			printf("section_list_rw_unlock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		if (section_list_rw_unlock(p_section[section_count / 2 + i]) < 0)
		{
			printf("section_list_rw_unlock(sid = %d) error\n", p_section[section_count / 2 + i]->sid);

			if (section_list_rw_unlock(p_section[i]) < 0)
			{
				printf("section_list_rw_unlock(sid = %d) error\n", p_section[i]->sid);
			}
			break;
		}
	}

	for (i = 0; i < section_count; i++)
	{
		if (section_list_rd_lock(p_section[i]) < 0)
		{
			printf("section_list_rd_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}

		if (p_section[i]->topic_count != (i < section_count / 2 ? 0 : group_count))
		{
			printf("Topic count error in section %d, %d != %d\n", i,
				   p_section[i]->topic_count, (i < section_count / 2 ? 0 : group_count));
			break;
		}

		if (p_section[i]->visible_topic_count != (i < section_count / 2 ? 0 : group_count / 2))
		{
			printf("Visible topic count error in section %d, %d != %d\n", i,
				   p_section[i]->visible_topic_count, (i < section_count / 2 ? 0 : group_count / 2));
			break;
		}

		if (p_section[i]->article_count != (i < section_count / 2 ? 0 : BBS_article_limit_per_section))
		{
			printf("Article count error in section %d, %d != %d\n", i,
				   p_section[i]->article_count, (i < section_count / 2 ? 0 : BBS_article_limit_per_section));
			break;
		}

		if (p_section[i]->visible_article_count != (i < section_count / 2 ? 0 : BBS_article_limit_per_section / 2))
		{
			printf("Visible article count error in section %d, %d != %d\n", i,
				   p_section[i]->visible_article_count, (i < section_count / 2 ? 0 : BBS_article_limit_per_section / 2));
			break;
		}

		if (p_section[i]->page_count != (i < section_count / 2 ? 0 : BBS_article_limit_per_section / 2 / BBS_article_limit_per_page))
		{
			printf("Page count error in section %d, %d != %d\n", i,
				   p_section[i]->page_count, (i < section_count / 2 ? 0 : BBS_article_limit_per_section / 2 / BBS_article_limit_per_page));
			break;
		}

		if (section_list_rd_unlock(p_section[i]) < 0)
		{
			printf("section_list_rd_unlock(sid = %d) error\n", p_section[i]->sid);
			break;
		}
	}

	printf("Testing #6 ...\n");

	for (i = 0; i < section_count; i++)
	{
		if (section_list_rd_lock(p_section[i]) < 0)
		{
			printf("section_list_rd_lock(sid = %d) error\n", p_section[i]->sid);
			break;
		}
	}

	printf("Try rw_lock for 5 sec...\n");
	if (section_list_try_rw_lock(NULL, 5) == 0)
	{
		printf("section_list_try_rw_lock(sid = %d) error, expectation is timeout\n", p_section[i]->sid);
	}

	for (i = 0; i < section_count; i++)
	{
		if (section_list_rd_unlock(p_section[i]) < 0)
		{
			printf("section_list_rd_unlock(sid = %d) error\n", p_section[i]->sid);
			break;
		}
	}

	if (section_list_try_rw_lock(NULL, 5) < 0)
	{
		printf("section_list_rd_lock(sid = %d) error\n", p_section[i]->sid);
	}

	for (i = 0; i < section_count; i++)
	{
		if (section_list_try_rd_lock(p_section[i], 0) == 0)
		{
			printf("section_list_try_rd_lock(sid = %d) error, expectation is timeout\n", p_section[i]->sid);
			break;
		}
	}

	if (section_list_rw_unlock(NULL) < 0)
	{
		printf("section_list_rw_unlock(sid = %d) error\n", p_section[i]->sid);
	}

	printf("Press ENTER to exit...");
	getchar();

	section_list_cleanup();
	article_block_cleanup();
	trie_dict_cleanup();

	if (unlink(TRIE_DICT_SHM_FILE) < 0)
	{
		log_error("unlink(%s) error\n", TRIE_DICT_SHM_FILE);
		return -1;
	}

	if (unlink(ARTICLE_BLOCK_SHM_FILE) < 0)
	{
		log_error("unlink(%s) error\n", ARTICLE_BLOCK_SHM_FILE);
		return -1;
	}

	if (unlink(SECTION_LIST_SHM_FILE) < 0)
	{
		log_error("unlink(%s) error\n", SECTION_LIST_SHM_FILE);
		return -1;
	}

	log_end();

	return 0;
}
