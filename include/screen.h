/***************************************************************************
                            screen.h  -  description
                             -------------------
    begin                : Mon Oct 18 2004
    copyright            : (C) 2004 by Leaflet
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

#ifndef _SCREEN_H_
#define _SCREEN_H_

extern int screen_rows;
extern int screen_cols;

extern void moveto(int row, int col);

extern void clrtoeol();

extern void clrline(int line_begin, int line_end);

extern void clrtobot(int line_begin);

extern void clearscr();

extern int press_any_key();

extern void set_input_echo(int echo);

extern int str_input(char *buffer, int buffer_length, int echo_mode);

extern int get_data(int row, int col, char *prompt, char *buffer, int buffer_length, int echo_mode);

extern int display_file(const char *filename);

extern int display_file_ex(const char *filename, int begin_line, int wait);

extern int show_top(char *status);

extern int show_bottom(char *msg);

extern int show_active_board();

#endif //_SCREEN_H_
