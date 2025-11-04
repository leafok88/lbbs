/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * io
 *   - basic terminal-based user input / output features
 *
 * Copyright (C) 2004-2025 by Leaflet <leaflet@leafok.com>
 */

#ifndef _IO_H_
#define _IO_H_

#include <iconv.h>
#include <stdio.h>

#define CR '\r'
#define LF '\n'
#define BACKSPACE '\b'
#define BELL '\b'
#define KEY_TAB 9
#define KEY_ESC 27
#define KEY_SPACE '\040'

#ifndef EXTEND_KEY
#define EXTEND_KEY

#define KEY_NULL 0xffff
#define KEY_TIMEOUT 0xfffe
#define KEY_CONTROL 0xff
#define KEY_UP 0x0101
#define KEY_DOWN 0x0102
#define KEY_RIGHT 0x0103
#define KEY_LEFT 0x0104
#define KEY_CSI 0x011b // ESC ESC
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
#define KEY_F11 0x0211
#define KEY_F12 0x0212

#define KEY_SHIFT_F1 0x0213
#define KEY_SHIFT_F2 0x0214
#define KEY_SHIFT_F3 0x0215
#define KEY_SHIFT_F4 0x0216
#define KEY_SHIFT_F5 0x0217
#define KEY_SHIFT_F6 0x0218
#define KEY_SHIFT_F7 0x0219
#define KEY_SHIFT_F8 0x021a
#define KEY_SHIFT_F9 0x021b
#define KEY_SHIFT_F10 0x021c
#define KEY_SHIFT_F11 0x021d
#define KEY_SHIFT_F12 0x021e

#define KEY_CTRL_F1 0x021f
#define KEY_CTRL_F2 0x0220
#define KEY_CTRL_F3 0x0221
#define KEY_CTRL_F4 0x0222
#define KEY_CTRL_F5 0x0223
#define KEY_CTRL_F6 0x0224
#define KEY_CTRL_F7 0x0225
#define KEY_CTRL_F8 0x0226
#define KEY_CTRL_F9 0x0227
#define KEY_CTRL_F10 0x0228
#define KEY_CTRL_F11 0x0229
#define KEY_CTRL_F12 0x022a

#define KEY_CTRL_UP 0x0230
#define KEY_CTRL_DOWN 0x0231
#define KEY_CTRL_RIGHT 0x0232
#define KEY_CTRL_LEFT 0x0233
#define KEY_CTRL_HOME 0x0234
#define KEY_CTRL_END 0x0235

#endif // EXPAND_KEY

#define Ctrl(C) ((C) - 'A' + 1)

#define DOECHO (1)
#define NOECHO (0)

#define BBS_DEFAULT_CHARSET "UTF-8"

extern char stdio_charset[20];

extern int prints(const char *format, ...);
extern int outc(char c);
extern int iflush(void);

extern int igetch(int timeout);
extern int igetch_t(int sec);
extern void igetch_reset(void);

extern int io_buf_conv(iconv_t cd, char *p_buf, int *p_buf_len, int *p_buf_offset, char *p_conv, size_t conv_size, int *p_conv_len);
extern int io_conv_init(const char *charset);
extern int io_conv_cleanup(void);

#endif //_IO_H_
