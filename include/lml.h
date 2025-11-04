/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * lml
 *   - LML render
 *
 * Copyright (C) 2004-2025 by Leaflet <leaflet@leafok.com>
 */

#ifndef _LML_H_
#define _LML_H_

#include <stddef.h>
#include <time.h>

extern clock_t lml_total_exec_duration; // For testing purpose

extern int lml_render(const char *str_in, char *str_out, int buf_len, int width, int quote_mode);

#endif //_LML_H_
