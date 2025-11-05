/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * section_list_display
 *   - user interactive feature of section articles list
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _SECTION_LIST_DISPLAY_H_
#define _SECTION_LIST_DISPLAY_H_

#include "section_list.h"

extern int section_list_display(const char *sname, int32_t aid);

extern int section_list_ex_dir_display(SECTION_LIST *p_section);

extern int section_aid_locations_save(int uid);
extern int section_aid_locations_load(int uid);

#endif //_SECTION_LIST_DISPLAY_H_
