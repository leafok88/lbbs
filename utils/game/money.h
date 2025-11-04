/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * money
 *   - basic operations of user money
 *
 * Copyright (C) 2004-2025  Leaflet <leaflet@leafok.com>
 */

#ifndef _MONEY_H_
#define _MONEY_H_

#define USER_MONEY_MAX 1000000000 // 1 billion

extern int BBS_user_money;

extern int money_balance();
extern int money_deposit(int money);
extern int money_withdraw(int money);
extern int money_refresh(void);

#endif
