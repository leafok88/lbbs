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

typedef int (*lml_tag_filter_cb)(const char *tag_name, const char *tag_param_buf, char *tag_output_buf, size_t tag_output_buf_len, int quote_mode);

static int lml_tag_color_filter(const char *tag_name, const char *tag_param_buf, char *tag_output_buf, size_t tag_output_buf_len, int quote_mode)
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

static const char *lml_tag_quote_color[] = {
	"\033[33m", // yellow
	"\033[32m", // green
	"\033[35m", // magenta
};

static const int LML_TAG_QUOTE_LEVEL_LOOP = (int)(sizeof(lml_tag_quote_color) / sizeof(const char *));

static int lml_tag_quote_level = 0;

static int lml_tag_quote_filter(const char *tag_name, const char *tag_param_buf, char *tag_output_buf, size_t tag_output_buf_len, int quote_mode)
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

static int lml_tag_disabled = 0;

static int lml_tag_disable_filter(const char *tag_name, const char *tag_param_buf, char *tag_output_buf, size_t tag_output_buf_len, int quote_mode)
{
	lml_tag_disabled = 1;

	return snprintf(tag_output_buf, tag_output_buf_len, "%s", (quote_mode ? "[plain]" : ""));
}

typedef struct lml_tag_def_t
{
	const char *tag_name;			 // tag name
	const char *tag_output;			 // output string
	const char *default_param;		 // default param string
	const char *quote_mode_output;	 // output string in quote mode
	lml_tag_filter_cb tag_filter_cb; // tag filter callback
} LML_TAG_DEF;

const LML_TAG_DEF lml_tag_def[] = {
	// Definition of tuple: {lml_tag, lml_output, default_param, quote_mode_output, lml_filter_cb}
	{"plain", NULL, NULL, NULL, lml_tag_disable_filter},
	{"nolml", "", NULL, "", NULL}, // deprecated
	{"lml", "", NULL, "", NULL},   // deprecated
	{"align", "", "", "", NULL},   // N/A
	{"/align", "", "", "", NULL},  // N/A
	{"size", "", "", "", NULL},	   // N/A
	{"/size", "", "", "", NULL},   // N/A
	{"left", "[", "", "[left]", NULL},
	{"right", "]", "", "[right]", NULL},
	{"bold", "\033[1m", "", "", NULL}, // does not work in Fterm
	{"/bold", "\033[22m", NULL, "", NULL},
	{"b", "\033[1m", "", "", NULL},
	{"/b", "\033[22m", NULL, "", NULL},
	{"italic", "\033[5m", "", "", NULL},   // use blink instead
	{"/italic", "\033[m", NULL, "", NULL}, // \033[25m does not work in Fterm
	{"i", "\033[5m", "", "", NULL},
	{"/i", "\033[m", NULL, "", NULL},
	{"underline", "\033[4m", "", "", NULL},
	{"/underline", "\033[m", NULL, "", NULL}, // \033[24m does not work in Fterm
	{"u", "\033[4m", "", "", NULL},
	{"/u", "\033[m", NULL, "", NULL},
	{"color", NULL, NULL, "", lml_tag_color_filter},
	{"/color", "\033[m", NULL, "", NULL},
	{"quote", NULL, NULL, "", lml_tag_quote_filter},
	{"/quote", NULL, NULL, "", lml_tag_quote_filter},
	{"url", "", "", "", NULL},
	{"/url", "（链接: %s）", NULL, "", NULL},
	{"link", "", "", "", NULL},
	{"/link", "（链接: %s）", NULL, "", NULL},
	{"email", "", "", "", NULL},
	{"/email", "（Email: %s）", NULL, "", NULL},
	{"user", "", "", "", NULL},
	{"/user", "（用户: %s）", NULL, "", NULL},
	{"article", "", "", "", NULL},
	{"/article", "（文章: %s）", NULL, "", NULL},
	{"image", "（图片: %s）", "", "%s", NULL},
	{"flash", "（Flash: %s）", "", "", NULL},
	{"bwf", "\033[1;31m****\033[m", "", "****", NULL},
};

#define LML_TAG_COUNT (sizeof(lml_tag_def) / sizeof(LML_TAG_DEF))

static int lml_tag_name_len[LML_TAG_COUNT];
static int lml_ready = 0;

inline static void lml_init(void)
{
	int i;

	if (!lml_ready)
	{
		for (i = 0; i < LML_TAG_COUNT; i++)
		{
			lml_tag_name_len[i] = (int)strlen(lml_tag_def[i].tag_name);
		}

		lml_ready = 1;
	}
}

#define CHECK_AND_APPEND_OUTPUT(out_buf, out_buf_len, out_buf_offset, tag_out, tag_out_len) \
{ \
	if ((out_buf_offset) + (tag_out_len) >= (out_buf_len)) \
	{ \
		log_error("Buffer is not longer enough for output string %d >= %d\n", (out_buf_offset) + (tag_out_len), (out_buf_len)); \
		out_buf[out_buf_offset] = '\0'; \
		return (out_buf_offset); \
	} \
	memcpy((out_buf) + (out_buf_offset), (tag_out), (size_t)(tag_out_len)); \
	(out_buf_offset) += (tag_out_len); \
}

int lml_render(const char *str_in, char *str_out, int buf_len, int quote_mode)
{
	char c;
	char tag_param_buf[LML_TAG_PARAM_BUF_LEN];
	char tag_output_buf[LML_TAG_OUTPUT_BUF_LEN];
	int i;
	int j = 0;
	int k;
	int tag_start_pos = -1;
	int tag_name_pos = -1;
	int tag_end_pos = -1;
	int tag_param_pos = -1;
	int tag_output_len;
	int new_line = 1;
	int fb_quote_level = 0;
	int tag_name_found;

	lml_init();

	lml_tag_disabled = 0;
	lml_tag_quote_level = 0;

	for (i = 0; str_in[i] != '\0'; i++)
	{
		if (!quote_mode && !lml_tag_disabled && new_line)
		{
			fb_quote_level = 0;

			while (str_in[i + fb_quote_level * 2] == ':' && str_in[i + fb_quote_level * 2 + 1] == ' ') // FB2000 quote leading str
			{
				fb_quote_level++;
			}

			lml_tag_quote_level += fb_quote_level;

			if (lml_tag_quote_level > 0)
			{
				tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, "%s",
										  lml_tag_quote_color[lml_tag_quote_level % LML_TAG_QUOTE_LEVEL_LOOP]);
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, tag_output_buf, tag_output_len);
			}

			new_line = 0;
		}

		if (str_in[i] == '\033' && str_in[i + 1] == '[') // Escape sequence -- copy directly
		{
			for (k = i + 2; str_in[k] != '\0' && str_in[k] != 'm'; k++)
				;

			if (str_in[k] == 'm') // Valid
			{
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, str_in + i, k - i + 1);
				i = k;
				continue;
			}
			else // reach end of string
			{
				break;
			}
		}

		if (str_in[i] == '\n') // jump out of tag at end of line
		{
			if (tag_start_pos != -1) // tag is not closed
			{
				tag_end_pos = i - 1;
				tag_output_len = tag_end_pos - tag_start_pos + 1;
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, str_in + tag_start_pos, tag_output_len);
			}

			if (fb_quote_level > 0)
			{
				lml_tag_quote_level -= fb_quote_level;

				tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, "\033[m");
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, tag_output_buf, tag_output_len);
			}

			tag_start_pos = -1;
			tag_name_pos = -1;
			new_line = 1;
		}
		else if (str_in[i] == '\r')
		{
			continue; // ignore '\r'
		}

		if (!lml_tag_disabled && str_in[i] == '[')
		{
			if (tag_start_pos != -1) // tag is not closed
			{
				tag_end_pos = i - 1;
				tag_output_len = tag_end_pos - tag_start_pos + 1;
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, str_in + tag_start_pos, tag_output_len);
			}

			tag_start_pos = i;
			tag_name_pos = i + 1;
		}
		else if (!lml_tag_disabled && str_in[i] == ']' && tag_name_pos >= 0)
		{
			tag_end_pos = i;

			// Skip space characters
			while (str_in[tag_name_pos] == ' ')
			{
				tag_name_pos++;
			}

			for (tag_name_found = 0, k = 0; k < LML_TAG_COUNT; k++)
			{
				if (strncasecmp(lml_tag_def[k].tag_name, str_in + tag_name_pos, (size_t)lml_tag_name_len[k]) == 0)
				{
					tag_param_pos = -1;
					switch (str_in[tag_name_pos + lml_tag_name_len[k]])
					{
					case ' ':
						tag_name_found = 1;
						tag_param_pos = tag_name_pos + lml_tag_name_len[k] + 1;
						while (str_in[tag_param_pos] == ' ')
						{
							tag_param_pos++;
						}
						strncpy(tag_param_buf, str_in + tag_param_pos, (size_t)MIN(tag_end_pos - tag_param_pos, LML_TAG_PARAM_BUF_LEN));
						tag_param_buf[MIN(tag_end_pos - tag_param_pos, LML_TAG_PARAM_BUF_LEN)] = '\0';
					case ']':
						tag_name_found = 1;
						if (tag_param_pos == -1 && lml_tag_def[k].tag_output != NULL && lml_tag_def[k].default_param != NULL) // Apply default param if not defined
						{
							strncpy(tag_param_buf, lml_tag_def[k].default_param, LML_TAG_PARAM_BUF_LEN - 1);
							tag_param_buf[LML_TAG_PARAM_BUF_LEN - 1] = '\0';
						}
						tag_output_len = 0;
						if (!quote_mode)
						{
							if (lml_tag_def[k].tag_output != NULL)
							{
								tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, lml_tag_def[k].tag_output, tag_param_buf);
							}
							else if (lml_tag_def[k].tag_filter_cb != NULL)
							{
								tag_output_len = lml_tag_def[k].tag_filter_cb(
									lml_tag_def[k].tag_name, tag_param_buf, tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, 0);
							}
						}
						else // if (quote_mode)
						{
							if (lml_tag_def[k].quote_mode_output != NULL)
							{
								tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, lml_tag_def[k].quote_mode_output, tag_param_buf);
							}
							else if (lml_tag_def[k].tag_filter_cb != NULL)
							{
								tag_output_len = lml_tag_def[k].tag_filter_cb(
									lml_tag_def[k].tag_name, tag_param_buf, tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, 1);
							}
						}
						CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, tag_output_buf, tag_output_len);
						break;
					default: // tag_name not match
						continue;
					}
					break;
				}
			}

			if (!tag_name_found)
			{
				tag_output_len = tag_end_pos - tag_start_pos + 1;
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, str_in + tag_start_pos, tag_output_len);
			}

			tag_start_pos = -1;
			tag_name_pos = -1;
		}
		else if (lml_tag_disabled || tag_name_pos == -1) // not in LML tag
		{
			if (str_in[i] & 0x80) // head of multi-byte character
			{
				if (j + 4 >= buf_len) // Assuming UTF-8 CJK characters use 4 bytes, though most of them actually use 3 bytes
				{
					log_error("Buffer is not longer enough for output string %ld >= %d\n", j + 4, buf_len);
					str_out[j] = '\0';
					return j;
				}

				c = (str_in[i] & 0x70) << 1;
				while (c & 0x80)
				{
					str_out[j++] = str_in[i++];
					if (str_in[i] == '\0')
					{
						str_out[j] = '\0';
						return j;
					}
					c = (c & 0x7f) << 1;
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

	if (tag_start_pos != -1) // tag is not closed
	{
		tag_end_pos = i - 1;
		tag_output_len = tag_end_pos - tag_start_pos + 1;
		CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, str_in + tag_start_pos, tag_output_len);
	}

	if (!quote_mode && !lml_tag_disabled && lml_tag_quote_level > 0)
	{
		tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, "\033[m");
		CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, tag_output_buf, tag_output_len);
	}

	str_out[j] = '\0';

	return j;
}
