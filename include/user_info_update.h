/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * user_info_update
 *   - update user information
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 *
 * Author:                  ytht <2391669999@qq.com>
 */

#ifndef _USER_INFO_UPDATE_
#define _USER_INFO_UPDATE_

enum bbs_userinfo_const_t
{
    BBS_user_intro_line_len = 256,
    BBS_user_sign_max_len = 4096,
    BBS_user_sign_max_line = 10, 
};

extern int user_intro_edit(int);
extern int user_sign_edit(int);

#endif //_USER_INFO_UPDATE_
