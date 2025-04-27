/***************************************************************************
                          menu_proc.c  -  description
                             -------------------
    begin                : Fri May 6 2005
    copyright            : (C) 2005 by Leaflet
    email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "bbs.h"
#include "bbs_cmd.h"
#include "common.h"
#include "io.h"
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int
exec_mbem(const char *str)
{
    void *hdll;
    int (*func) ();
    char *c,*s;
    char buf[1024];

    strcpy(buf, str);
    s = strstr(buf, "@mod:");
    if (s) {
        c = strstr(s + 5, "#");
        if (c) {
            *c = 0;
            c++;
        }

        hdll = dlopen (s + 5, RTLD_LAZY);

        if (hdll) {
            char *error;

            if ((func = dlsym (hdll, c ? c : "mod_main")) != NULL)
                func ();
            else if ((error = dlerror ()) != NULL) {
                clearscr ();
                prints ("%s\r\n", error);
                press_any_key ();
            }
            dlclose (hdll);
        } else {
            clearscr ();
            prints ("加载库文件 [%s] 失败!!\r\n", s + 5);
            prints ("失败原因:%s\r\n", dlerror());
            press_any_key ();
        }
    }

 	return REDRAW;
}

int
exitbbs (const char *s)
{
 	return EXITBBS;
}

int
license (const char *s)
{
	char temp[256];
	
    strcpy (temp, app_home_dir);
    strcat (temp, "data/license.txt");
    display_file_ex (temp, 0, 1);

 	return REDRAW;
}

int
copyright (const char *s)
{
    char temp[256];
	
    strcpy (temp, app_home_dir);
    strcat (temp, "data/copyright.txt");
    display_file_ex (temp, 0, 1);

    return REDRAW;
}

int
reloadbbsmenu (const char *s)
{
    if (kill (getppid (), SIG_RELOAD_MENU) < 0)
      log_error ("Send SIG_RELOAD_MENU signal failed (%d)\n", errno);

    return REDRAW;
}

int
shutdownbbs (const char *s)
{
    if (kill (0, SIGTERM) < 0)
      log_error ("Send SIGTERM signal failed (%d)\n", errno);

    return REDRAW;
}
