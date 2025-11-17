/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * io
 *   - basic terminal-based user input / output features
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _IO_H_
#define _IO_H_

#include <iconv.h>
#include <stdio.h>

enum io_key_t
{
    CR = '\r',
    LF = '\n',
    BACKSPACE = '\b',
    BELL = '\b',
    KEY_TAB = 9,
    KEY_ESC = 27,
    KEY_SPACE = '\040',

    // Expand key
    KEY_NULL = 0xffff,
    KEY_TIMEOUT = 0xfffe,
    KEY_CONTROL = 0xff,
    KEY_UP = 0x0101,
    KEY_DOWN = 0x0102,
    KEY_RIGHT = 0x0103,
    KEY_LEFT = 0x0104,
    KEY_CSI = 0x011b, // ESC ESC
    KEY_HOME = 0x0201,
    KEY_INS = 0x0202,
    KEY_DEL = 0x0203,
    KEY_END = 0x0204,
    KEY_PGUP = 0x0205,
    KEY_PGDN = 0x0206,

    KEY_F1 = 0x0207,
    KEY_F2 = 0x0208,
    KEY_F3 = 0x0209,
    KEY_F4 = 0x020a,
    KEY_F5 = 0x020b,
    KEY_F6 = 0x020c,
    KEY_F7 = 0x020d,
    KEY_F8 = 0x020e,
    KEY_F9 = 0x020f,
    KEY_F10 = 0x0210,
    KEY_F11 = 0x0211,
    KEY_F12 = 0x0212,

    KEY_SHIFT_F1 = 0x0213,
    KEY_SHIFT_F2 = 0x0214,
    KEY_SHIFT_F3 = 0x0215,
    KEY_SHIFT_F4 = 0x0216,
    KEY_SHIFT_F5 = 0x0217,
    KEY_SHIFT_F6 = 0x0218,
    KEY_SHIFT_F7 = 0x0219,
    KEY_SHIFT_F8 = 0x021a,
    KEY_SHIFT_F9 = 0x021b,
    KEY_SHIFT_F10 = 0x021c,
    KEY_SHIFT_F11 = 0x021d,
    KEY_SHIFT_F12 = 0x021e,

    KEY_CTRL_F1 = 0x021f,
    KEY_CTRL_F2 = 0x0220,
    KEY_CTRL_F3 = 0x0221,
    KEY_CTRL_F4 = 0x0222,
    KEY_CTRL_F5 = 0x0223,
    KEY_CTRL_F6 = 0x0224,
    KEY_CTRL_F7 = 0x0225,
    KEY_CTRL_F8 = 0x0226,
    KEY_CTRL_F9 = 0x0227,
    KEY_CTRL_F10 = 0x0228,
    KEY_CTRL_F11 = 0x0229,
    KEY_CTRL_F12 = 0x022a,

    KEY_CTRL_UP = 0x0230,
    KEY_CTRL_DOWN = 0x0231,
    KEY_CTRL_RIGHT = 0x0232,
    KEY_CTRL_LEFT = 0x0233,
    KEY_CTRL_HOME = 0x0234,
    KEY_CTRL_END = 0x0235,
};

#define Ctrl(C) ((C) - 'A' + 1)

enum io_echo_t
{
    NOECHO = 0,
    DOECHO = 1,
};

enum io_iconv_t
{
    CHARSET_MAX_LEN = 20,
};

extern const char BBS_default_charset[CHARSET_MAX_LEN + 1];
extern char stdio_charset[CHARSET_MAX_LEN + 1];

extern int io_init(void);
extern void io_cleanup(void);

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
