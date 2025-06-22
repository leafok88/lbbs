/***************************************************************************
						  str_process.c  -  description
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
#include "log.h"
#include "str_process.h"
#include <stdio.h>
#include <string.h>

/**
* 检测并跳过ANSI控制序列
*
* @param buffer 输入字符串
* @param index 当前处理的字符索引
* @return int 处理后的新索引位置
*/
static int skip_ansi_control_sequence(const char *buffer, int index)
{
	assert(buffer != NULL);
	
	// 确认这是一个ANSI控制序列的开始
	if (buffer[index] == '\033' && buffer[index + 1] == '[') {
		index += 2; //跳过ESC[
		
		// 跳过控制序列直到结束符'm'或字符串结束
		while (buffer[index] != '\0' && buffer[index] != 'm') {
			index++;
		}

		// 如果找到了结束符'm',跳过它
		if (buffer[index] == 'm') {
			index++;
		}
	}
	
	return index;
}

/**
* 计算字符的显示宽度
* 
* @param c 要计算的字符
* @return int 字符的显示宽度（1或2）
*/
static inline int get_char_display_width(unsigned char c)
{
	// GBK中文字符(首字母范围)
	// GBK编码中,汉字的第一个字节的范围通常为0x81-0xFE
	return (c >= 0x81) ? 2 : 1;
}

int split_line(const char *buffer, int max_display_len, int *p_eol, int *p_display_len, int skip_ctrl_seq)
{
	if (!buffer || !p_eol || !p_display_len) {
		log_error("split_line: Invalid parameters\n");
		return 0;
	}
	
	int i = 0;
	*p_eol = 0;
	*p_display_len = 0;
	char c;

	while ((c = (unsigned char)buffer[i]) != '\0') {
		// 跳过回车和响铃控制字符
		if (c == '\r' || c == '\7') {
			i++;
			continue;
		}

		// 处理ANSI控制序列
		if (skip_ctrl_seq && c == '\033' && buffer[i + 1] == '['){
			i = skip_ansi_control_sequence(buffer, i);
			continue;
		}

		// 处理字符的显示宽度
		int char_width = get_char_display_width(c);

		// 检查是否超出最大显示长度
		if (*p_display_len + char_width > max_display_len) {
			break;
		}

		// 中文字符处理(GBK编码)
		if (char_width == 2) {
			i += 2; //中文字符占2个字节
			*p_display_len += 2；
		} else {
			i++;
			(*p_display_len)++;

			//换行符处理
			if (c == '\n') {
				*p_eol = 1;
				break;
			}
		}
	}

	return i;
}

long split_data_lines(const char *p_buf, int max_display_len, long *p_line_offsets, long line_offsets_count, int skip_ctrl_seq)
{
	int line_cnt = 0;
	int len;
	int end_of_line = 0;
	int display_len = 0;

	p_line_offsets[line_cnt] = 0L;

	do
	{
		len = split_line(p_buf, max_display_len, &end_of_line, &display_len, skip_ctrl_seq);

		// Exceed max_line_cnt
		if (line_cnt + 1 >= line_offsets_count)
		{
			// log_error("Line count %d reaches limit %d\n", line_cnt + 1, line_offsets_count);
			return line_cnt;
		}

		p_line_offsets[line_cnt + 1] = p_line_offsets[line_cnt] + len;
		line_cnt++;
		p_buf += len;
	} while (p_buf[0] != '\0');

	return line_cnt;
}

int str_filter(char *buffer, int skip_ctrl_seq)
{
	int i;
	int j;

	for (i = 0, j = 0; buffer[i] != '\0'; i++)
	{
		if (buffer[i] == '\r' || buffer[i] == '\7') // skip
		{
			continue;
		}

		if (skip_ctrl_seq && buffer[i] == '\033' && buffer[i + 1] == '[') // Skip control sequence
		{
			i += 2;
			while (buffer[i] != '\0' && buffer[i] != 'm')
			{
				i++;
			}
			continue;
		}

		buffer[j] = buffer[i];
		j++;
	}

	buffer[j] = '\0';

	return j;
}
