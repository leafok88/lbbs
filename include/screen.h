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

#define CTRL_SEQ_CLR_LINE "\033[K"

extern void moveto(int row, int col);

extern void clrtoeol();

extern void clrline(int line_begin, int line_end);

extern void clrtobot(int line_begin);

extern void clearscr();

extern int press_any_key();

extern void set_input_echo(int echo);

extern int str_input(char *buffer, int buffer_length, int echo_mode);

extern int get_data(int row, int col, char *prompt, char *buffer, int buffer_length, int echo_mode);

extern int display_file_ex(const char *filename, int begin_line, int wait);

extern int show_top(const char *str_left, const char *str_center);

extern int show_bottom(const char *msg);

extern int show_active_board();

#endif //_SCREEN_H_
