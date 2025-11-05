/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * user_info_update
 *   - update user information
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#include "bbs.h"
#include "database.h"
#include "editor.h"
#include "io.h"
#include "log.h"
#include "screen.h"
#include "str_process.h"
#include "user_list.h"
#include "user_priv.h"
#include <ctype.h>
#include <stdlib.h>
#include <sys/param.h>

#define BBS_user_intro_line_len 256

int user_intro_edit(int uid)
{
    MYSQL *db = NULL;
    MYSQL_RES *rs = NULL;
    MYSQL_ROW row;
    char sql_intro[SQL_BUFFER_LEN + 2 * BBS_user_intro_max_len + 1];
    EDITOR_DATA *p_editor_data = NULL;
    int ret = 0;
    int ch = 0;
    char intro[BBS_user_intro_max_len + 1];
    char intro_f[BBS_user_intro_max_len + 1];
    long len_intro = 0L;
    long line_offsets[BBS_user_intro_max_line + 1];
    long lines = 0L;

    db = db_open();
    if (db == NULL)
    {
        log_error("db_open() error: %s\n", mysql_error(db));
        ret = -1;
        goto cleanup;
    }

    snprintf(sql_intro, sizeof(sql_intro), "SELECT introduction FROM user_pubinfo WHERE UID=%d", uid);
    if (mysql_query(db, sql_intro) != 0)
    {
        log_error("Query user_pubinfo error: %s\n", mysql_error(db));
        ret = -1;
        goto cleanup;
    }
    if ((rs = mysql_store_result(db)) == NULL)
    {
        log_error("Get user_intro data failed\n");
        ret = -1;
        goto cleanup;
    }
    if ((row = mysql_fetch_row(rs)))
    {
        p_editor_data = editor_data_load(row[0] ? row[0] : "");
    }
    else
    {
        log_error("mysql_fetch_row failed\n");
        ret = -1;
        goto cleanup;
    }
    mysql_free_result(rs);
    mysql_close(db);
    rs = NULL;
    db = NULL;

    editor_display(p_editor_data);

    while (!SYS_server_exit)
    {
        clearscr();
        moveto(1, 1);
        prints("(S)保存, (C)取消 or (E)再编辑? [S]: ");
        iflush();

        ch = igetch_t(BBS_max_user_idle_time);
        switch (toupper(ch))
        {
        case KEY_NULL:
        case KEY_TIMEOUT:
            goto cleanup;
        case CR:
        case 'S':
            len_intro = editor_data_save(p_editor_data, intro, BBS_user_intro_max_len);
            if (len_intro < 0)
            {
                log_error("editor_data_save() error\n");
                ret = -1;
                goto cleanup;
            }
            lines = split_data_lines(intro, BBS_user_intro_line_len, line_offsets, BBS_user_intro_max_line + 2, 1, NULL);

            if (lines > BBS_user_intro_max_line)
            {
                clearscr();
                moveto(1, 1);
                prints("说明档限%d行以内。", BBS_user_intro_max_line);
                press_any_key();
                editor_display(p_editor_data);

                continue;
            }
            break;
        case 'C':
            clearscr();
            moveto(1, 1);
            prints("取消...");
            press_any_key();
            goto cleanup;
        case 'E':
            editor_display(p_editor_data);
        default: // Invalid selection
            continue;
        }

        break;
    }

    db = db_open();
    if (db == NULL)
    {
        log_error("db_open() error: %s\n", mysql_error(db));
        ret = -1;
        goto cleanup;
    }

    // Secure SQL parameters
    mysql_real_escape_string(db, intro_f, intro, (unsigned long)len_intro);

    // Update user intro
    snprintf(sql_intro, sizeof(sql_intro),
             "UPDATE user_pubinfo set introduction = '%s' where UID=%d",
             intro_f, uid);

    if (mysql_query(db, sql_intro) != 0)
    {
        log_error("Edit user intro error: %s\n", mysql_error(db));
        ret = -1;
        goto cleanup;
    }

    clearscr();
    moveto(1, 1);
    prints("说明档修改完成，新内容通常会在%d秒后可见",BBS_user_list_load_interval);
    press_any_key();
    ret = 1; // Success

cleanup:
    mysql_free_result(rs);
    mysql_close(db);

    // Cleanup buffers
    editor_data_cleanup(p_editor_data);

    return ret;
}