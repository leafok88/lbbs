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

#include "common.h"
#include "lml.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#define LML_TAG_PARAM_BUF_LEN 256
#define LML_TAG_OUTPUT_BUF_LEN 1024

typedef int (*lml_tag_filter_cb)(const char *tag_name, const char *tag_param_buf, char *tag_output_buf, size_t tag_output_buf_len);

static int lml_tag_color_filter(const char *tag_name, const char *tag_param_buf, char *tag_output_buf, size_t tag_output_buf_len)
{
	if (strcasecmp(tag_name, "color") == 0)
	{
		if (strcasecmp(tag_param_buf, "red") == 0)
		{
			return snprintf(tag_output_buf, tag_output_buf_len, "\033[1;31m");
		}
		else if (strcasecmp(tag_param_buf, "green") == 0)
		{
			return snprintf(tag_output_buf, tag_output_buf_len, "\033[1;32m");
		}
		else if (strcasecmp(tag_param_buf, "yellow") == 0)
		{
			return snprintf(tag_output_buf, tag_output_buf_len, "\033[1;33m");
		}
		else if (strcasecmp(tag_param_buf, "blue") == 0)
		{
			return snprintf(tag_output_buf, tag_output_buf_len, "\033[1;34m");
		}
		else if (strcasecmp(tag_param_buf, "magenta") == 0)
		{
			return snprintf(tag_output_buf, tag_output_buf_len, "\033[1;35m");
		}
		else if (strcasecmp(tag_param_buf, "cyan") == 0)
		{
			return snprintf(tag_output_buf, tag_output_buf_len, "\033[1;36m");
		}
		else if (strcasecmp(tag_param_buf, "black") == 0)
		{
			return snprintf(tag_output_buf, tag_output_buf_len, "\033[1;30;47m");
		}
	}
	return 0;
}

#define LML_TAG_QUOTE_MAX_LEVEL 10
#define LML_TAG_QUOTE_LEVEL_LOOP 3

static const char *lml_tag_quote_color[] = {
	"\033[33m", // yellow
	"\033[32m", // green
	"\033[35m", // magenta
};

static int lml_tag_quote_level = 0;

static int lml_tag_quote_filter(const char *tag_name, const char *tag_param_buf, char *tag_output_buf, size_t tag_output_buf_len)
{
	if (strcasecmp(tag_name, "quote") == 0)
	{
		if (lml_tag_quote_level <= LML_TAG_QUOTE_MAX_LEVEL)
		{
			lml_tag_quote_level++;
		}
		return snprintf(tag_output_buf, tag_output_buf_len, "%s",
						lml_tag_quote_color[lml_tag_quote_level % LML_TAG_QUOTE_LEVEL_LOOP]);
	}
	else if (strcasecmp(tag_name, "/quote") == 0)
	{
		if (lml_tag_quote_level > 0)
		{
			lml_tag_quote_level--;
		}
		return snprintf(tag_output_buf, tag_output_buf_len, "%s",
						(lml_tag_quote_level > 0 ? lml_tag_quote_color[lml_tag_quote_level % LML_TAG_QUOTE_LEVEL_LOOP] : "\033[m"));
	}

	return 0;
}

const static char *lml_tag_def[][4] = {
	// Definition of tuple: {lml_tag, lml_output, default_param | lml_filter_cb, no_lml_output}
	{"left", "[", "", "[left]"},
	{"right", "]", "", "[right]"},
	{"bold", "\033[1m", "", ""}, // does not work in Fterm
	{"/bold", "\033[22m", NULL, ""},
	{"b", "\033[1m", "", ""},
	{"/b", "\033[22m", NULL, ""},
	{"italic", "\033[5m", "", ""},	 // use blink instead
	{"/italic", "\033[m", NULL, ""}, // \033[25m does not work in Fterm
	{"i", "\033[5m", "", ""},
	{"/i", "\033[m", NULL, ""},
	{"underline", "\033[4m", "", ""},
	{"/underline", "\033[m", NULL, ""}, // \033[24m does not work in Fterm
	{"u", "\033[4m", "", ""},
	{"/u", "\033[m", NULL, ""},
	{"color", NULL, (const char *)lml_tag_color_filter, ""},
	{"/color", "\033[m", NULL, ""},
	{"quote", NULL, (const char *)lml_tag_quote_filter, ""},
	{"/quote", NULL, (const char *)lml_tag_quote_filter, ""},
	{"url", "", "", ""},
	{"/url", "（链接: %s）", NULL, ""},
	{"link", "", "", ""},
	{"/link", "（链接: %s）", NULL, ""},
	{"email", "", "", ""},
	{"/email", "（Email: %s）", NULL, ""},
	{"user", "", "", ""},
	{"/user", "（用户: %s）", NULL, ""},
	{"article", "", "", ""},
	{"/article", "（文章: %s）", NULL, ""},
	{"image", "（图片: %s）", "", "%s"},
	{"flash", "（Flash: %s）", "", ""},
	{"bwf", "\033[1;31m****\033[m", "", "****"},
};

#define LML_TAG_COUNT 31

static int lml_tag_name_len[LML_TAG_COUNT];
static int lml_ready = 0;

inline static void lml_init(void)
{
	int i;

	if (!lml_ready)
	{
		for (i = 0; i < LML_TAG_COUNT; i++)
		{
			lml_tag_name_len[i] = (int)strlen(lml_tag_def[i][0]);
		}

		lml_ready = 1;
	}
}

int lml_render(const char *str_in, char *str_out, int buf_len, int lml_tag)
{
	char tag_param_buf[LML_TAG_PARAM_BUF_LEN];
	char tag_output_buf[LML_TAG_OUTPUT_BUF_LEN];
	int i;
	int j = 0;
	int k;
	int tag_start_pos = -1;
	int tag_end_pos = -1;
	int tag_param_pos = -1;
	int tag_output_len;
	int new_line = 1;
	int fb_quote_level = 0;

	lml_init();

	lml_tag_quote_level = 0;

	for (i = 0; str_in[i] != '\0'; i++)
	{
		if (lml_tag && new_line)
		{
			if (fb_quote_level > 0)
			{
				lml_tag_quote_level -= fb_quote_level;

				tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, "%s",
										  (lml_tag_quote_level > 0 ? lml_tag_quote_color[lml_tag_quote_level % LML_TAG_QUOTE_LEVEL_LOOP] : "\033[m"));
				if (j + tag_output_len >= buf_len)
				{
					log_error("Buffer is not longer enough for output string %d >= %d\n", j + tag_output_len, buf_len);
					str_out[j] = '\0';
					return j;
				}
				memcpy(str_out + j, tag_output_buf, (size_t)tag_output_len);
				j += tag_output_len;

				fb_quote_level = 0;
			}

			while (str_in[i + fb_quote_level * 2] == ':' && str_in[i + fb_quote_level * 2 + 1] == ' ') // FB2000 quote leading str
			{
				fb_quote_level++;
			}

			if (fb_quote_level > 0)
			{
				lml_tag_quote_level += fb_quote_level;

				tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, "%s",
										  lml_tag_quote_color[(lml_tag_quote_level) % LML_TAG_QUOTE_LEVEL_LOOP]);
				if (j + tag_output_len >= buf_len)
				{
					log_error("Buffer is not longer enough for output string %d >= %d\n", j + tag_output_len, buf_len);
					str_out[j] = '\0';
					return j;
				}
				memcpy(str_out + j, tag_output_buf, (size_t)tag_output_len);
				j += tag_output_len;
			}

			new_line = 0;
		}

		if (str_in[i] == '\033' && str_in[i + 1] == '[') // Escape sequence -- copy directly
		{
			for (k = i + 2; str_in[k] != '\0' && str_in[k] != 'm'; k++)
				;

			if (str_in[k] == 'm') // Valid
			{
				if (j + (k - i + 1) >= buf_len)
				{
					log_error("Buffer is not longer enough for output string %d >= %d\n", j + (k - i + 1), buf_len);
					str_out[j] = '\0';
					return j;
				}
				memcpy(str_out + j, str_in + i, (size_t)(k - i + 1));
				j += (k - i + 1);
				i = k;
				continue;
			}
			else // reach end of string
			{
				break;
			}
		}

		if (str_in[i] == '\n')
		{
			tag_start_pos = -1; // jump out of tag at end of line
			new_line = 1;
		}
		else if (str_in[i] == '\r')
		{
			continue; // ignore '\r'
		}

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
					if (strncasecmp(lml_tag_def[k][0], str_in + tag_start_pos, (size_t)lml_tag_name_len[k]) == 0)
					{
						tag_param_pos = -1;
						switch (str_in[tag_start_pos + lml_tag_name_len[k]])
						{
						case ' ':
							tag_param_pos = tag_start_pos + lml_tag_name_len[k] + 1;
							while (str_in[tag_param_pos] == ' ')
							{
								tag_param_pos++;
							}
							strncpy(tag_param_buf, str_in + tag_param_pos, (size_t)MIN(tag_end_pos - tag_param_pos, LML_TAG_PARAM_BUF_LEN));
							tag_param_buf[MIN(tag_end_pos - tag_param_pos, LML_TAG_PARAM_BUF_LEN)] = '\0';
						case ']':
							if (tag_param_pos == -1 && lml_tag_def[k][1] != NULL && lml_tag_def[k][2] != NULL) // Apply default param if not defined
							{
								strncpy(tag_param_buf, lml_tag_def[k][2], LML_TAG_PARAM_BUF_LEN - 1);
								tag_param_buf[LML_TAG_PARAM_BUF_LEN - 1] = '\0';
							}
							if (lml_tag)
							{
								if (lml_tag_def[k][1] != NULL)
								{
									tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, lml_tag_def[k][1], tag_param_buf);
								}
								else
								{
									tag_output_len = ((lml_tag_filter_cb)lml_tag_def[k][2])(
										lml_tag_def[k][0], tag_param_buf, tag_output_buf, LML_TAG_OUTPUT_BUF_LEN);
								}
							}
							else
							{
								if (lml_tag_def[k][3] != NULL)
								{
									tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, lml_tag_def[k][3], tag_param_buf);
								}
								else
								{
									tag_output_len = 0;
								}
							}
							if (j + tag_output_len >= buf_len)
							{
								log_error("Buffer is not longer enough for output string %d >= %d\n", j + tag_output_len, buf_len);
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
				if (j + 2 >= buf_len)
				{
					log_error("Buffer is not longer enough for output string %ld >= %d\n", j + 2, buf_len);
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

			if (j + 1 >= buf_len)
			{
				log_error("Buffer is not longer enough for output string %ld >= %d\n", j + 1, buf_len);
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

	if (lml_tag && lml_tag_quote_level > 0)
	{
		tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, "\033[m");
		if (j + tag_output_len >= buf_len)
		{
			log_error("Buffer is not longer enough for output string %d >= %d\n", j + tag_output_len, buf_len);
			str_out[j] = '\0';
			return j;
		}
		memcpy(str_out + j, tag_output_buf, (size_t)tag_output_len);
		j += tag_output_len;
	}

	str_out[j] = '\0';

	return j;
}
