/***************************************************************************
							lml.h  -  description
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

#include "lml.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#define LML_TAG_PARAM_MAX_LEN 20
#define LML_TAG_OUTPUT_BUF_LEN 256

#define LML_TAG_COUNT 21

const static char *LML_tag_name[][2] = {
	{"left", "["},
	{"right", "]"},
	{"underline", "\033[4m"},
	{"/underline", "\033[24m"},
	{"u", "\033[4m"},
	{"/u", "\033[24m"},
	{"url", ""},
	{"/url", "(链接: %s)"},
	{"link", ""},
	{"/link", "(链接: %s)"},
	{"email", ""},
	{"/email", "(Email: %s)"},
	{"user", ""},
	{"/user", "(用户: %s)"},
	{"article", ""},
	{"/article", "(文章: %s)"},
	{"hide", ""},
	{"/hide", ""},
	{"image", "(图片: %s)"},
	{"flash", "(Flash: %s)"},
	{"bwf", "\033[31m****\033[m"},
};

static int LML_tag_name_len[LML_TAG_COUNT];
static int LML_init = 0;

inline static void lml_init(void)
{
	int i;

	if (!LML_init)
	{
		for (i = 0; i < LML_TAG_COUNT; i++)
		{
			LML_tag_name_len[i] = (int)strlen(LML_tag_name[i][0]);
		}

		LML_init = 1;
	}
}

int lml_plain(const char *str_in, char *str_out, int buf_len)
{
	char tag_param_buf[LML_TAG_PARAM_MAX_LEN + 1];
	char tag_output_buf[LML_TAG_OUTPUT_BUF_LEN];
	int i;
	int j = 0;
	int k;
	int tag_start_pos = -1;
	int tag_end_pos = -1;
	int tag_param_pos;
	int tag_output_len;

	lml_init();

	for (i = 0; str_in[i] != '\0'; i++)
	{
		if (str_in[i] == '[')
		{
			tag_start_pos = i + 1;
		}
		else if (str_in[i] == ']')
		{
			if (tag_start_pos >= 0)
			{
				tag_end_pos = i;

				// Skip space characters
				while (str_in[tag_start_pos] == ' ')
				{
					tag_start_pos++;
				}

				for (k = 0; k < LML_TAG_COUNT; k++)
				{
					if (strncasecmp(LML_tag_name[k][0], str_in + tag_start_pos, (size_t)LML_tag_name_len[k]) == 0)
					{
						switch (str_in[tag_start_pos + LML_tag_name_len[k]])
						{
						case ' ':
							tag_param_pos = tag_start_pos + LML_tag_name_len[k] + 1;
							while (str_in[tag_param_pos] == ' ')
							{
								tag_param_pos++;
							}
							if (tag_end_pos - tag_param_pos > 0)
							{
								strncpy(tag_param_buf, str_in + tag_param_pos, (size_t)MIN(tag_end_pos - tag_param_pos, LML_TAG_PARAM_MAX_LEN));
								tag_param_buf[MIN(tag_end_pos - tag_param_pos, LML_TAG_PARAM_MAX_LEN)] = '\0';
							}
						case ']':
							tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, LML_tag_name[k][1], tag_param_buf);
							if (j + tag_output_len >= buf_len - 1)
							{
								log_error("Buffer is not longer enough for output string %d >= %d\n", j + tag_output_len, buf_len - 1);
								str_out[j] = '\0';
								return j;
							}
							memcpy(str_out + j, tag_output_buf, (size_t)tag_output_len);
							j += tag_output_len;
							break;
						default: // tag_name not match
							continue;
						}
						break;
					}
				}

				tag_start_pos = -1;
			}
		}
		else if (tag_start_pos == -1) // not in LML tag
		{
			if (str_in[i] < 0 || str_in[i] > 127) // GBK chinese character
			{
				if (j + 2 >= buf_len - 1)
				{
					log_error("Buffer is not longer enough for output string %ld >= %d\n", j + 2, buf_len - 1);
					str_out[j] = '\0';
					return j;
				}
				str_out[j++] = str_in[i++];
				if (str_in[i] == '\0')
				{
					str_out[j] = '\0';
					return j;
				}
			}

			if (j + 1 >= buf_len - 1)
			{
				log_error("Buffer is not longer enough for output string %ld >= %d\n", j + 1, buf_len - 1);
				str_out[j] = '\0';
				return j;
			}
			str_out[j++] = str_in[i];
		}
		else // in LML tag
		{
			// Do nothing
		}
	}

	str_out[j] = '\0';

	return j;
}