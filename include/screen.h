/***************************************************************************
                            screen.h  -  description
                             -------------------
    copyright            : (C) 2004-2025 by Leaflet
    email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _SCREEN_H_
#define _SCREEN_H_

#include <stddef.h>

#define CTRL_SEQ_CLR_LINE "\033[K"
#define MSG_EXT_MAX_LEN 400

struct display_ctx_t
{
   int reach_begin;
   int reach_end;
   long line_top;
   long line_bottom;
   char msg[MSG_EXT_MAX_LEN];
};
typedef struct display_ctx_t DISPLAY_CTX;

typedef int (*display_data_key_handler)(int *p_key, DISPLAY_CTX *p_ctx);

extern void moveto(int row, int col);
extern void clrtoeol();
extern void clrline(int line_begin, int line_end);
extern void clrtobot(int line_begin);
extern void clearscr();

extern int press_any_key();

extern void set_input_echo(int echo);

extern int str_input(char *buffer, int buffer_length, int echo_mode);
extern int get_data(int row, int col, char *prompt, char *buffer, int buffer_length, int max_display_len, int echo_mode);

// eof_exit = 0 : Do not exit at EOF
//            1 : Prompt for any key at EOF, then exit
//            2 : Exit at EOF without prompt
extern int display_data(const void *p_data, long display_line_total, const long *p_line_offsets, int eof_exit,
                        display_data_key_handler key_handler, const char *help_filename);
extern int display_file(const char *filename, int eof_exit);

extern int show_top(const char *str_left, const char *str_center, const char *str_right);
extern int show_bottom(const char *msg);

extern int show_active_board();

#endif //_SCREEN_H_
