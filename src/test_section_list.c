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
	"_1234_"
};

const char *stitle[] = {
	" Test Section ",
	"×ÖÄ¸×éºÏABC",
	"_Êý×Ö_123"
};

const char *master_name[] = {
	"sysadm",
	"SYSOP",
	""
};

int section_count = 3;

int main(int argc, char *argv[])
{
	SECTION_DATA *p_section[BBS_max_section];
	ARTICLE article;
	int i, j;
	int last_aid;

	if (log_begin("../log/bbsd.log", "../log/error.log") < 0)
	{
		printf("Open log error\n");
		return -1;
	}

	log_std_redirect(STDOUT_FILENO);
	log_err_redirect(STDERR_FILENO);

	if (section_data_pool_init("../conf/menu.conf", BBS_article_block_limit_per_section * BBS_max_section) < 0)
	{
		log_error("section_data_pool_init() error\n");
		return -2;
	}

	last_aid = 0;

	for (i = 0; i < section_count; i++)
	{
		if ((p_section[i] = section_data_create(sname[i], stitle[i], master_name[i])) == NULL)
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

			section_data_append_article(p_section[i], &article);
		}
	}

	for (i = 0; i < section_count; i++)
	{
		if (section_data_free_block(p_section[i]) != 0)
		{
			log_error("section_data_free_block(i=%d) error\n", i);
			return -4;
		}
	}

	section_data_pool_cleanup();

	log_end();

	return 0;
}
