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

#define USER_INTRO_MAX_LEN 1280

int user_intro_edit(int uid)
{
    MYSQL *db = NULL;
    MYSQL_RES *rs = NULL;
    char *sql_content = NULL;
    EDITOR_DATA *p_editor_data = NULL;
    USER_INFO user_info;
    int ret = 0;
    int ch = 0;
    char *intro = NULL;
    char *intro_f = NULL;
    long len_intro = 0L;

    clearscr();

    if ((ret = query_user_info_by_uid(uid, &user_info)) < 0)
    {
        log_error("query_user_info_by_uid(uid=%d) error\n", uid);
        return -2;
    }
    p_editor_data = editor_data_load(user_info.intro);

    editor_display(p_editor_data);

    while (!SYS_server_exit)
    {
        clearscr();
        moveto(1, 1);
        prints("(S)保存, (C)取消 or (E)再编辑? [S]: ");
        iflush();

        ch = igetch_t(MAX_DELAY_TIME);
        switch (toupper(ch))
        {
        case KEY_NULL:
        case KEY_TIMEOUT:
            goto cleanup;
        case CR:
        case 'S':
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
    if (SYS_server_exit) // Do not save data on shutdown
    {
        goto cleanup;
    }

    intro = malloc(USER_INTRO_MAX_LEN);
    if (intro == NULL)
    {
        log_error("malloc(content) error: OOM\n");
        ret = -1;
        goto cleanup;
    }

    len_intro = editor_data_save(p_editor_data, intro, USER_INTRO_MAX_LEN);
    if (len_intro < 0)
    {
        log_error("editor_data_save() error\n");
        ret = -1;
        goto cleanup;
    }

    db = db_open();
    if (db == NULL)
    {
        log_error("db_open() error: %s\n", mysql_error(db));
        ret = -1;
        goto cleanup;
    }

    // Begin transaction
    if (mysql_query(db, "SET autocommit=0") != 0)
    {
        log_error("SET autocommit=0 error: %s\n", mysql_error(db));
        ret = -1;
        goto cleanup;
    }

    if (mysql_query(db, "BEGIN") != 0)
    {
        log_error("Begin transaction error: %s\n", mysql_error(db));
        ret = -1;
        goto cleanup;
    }

    // Secure SQL parameters
    intro_f = malloc((size_t)len_intro * 2 + 1);
    if (intro_f == NULL)
    {
        log_error("malloc(intro_f) error: OOM\n");
        ret = -1;
        goto cleanup;
    }

    mysql_real_escape_string(db, intro_f, intro, (unsigned long)len_intro);

    free(intro);
    intro = NULL;

    sql_content = malloc(SQL_BUFFER_LEN + (size_t)len_intro * 2 + 1);
    if (sql_content == NULL)
    {
        log_error("malloc(sql_content) error: OOM\n");
        ret = -1;
        goto cleanup;
    }

    // Update user intro
    snprintf(sql_content, SQL_BUFFER_LEN + (size_t)len_intro * 2 + 1,
             "UPDATE user_pubinfo set introduction = '%s' where uid=%d",
             intro_f, BBS_priv.uid);

    free(intro_f);
    intro_f = NULL;

    if (mysql_query(db, sql_content) != 0)
    {
        log_error("Edit user intro error: %s\n", mysql_error(db));
        ret = -1;
        goto cleanup;
    }

    // Commit transaction
	if (mysql_query(db, "COMMIT") != 0)
	{
		log_error("Commit transaction error: %s\n", mysql_error(db));
		ret = -1;
		goto cleanup;
	}
   
    free(sql_content);
    sql_content = NULL;

    clearscr();
    moveto(1, 1);
    prints("说明档修改完成，新内容通常会在60秒后可见");
    press_any_key();
    ret = 1; // Success

cleanup:
    mysql_free_result(rs);
    mysql_close(db);

    // Cleanup buffers
    editor_data_cleanup(p_editor_data);

    free(sql_content);
    free(intro);
    free(intro_f);    

    return (int)ret;
}