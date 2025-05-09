/***************************************************************************
							io.h  -  description
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

#ifndef _IO_H_
#define _IO_H_

#include <stdio.h>

#define ESC_KEY '\033'
#define CR '\r'
#define LF '\n'
#define BACKSPACE '\b'
#define BELL '\b'
#define KEY_SPACE '\040'

#ifndef EXTEND_KEY
#define EXTEND_KEY
#define KEY_NULL 0xffff
#define KEY_TIMEOUT 0xfffe
#define KEY_CONTROL 0xff
#define KEY_TAB 9
#define KEY_ESC 27
#define KEY_UP 0x0101
#define KEY_DOWN 0x0102
#define KEY_RIGHT 0x0103
#define KEY_LEFT 0x0104
#define KEY_HOME 0x0201
#define KEY_INS 0x0202
#define KEY_DEL 0x0203
#define KEY_END 0x0204
#define KEY_PGUP 0x0205
#define KEY_PGDN 0x0206
#define KEY_F1 0x0207
#define KEY_F2 0x0208
#define KEY_F3 0x0209
#define KEY_F4 0x020a
#define KEY_F5 0x020b
#define KEY_F6 0x020c
#define KEY_F7 0x020d
#define KEY_F8 0x020e
#define KEY_F9 0x020f
#define KEY_F10 0x0210
#endif

#define Ctrl(C) ((C) - 'A' + 1)

#define DOECHO (1)
#define NOECHO (0)

extern int outc(char c);

extern int prints(const char *format, ...);

extern int iflush();

extern int igetch(int clear_buf);

extern int igetch_t(long int sec);

#endif //_IO_H_
