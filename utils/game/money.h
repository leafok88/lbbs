/***************************************************************************
					       money.h  -  description
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

#ifndef _MONEY_H_
#define _MONEY_H_

#define USER_MONEY_MAX 1000000000 // 1 billion

extern int BBS_user_money;

extern int money_balance();
extern int money_deposit(int money);
extern int money_withdraw(int money);
extern int money_refresh(void);

#endif
