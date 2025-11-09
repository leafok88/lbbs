/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_file_mgr
 *   - Edit / Preview data files
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#include "bbs.h"
#include "bbs_cmd.h"
#include "common.h"
#include "data_file_mgr.h"
#include "editor.h"
#include "log.h"
#include "screen.h"
#include "str_process.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

enum _data_file_mgr_constant_t
{
	DATA_FILE_MAX_LEN = 1024 * 1024 * 4, // 4MB
};

struct data_file_item_t
{
	const char *const name;
	const char *const path;
};
typedef struct data_file_item_t DATA_FILE_ITEM;

static const DATA_FILE_ITEM data_file_list[] = {
	{"进站欢迎", DATA_WELCOME},
	{"离站告别", DATA_GOODBYE},
	{"登陆错误", DATA_LOGIN_ERROR},
	{"活动看板", DATA_ACTIVE_BOARD},
	{"浏览帮助", DATA_READ_HELP},
	{"编辑帮助", DATA_EDITOR_HELP},
	{"敏感词表", CONF_BWF},
	{"穿梭配置", CONF_BBSNET},
};

static const int data_file_cnt = sizeof(data_file_list) / sizeof(DATA_FILE_ITEM);

int data_file_mgr(void *param)
{
	char path[FILE_PATH_LEN];
	int fd = -1;
	struct stat sb;
	void *p_data = NULL;
	size_t len_data = 0;
	EDITOR_DATA *p_editor_data = NULL;
	long len_data_new = 0;
	char *p_data_new = NULL;
	long line_total;
	long line_offsets[MAX_SPLIT_FILE_LINES];
	char buf[3] = "";
	int ch = 0;
	int previewed = 0;
	int i;

	clearscr();
	moveto(1, 1);
	prints("数据文件列表: ");

	for (i = 0; i < data_file_cnt; i++)
	{
		moveto(3 + i, 4);
		prints("%2d. %s", i + 1, data_file_list[i].name);
	}

	get_data(SCREEN_ROWS, 1, "请输入需要编辑的文件编号: ", buf, sizeof(buf), 2);
	i = atoi(buf) - 1;
	if (i < 0 || i >= data_file_cnt)
	{
		return REDRAW;
	}

	snprintf(path, sizeof(path), "%s.user", data_file_list[i].path);
	if ((fd = open(path, O_RDONLY)) < 0 &&
		(fd = open(data_file_list[i].path, O_RDONLY)) < 0)
	{
		log_error("open(%s) error (%d)\n", data_file_list[i].path, errno);
		return REDRAW;
	}

	if (fstat(fd, &sb) < 0)
	{
		log_error("fstat(fd) error (%d)\n", errno);
		goto cleanup;
	}

	len_data = (size_t)sb.st_size;
	p_data = mmap(NULL, len_data, PROT_READ, MAP_SHARED, fd, 0L);
	if (p_data == MAP_FAILED)
	{
		log_error("mmap() error (%d)\n", errno);
		goto cleanup;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)\n", errno);
		fd = -1;
		goto cleanup;
	}
	fd = -1;

	p_editor_data = editor_data_load(p_data);
	if (p_editor_data == NULL)
	{
		log_error("editor_data_load(data) error\n");
		goto cleanup;
	}

	if (munmap(p_data, len_data) < 0)
	{
		log_error("munmap() error (%d)\n", errno);
		p_data = NULL;
		goto cleanup;
	}
	p_data = NULL;

	p_data_new = malloc(DATA_FILE_MAX_LEN);
	if (p_data_new == NULL)
	{
		log_error("malloc(content) error: OOM\n");
		goto cleanup;
	}

	for (ch = 'E'; !SYS_server_exit;)
	{
		if (ch == 'E')
		{
			editor_display(p_editor_data);
			previewed = 0;
		}

		clearscr();
		moveto(1, 1);
		if (previewed)
		{
			prints("(S)保存, (P)预览, (C)取消 or (E)再编辑? [P]: ");
		}
		else
		{
			prints("(P)预览, (C)取消 or (E)再编辑? [P]: ");
		}
		iflush();

		ch = igetch_t(BBS_max_user_idle_time);
		switch (toupper(ch))
		{
		case KEY_NULL:
		case KEY_TIMEOUT:
			goto cleanup;
		case CR:
		case 'P':
			len_data_new = editor_data_save(p_editor_data, p_data_new, DATA_FILE_MAX_LEN);
			if (len_data_new < 0)
			{
				log_error("editor_data_save() error\n");
				goto cleanup;
			}
			line_total = split_data_lines(p_data_new, SCREEN_COLS, line_offsets, MAX_SPLIT_FILE_LINES, 1, NULL);
			display_data(p_data_new, line_total, line_offsets, 1, display_file_key_handler, DATA_READ_HELP);
			previewed = 1;
			continue;
		case 'S':
			if (!previewed)
			{
				continue;
			}
			break;
		case 'C':
			clearscr();
			moveto(1, 1);
			prints("取消更改...");
			press_any_key();
			goto cleanup;
		case 'E':
			ch = 'E';
			continue;
		default: // Invalid selection
			continue;
		}

		break;
	}

	if (SYS_server_exit) // Do not save data on shutdown
	{
		goto cleanup;
	}

	if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0640)) < 0)
	{
		log_error("open(%s) error (%d)\n", path, errno);
		goto cleanup;
	}

	if (write(fd, p_data_new, (size_t)len_data_new) < 0)
	{
		log_error("write(len=%ld) error (%d)\n", len_data_new, errno);
		goto cleanup;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)\n", errno);
		fd = -1;
		goto cleanup;
	}

	// Make another copy of the data file so that the user editored copy will be kept after upgrade / reinstall
	if ((fd = open(data_file_list[i].path, O_WRONLY | O_CREAT | O_TRUNC, 0640)) < 0)
	{
		log_error("open(%s) error (%d)\n", data_file_list[i].path, errno);
		goto cleanup;
	}

	if (write(fd, p_data_new, (size_t)len_data_new) < 0)
	{
		log_error("write(len=%ld) error (%d)\n", len_data_new, errno);
		goto cleanup;
	}

	if (close(fd) < 0)
	{
		log_error("close(fd) error (%d)\n", errno);
		fd = -1;
		goto cleanup;
	}
	fd = -1;

	clearscr();
	moveto(1, 1);
	prints("修改已完成，需要重新加载配置后才会生效...");
	press_any_key();

cleanup:
	if (p_editor_data != NULL)
	{
		editor_data_cleanup(p_editor_data);
	}

	if (p_data_new != NULL)
	{
		free(p_data_new);
	}

	if (p_data != NULL && munmap(p_data, len_data) < 0)
	{
		log_error("munmap() error (%d)\n", errno);
	}

	if (fd >= 0 && close(fd) < 0)
	{
		log_error("close(fd) error (%d)\n", errno);
	}

	return REDRAW;
}
