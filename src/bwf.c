/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bwf
 *   - bad words filter
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define PCRE2_CODE_UNIT_WIDTH 8

#include "bwf.h"
#include "log.h"
#include <errno.h>
#include <pcre2.h>
#include <stdio.h>
#include <string.h>

enum _bwf_constant_t
{
	BWF_max_pattern_len = 64,
	BWF_max_pattern_cnt = 1000,
};

static char bwf_pattern_str[(BWF_max_pattern_len + 1) * BWF_max_pattern_cnt];
static pcre2_code *bwf_code = NULL;

int bwf_load(const char *filename)
{
	FILE *fp;
	char line[BWF_max_pattern_len + 1];
	size_t len_line;
	char *p = bwf_pattern_str;
	int line_id = 0;

	if (filename == NULL)
	{
		log_error("NULL pointer error");
		return -1;
	}

	if ((fp = fopen(filename, "r")) == NULL)
	{
		log_error("fopen(%s) error: %d", filename, errno);
		return -2;
	}

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		len_line = strnlen(line, sizeof(line) - 1);
		if (!feof(fp) && line[len_line - 1] != '\n')
		{
			log_error("Data line %d (len=%d) is truncated", line_id, len_line);
			bwf_pattern_str[0] = '\0';
			return -3;
		}

		while (len_line > 0 && (line[len_line - 1] == '\n' || line[len_line - 1] == '\r'))
		{
			line[len_line - 1] = '\0';
			len_line--;
		}

		if (len_line == 0)
		{
			continue;
		}

		if (p > bwf_pattern_str)
		{
			*p++ = '|';
		}

		if (len_line + 2 > sizeof(bwf_pattern_str) - 1 - (size_t)(p - bwf_pattern_str))
		{
			log_error("Data in %s exceed length limit %d", filename, sizeof(bwf_pattern_str) - 1);
			bwf_pattern_str[0] = '\0';
			return -3;
		}

		*p++ = '(';
		p = mempcpy(p, line, strnlen(line, sizeof(bwf_pattern_str) - 1 - (size_t)(p - bwf_pattern_str)));
		*p++ = ')';
		line_id++;
	}

	*p = '\0';

	fclose(fp);

	log_debug("Debug: bwf_pattern_str: %s", bwf_pattern_str);

	return 0;
}

int bwf_compile(void)
{
	int errorcode;
	PCRE2_SIZE erroroffset;

	bwf_cleanup();

	bwf_code = pcre2_compile((PCRE2_SPTR)bwf_pattern_str, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &errorcode, &erroroffset, NULL);
	if (bwf_code == NULL)
	{
		log_error("pcre2_compile() error: %d", errorcode);
		return -1;
	}

	return 0;
}

void bwf_cleanup(void)
{
	if (bwf_code != NULL)
	{
		pcre2_code_free(bwf_code);
		bwf_code = NULL;
	}
}

int check_badwords(char *str, char c_mask)
{
	pcre2_match_data *match_data;
	PCRE2_SIZE startoffset = 0;
	PCRE2_SIZE *ovector;
	uint32_t match_count;
	int ret;
	int i;
	int total_match_count = 0;

	if (bwf_code == NULL)
	{
		log_error("BWF not loaded");
		return -1;
	}

	match_data = pcre2_match_data_create_from_pattern(bwf_code, NULL);

	while (1)
	{
		ret = pcre2_match(bwf_code, (PCRE2_SPTR)str, PCRE2_ZERO_TERMINATED, startoffset, 0, match_data, NULL);
		if (ret == PCRE2_ERROR_NOMATCH)
		{
			ret = total_match_count;
			break;
		}
		else if (ret < 0)
		{
			log_error("pcre2_match() error: %d", ret);
		}
		else if (ret == 0)
		{
			log_error("Vector of offsets is too small");
		}
		else // ret >= 1
		{
			ovector = pcre2_get_ovector_pointer(match_data);
			match_count = pcre2_get_ovector_count(match_data);

			i = ret - 1;
			if (ovector[i * 2] == -1 || ovector[i * 2 + 1] == -1)
			{
				log_error("Bug: match pattern #%d of %d with invalid offsets [%d, %d)",
						  i, match_count, ovector[i * 2], ovector[i * 2 + 1]);
				ret = -2;
			}
			else
			{
				log_debug("Debug: match pattern #%d of %d at offsets [%d, %d]",
						  i, match_count, ovector[i * 2], ovector[i * 2 + 1] - ovector[i * 2]);
				memset(str + ovector[i * 2], c_mask, ovector[i * 2 + 1] - ovector[i * 2]);
				total_match_count++;
				startoffset = ovector[i * 2 + 1];
			}
		}
	}

	pcre2_match_data_free(match_data);

	return ret;
}
