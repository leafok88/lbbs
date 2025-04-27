#include "bbs.h"

int inmoney(unsigned int money)
{
	if (BBS_user_money + money < 400000000)
		BBS_user_money += money;
	else
		BBS_user_money = 400000000;
	return BBS_user_money;
}

int demoney(unsigned int money)
{
	if (BBS_user_money > money)
		BBS_user_money -= money;
	else
		BBS_user_money = 0;
	return BBS_user_money;
}
