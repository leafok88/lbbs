/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * lml
 *   - LML render
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "lml.h"
#include "log.h"
#include "str_process.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

enum _lml_constant_t
{
	LML_TAG_PARAM_BUF_LEN = 256,
	LML_TAG_OUTPUT_BUF_LEN = 1024,
	LML_TAG_QUOTE_MAX_LEVEL = 10,
};

clock_t lml_total_exec_duration = 0;

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

static const int lml_tag_count = sizeof(lml_tag_def) / sizeof(LML_TAG_DEF);
static int lml_tag_name_len[sizeof(lml_tag_def) / sizeof(LML_TAG_DEF)];
static int lml_ready = 0;

inline static void lml_init(void)
{
	int i;

	if (!lml_ready)
	{
		for (i = 0; i < lml_tag_count; i++)
		{
			lml_tag_name_len[i] = (int)strlen(lml_tag_def[i].tag_name);
		}

		lml_ready = 1;
	}
}

#define CHECK_AND_APPEND_OUTPUT(out_buf, out_buf_len, out_buf_offset, tag_out, tag_out_len, line_width)                             \
	if ((tag_out_len) > 0)                                                                                                          \
	{                                                                                                                               \
		if ((out_buf_offset) + (tag_out_len) >= (out_buf_len))                                                                      \
		{                                                                                                                           \
			log_error("Buffer is not longer enough for output string %d >= %d\n", (out_buf_offset) + (tag_out_len), (out_buf_len)); \
			out_buf[out_buf_offset] = '\0';                                                                                         \
			return (out_buf_offset);                                                                                                \
		}                                                                                                                           \
		memcpy((out_buf) + (out_buf_offset), (tag_out), (size_t)(tag_out_len));                                                     \
		*((out_buf) + (out_buf_offset) + (size_t)(tag_out_len)) = '\0';                                                             \
		(line_width) += str_length((out_buf) + (out_buf_offset), 1);                                                                \
		(out_buf_offset) += (tag_out_len);                                                                                          \
	}

int lml_render(const char *str_in, char *str_out, int buf_len, int width, int quote_mode)
{
	clock_t clock_begin;
	clock_t clock_end;

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
	int line_width = 0;
	char tab_spaces[TAB_SIZE + 1];
	int tab_width = 0;

#ifdef _DEBUG
	size_t str_in_len = strlen(str_in);
#endif

	clock_begin = clock();

	lml_init();

	lml_tag_disabled = 0;
	lml_tag_quote_level = 0;

	if (width <= 0)
	{
		width = INT_MAX;
	}

	for (i = 0; str_in[i] != '\0'; i++)
	{
#ifdef _DEBUG
		if (i >= str_in_len)
		{
			log_error("Bug: i(%d) >= str_in_len(%d)\n", i, str_in_len);
			break;
		}
#endif

		if (!lml_tag_disabled && new_line)
		{
			while (str_in[i] == ':' && str_in[i + 1] == ' ') // FB2000 quote leading str
			{
				fb_quote_level++;
				lml_tag_quote_level++;
				i += 2;
			}

			if (!quote_mode && lml_tag_quote_level > 0)
			{
				tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, "%s",
										  lml_tag_quote_color[lml_tag_quote_level % LML_TAG_QUOTE_LEVEL_LOOP]);
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, tag_output_buf, tag_output_len, line_width);
			}

			for (k = 0; k < fb_quote_level; k++)
			{
				tag_output_buf[k * 2] = ':';
				tag_output_buf[k * 2 + 1] = ' ';
			}
			tag_output_buf[fb_quote_level * 2] = '\0';
			tag_output_len = fb_quote_level * 2;
			CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, tag_output_buf, tag_output_len, line_width);

			new_line = 0;
			i--; // redo at current i
			continue;
		}

		if (lml_tag_disabled && new_line)
		{
			new_line = 0;
		}

		if (!quote_mode && !lml_tag_disabled && str_in[i] == '\033' && str_in[i + 1] == '[') // Escape sequence
		{
			for (k = i + 2; isdigit((int)str_in[k]) || str_in[k] == ';' || str_in[k] == '?'; k++)
				;

			if (str_in[k] == 'm') // valid -- copy directly
			{
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, str_in + i, k - i + 1, line_width);
			}
			else if (isalpha((int)str_in[k]))
			{
				// unsupported ANSI CSI command
			}
			else
			{
				k--;
			}

			i = k;
			continue;
		}

		if (str_in[i] == '\n') // jump out of tag at end of line
		{
			if (!lml_tag_disabled && tag_start_pos != -1) // tag is not closed
			{
				if (line_width + 1 > width)
				{
					CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, "\n", 1, line_width);
					new_line = 1;
					line_width = 0;
					i--; // redo at current i
					continue;
				}

				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, "[", 1, line_width);
				i = tag_start_pos; // restart from tag_start_pos + 1
				tag_start_pos = -1;
				tag_name_pos = -1;
				continue;
			}

			if (!lml_tag_disabled && fb_quote_level > 0)
			{
				lml_tag_quote_level -= fb_quote_level;

				tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, (quote_mode ? "" : "\033[m"));
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, tag_output_buf, tag_output_len, line_width);

				fb_quote_level = 0;
			}

			if (new_line)
			{
				continue;
			}

			tag_start_pos = -1;
			tag_name_pos = -1;
			new_line = 1;
			line_width = -1;
		}
		else if (str_in[i] == '\r' || str_in[i] == '\7')
		{
			continue; // Skip special characters
		}
		else if (str_in[i] == '\t')
		{
			tab_width = TAB_SIZE - (line_width % TAB_SIZE);
			if (line_width + tab_width > width)
			{
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, "\n", 1, line_width);
				new_line = 1;
				line_width = 0;
				// skip current Tab
				continue;
			}

			for (k = 0; k < tab_width; k++)
			{
				tab_spaces[k] = ' ';
			}
			tab_spaces[tab_width] = '\0';
			CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, tab_spaces, tab_width, line_width);
			continue;
		}

		if (!lml_tag_disabled && str_in[i] == '[')
		{
			if (tag_start_pos != -1) // tag is not closed
			{
				if (line_width + 1 > width)
				{
					CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, "\n", 1, line_width);
					new_line = 1;
					line_width = 0;
					i--; // redo at current i
					continue;
				}

				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, "[", 1, line_width);
				i = tag_start_pos; // restart from tag_start_pos + 1
				tag_start_pos = -1;
				tag_name_pos = -1;
				continue;
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

			for (tag_name_found = 0, k = 0; k < lml_tag_count; k++)
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

						if (line_width + tag_output_len > width)
						{
							CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, "\n", 1, line_width);
							new_line = 1;
							line_width = 0;
							i--; // redo at current i
							continue;
						}

						CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, tag_output_buf, tag_output_len, line_width);
						break;
					default: // tag_name not match
						continue;
					}
					break;
				}
			}

			if (!tag_name_found)
			{
				if (line_width + 1 > width)
				{
					CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, "\n", 1, line_width);
					new_line = 1;
					line_width = 0;
					i--; // redo at current i
					continue;
				}

				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, "[", 1, line_width);
				i = tag_start_pos; // restart from tag_start_pos + 1
				tag_start_pos = -1;
				tag_name_pos = -1;
				continue;
			}

			tag_start_pos = -1;
			tag_name_pos = -1;
		}
		else if (lml_tag_disabled || tag_name_pos == -1) // not in LML tag
		{
			if (line_width + (str_in[i] & 0x80 ? 2 : 1) > width)
			{
				CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, "\n", 1, line_width);
				new_line = 1;
				line_width = 0;
				i--; // redo at current i
				continue;
			}

			tag_output_len = 1;
			if (str_in[i] & 0x80) // head of multi-byte character
			{
				c = (str_in[i] & 0x70) << 1;
				while (c & 0x80)
				{
					if (str_in[i + tag_output_len] == '\0')
					{
						break;
					}
					tag_output_len++;
					c = (c & 0x7f) << 1;
				}
			}

			CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, str_in + i, tag_output_len, line_width);
			i += (tag_output_len - 1);
		}
		else // in LML tag
		{
			// Do nothing
		}
	}

	if (!lml_tag_disabled && tag_start_pos != -1) // tag is not closed
	{
		tag_end_pos = i - 1;
		tag_output_len = tag_end_pos - tag_start_pos + 1;
		CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, str_in + tag_start_pos, tag_output_len, line_width);
	}

	if (!quote_mode && !lml_tag_disabled && lml_tag_quote_level > 0)
	{
		tag_output_len = snprintf(tag_output_buf, LML_TAG_OUTPUT_BUF_LEN, "\033[m");
		CHECK_AND_APPEND_OUTPUT(str_out, buf_len, j, tag_output_buf, tag_output_len, line_width);
	}

	str_out[j] = '\0';

	clock_end = clock();
	lml_total_exec_duration += (clock_end - clock_begin);

	return j;
}
