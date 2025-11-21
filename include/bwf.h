/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bwf
 *   - bad words filter
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _BWF_H_
#define _BWF_H_

extern int bwf_load(const char *filename);
extern int bwf_compile(void);
extern void bwf_cleanup(void);

extern int check_badwords(char *str, char c_replace);

#endif //_BWF_H_
