/* 电子鸡 小码力..几a几b游戏.□ */

/* Writed by Birdman From 140.116.102.125 创意哇哈哈*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bbs.h"
#include "bwf.h"
#include "common.h"
#include "io.h"
#include "log.h"
#include "login.h"
#include "money.h"
#include "screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>

enum _chicken_constant_t
{
	CHICKEN_NAME_LEN = 40,
};

static const char DATA_FILE[] = "var/chicken";
static const char LOG_FILE[] = "var/chicken/log";

static const char *cstate[10] =
	{"我在吃饭", "偷吃零食", "拉便便", "笨蛋..输给鸡?", "哈..赢小鸡也没多光荣", "没食物啦..", "疲劳全消!"};

char fname[FILE_PATH_LEN];
time_t birth;
int weight, satis, mon, day, age, angery, sick, oo, happy, clean, tiredstrong, play;
int winn, losee, last, chictime, agetmp, food, zfood;
char chicken_name[CHICKEN_NAME_LEN + 1];
FILE *cfp;
int gold, x[9] = {0}, ran, q_mon, p_mon;
unsigned long int bank;
char buf[1], buf1[6];

static int load_chicken(void);
static int save_chicken(void);
static int create_a_egg(void);
static int death(void);
static int guess(void);
static int lose(void);
static int pressany(int i);
static int sell(void);
static int show_chicken(void);
static int situ(void);
static int select_menu(void);
static int tie(void);
static int win_c(void);

int chicken_main()
{
	if (money_refresh() < 0)
	{
		return -1;
	}

	setuserfile(fname, sizeof(fname), DATA_FILE);

	if (load_chicken() < 0)
	{
		return -2;
	}

	if (user_online_update("CHICKEN") < 0)
	{
		log_error("user_online_update(CHICKEN) error\n");
	}

	show_chicken();
	select_menu();
	save_chicken();

	return 0;
}

static int load_chicken()
{
	FILE *fp;
	time_t now;
	struct tm ptime;
	int ret;

	agetmp = 1;
	//  modify_user_mode(CHICK);
	time(&now);
	localtime_r(&now, &ptime);

	if ((fp = fopen(fname, "r+")) == NULL)
	{
		if (create_a_egg() < 0)
		{
			return -1;
		}
		last = 1;
		fp = fopen(fname, "r");
	}
	else
	{
		last = 0;
	}
	ret = fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ",
				 &weight, &mon, &day, &satis, &age, &oo, &happy, &clean, &tiredstrong, &play, &winn, &losee, &food, &zfood, chicken_name);
	if (ret != 15)
	{
		log_error("Error in chicken data\n");
	}
	fclose(fp);

	if (day < (ptime.tm_mon + 1))
	{
		age = ptime.tm_mday;
		age = age + 31 - mon;
	}
	else
	{
		age = ptime.tm_mday - mon;
	}

	return 0;
}

int save_chicken()
{
	FILE *fp;

	fp = fopen(fname, "r+");
	fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ",
			weight, mon, day, satis, age, oo, happy, clean, tiredstrong, play, winn, losee, food, zfood, chicken_name);
	fclose(fp);

	return 0;
}

static int create_a_egg()
{
	FILE *fp;
	struct tm ptime;
	time_t now;
	time(&now);
	localtime_r(&now, &ptime);
	char name_tmp[CHICKEN_NAME_LEN + 1];
	int ret;

	snprintf(name_tmp, sizeof(name_tmp), "宝宝");

	clrtobot(2);

	for (chicken_name[0] = '\0'; !SYS_server_exit && chicken_name[0] == '\0';)
	{
		if (get_data(2, 1, "帮小鸡取个好名字: ", name_tmp, sizeof(name_tmp), CHICKEN_NAME_LEN / 2) > 0)
		{
			if ((ret = check_badwords(name_tmp, '*')) < 0)
			{
				log_error("check_badwords(name) error\n");
			}
			else if (ret > 0)
			{
				continue;
			}
			strncpy(chicken_name, name_tmp, sizeof(chicken_name) - 1);
			chicken_name[sizeof(chicken_name) - 1] = '\0';
		}
	}

	if ((fp = fopen(fname, "w")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", fname);
		return -2;
	}
	fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ",
			ptime.tm_hour * 2, ptime.tm_mday, ptime.tm_mon + 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 5, chicken_name);
	fclose(fp);

	if ((fp = fopen(LOG_FILE, "a")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", LOG_FILE);
		return -2;
	}
	fprintf(fp, "[32m%s[m 在 [34;43m[%d/%d  %d:%02d][m  养了一只叫 [33m%s[m 的小鸡\r\n",
			BBS_username, ptime.tm_mon + 1, ptime.tm_mday,
			ptime.tm_hour, ptime.tm_min, chicken_name);
	fclose(fp);

	return 0;
}

static int show_chicken()
{
	if (chictime >= 200)
	{
		weight -= 5;
		clean += 3;
		if (tiredstrong > 2)
		{
			tiredstrong -= 2;
		}
	}
	if (weight < 0)
	{
		death();
	}

	clrtobot(1);
	prints(
		"[33mName:%s[m"
		"  [45mAge :%d岁[m"
		"  重量:%d"
		"  食物:%d"
		"  零食:%d"
		"  疲劳:%d"
		"  便便:%d\r\n"
		"  生日:%d月%d日"
		"  糖糖:%8d"
		"  快乐度:%d"
		"  满意度:%d",
		// "  大补丸:%d\r\n",
		chicken_name, age, weight, food, zfood, tiredstrong, clean, day, mon, money_balance(), happy, satis); //,oo);

	moveto(3, 0);
	if (age <= 16)
	{
		switch (age)
		{
		case 0:
		case 1:
			prints("  ●●●●\r\n"
				   "●  ●●  ●\r\n"
				   "●●●●●●\r\n"
				   "●●    ●●\r\n"
				   "●●●●●●\r\n"
				   "  ●●●●   ");

			break;
		case 2:
		case 3:
			prints("    ●●●●●●\r\n"
				   "  ●            ●\r\n"
				   "●    ●    ●    ●\r\n"
				   "●                ●\r\n"
				   "●      ●●      ●\r\n"
				   "●                ●\r\n"
				   "●                ●\r\n"
				   "  ●            ●\r\n"
				   "    ●●●●●●   ");

			break;
		case 4:
		case 5:

			prints("      ●●●●●●\r\n"
				   "    ●            ●\r\n"
				   "  ●  ●        ●  ●\r\n"
				   "  ●                ●\r\n"
				   "  ●      ●●      ●\r\n"
				   "●●●    ●●      ●●\r\n"
				   "  ●                ●\r\n"
				   "  ●                ●\r\n"
				   "    ●  ●●●●  ●\r\n"
				   "      ●      ●  ●\r\n"
				   "                ●    ");
			break;
		case 6:
		case 7:
			prints("   ●    ●●●●●●\r\n"
				   "●  ●●  ●        ●\r\n"
				   "●              ●    ●\r\n"
				   "  ●●●                ●\r\n"
				   "●                      ●\r\n"
				   "●  ●●                ●\r\n"
				   "  ●  ●                ●\r\n"
				   "      ●                ●\r\n"
				   "        ●            ●\r\n"
				   "          ●●●●●●        ");
			break;

		case 8:
		case 9:
		case 10:
			prints("    ●●          ●●\r\n"
				   "  ●●●●      ●●●●\r\n"
				   "  ●●●●●●●●●●●\r\n"
				   "  ●                  ●\r\n"
				   "  ●    ●      ●    ●\r\n"
				   "●                      ●\r\n"
				   "●        ●●●        ●\r\n"
				   "  ●                  ●\r\n"
				   "●    ●          ●  ●\r\n"
				   "  ●●            ●●●\r\n"
				   "  ●                  ●\r\n"
				   "    ●              ●\r\n"
				   "      ●  ●●●  ●\r\n"
				   "      ●  ●    ●\r\n"
				   "        ●               ");

			break;

		case 11:
		case 12:
			prints("        ●●●●●●\r\n"
				   "      ●    ●    ●●\r\n"
				   "    ●  ●      ●  ●●\r\n"
				   "  ●●              ●●●\r\n"
				   "●              ●    ●●\r\n"
				   "●●●●●●●●      ●●\r\n"
				   "  ●                  ●●\r\n"
				   "    ●        ●  ●    ●\r\n"
				   "    ●        ●  ●    ●\r\n"
				   "    ●          ●      ●\r\n"
				   "      ●              ●\r\n"
				   "        ●  ●●●  ●\r\n"
				   "        ●  ●  ●  ●\r\n"
				   "          ●      ●             ");

			break;
		case 13:
		case 14:
			prints("              ●●●●\r\n"
				   "      ●●●●●●●●\r\n"
				   "    ●●●●●●●●●●\r\n"
				   "  ●●●●●●●●●●●●\r\n"
				   "  ●    ●●●●●●●●●\r\n"
				   "●●    ●            ●●\r\n"
				   "●●●●                ●\r\n"
				   "  ●                    ●\r\n"
				   "    ●●            ●●\r\n"
				   "  ●    ●●●●●●  ●\r\n"
				   "  ●                  ●\r\n"
				   "    ●                  ●\r\n"
				   "      ●                ●\r\n"
				   "    ●●●            ●●●        ");
			break;
		case 15:
		case 16:
			prints("  ●    ●●●●●●\r\n"
				   "●  ●●  ●        ●\r\n"
				   "●              ●    ●\r\n"
				   "  ●●●                ●\r\n"
				   "●                      ●\r\n"
				   "●  ●●                ●\r\n"
				   "  ●  ●                ●\r\n"
				   "      ●        ●  ●    ●\r\n"
				   "      ●          ●      ●\r\n"
				   "      ●                  ●\r\n"
				   "        ●              ●\r\n"
				   "        ●  ●●  ●●●\r\n"
				   "        ●  ●●  ●\r\n"
				   "          ●    ●             ");

			break;
		}
	}
	else
	{
		prints("          ●●●●●●●\r\n"
			   "        ●          ●●●\r\n"
			   "      ●    ●    ●  ●●●\r\n"
			   "  ●●●●●●●        ●●\r\n"
			   "  ●          ●          ●\r\n"
			   "  ●●●●●●●          ●            [1;5;31m我是大怪鸟[m\r\n"
			   "  ●        ●            ●\r\n"
			   "  ●●●●●●            ●\r\n"
			   "  ●                    ●\r\n"
			   "  ●                    ●\r\n"
			   "    ●                ●\r\n"
			   "●●  ●            ●\r\n"
			   "●      ●●●●●●  ●●\r\n"
			   "  ●                      ●\r\n"
			   "●●●    我是大怪鸟       ●●● ");
	}
	if (clean > 10)
	{
		moveto(10, 30);
		prints("便便好多..臭臭...");
		if (clean > 15)
			death();
		press_any_key();
	}

	moveto(17, 0);
	prints("[32m[1]-吃饭     [2]-吃零食   [3]-清理鸡舍   [4]-跟小鸡猜拳  [5]-目前战绩[m");
	prints("\r\n[32m[6]-买鸡饲料$20  [7]-买零食$30  [8]-吃大补丸  [9]-卖鸡喔 [m");
	return 0;
}

static int select_menu()
{
	int loop = 1;
	char inbuf[2];
	struct tm ptime;
	time_t now;
	time(&now);
	localtime_r(&now, &ptime);
	char name_tmp[CHICKEN_NAME_LEN + 1];
	int ret;

	while (!SYS_server_exit && loop)
	{
		moveto(23, 0);
		prints("[0;46;31m  使用帮助  [0;47;34m c 改名字   k 杀鸡   t 消除非疲劳($50)   q 退出     [m");
		inbuf[0] = '\0';
		if (get_data(22, 1, "要做些什么呢?: ", inbuf, sizeof(inbuf), 1) < 0)
		{
			return 0; // input timeout
		}
		if (tiredstrong > 20)
		{
			clearscr();
			moveto(15, 30);
			prints("呜~~~小鸡会累坏的...要先去休息一下..");
			prints("\r\n\r\n休    养     中");
		}
		switch (inbuf[0])
		{
		case '1':
			if (food <= 0)
			{
				pressany(5);
				break;
			}
			clrtobot(10);
			moveto(10, 0);
			prints("       □□□□□□\r\n"
				   "         ∵∴ □  □\r\n"
				   "              □  □                             □□□□  □          \r\n"
				   "              □  □     □              □      □□□□□□□            \r\n"
				   "         Ｃｏｋｅ □    _□□□□□□□□□_    □□□□□□□□                  \r\n"
				   "             □   □     ％％％％％％％％％       □—∩∩—□          \r\n"
				   "            □    □     □□□□□□□□□       │Mcdonald│      　　　 　\r\n"
				   "           □     □     ※※※※※※※※※　     □————□          \r\n"
				   "       □□□□□□      □□□□□□□□□     □□□□□□□□ ");

			pressany(0);
			iflush();
			food--;
			tiredstrong++;
			satis++;
			if (age < 5)
			{
				weight = weight + (5 - age);
			}
			else
			{
				weight++;
			}
			if (weight > 100)
			{
				moveto(9, 30);
				prints("太重了啦..肥鸡~~你想撑死鸡啊？....哇咧○●××");
				press_any_key();
			}
			if (weight > 150)
			{
				moveto(9, 30);
				prints("鸡撑晕了~~");
				press_any_key();
			}
			if (weight > 200)
			{
				death();
			}
			break;
		case '2':
			if (zfood <= 0)
			{
				pressany(5);
				break;
			}
			clrtobot(10);
			moveto(10, 0);
			prints("             □\r\n"
				   "       [33;1m□[m□○\r\n"
				   "       [37;42m■■[m\r\n"
				   "       [32m□□[m\r\n"
				   "       [32;40;1m□□[m\r\n"
				   "       [31m □ [m\r\n"
				   "      [31m □□[m   [32;1m水果酒冰淇淋苏打[m   嗯!好喝!   ");
			pressany(1);
			zfood--;
			tiredstrong++;
			happy++;
			weight += 2;
			if (weight > 100)
			{
				moveto(9, 30);
				prints("太重了啦..肥鸡~~");
				press_any_key();
			}
			if (weight > 200)
			{
				death();
			}
			break;
		case '3':
			clrtobot(10);
			moveto(10, 0);
			prints("[1;36m                              □□□□□[m\r\n"
				   "[1;33m                             『[37m□□□□[m\r\n"
				   "[1;37m                               □□□□[m\r\n"
				   "[1;37m             □□□□□□□□[32m◎[37m□□□□[m\r\n"
				   "[37m             □□□□□□□□□[1;37m□□□□[m\r\n"
				   "[37m             □□□□□□□□□[1;33m □[m\r\n"
				   "[36m                  □□□□□□[1;33m□□[m\r\n"
				   "[1;36m                  □□□□□□[m\r\n"
				   "  [1;37m                □□□□□□[m\r\n"
				   "                  耶耶耶...便便拉光光...                              ");

			pressany(2);
			tiredstrong += 5;
			clean = 0;
			break;
		case '4':
			guess();
			satis += (ptime.tm_sec % 2);
			tiredstrong++;
			break;
		case '5':
			situ();
			break;
		case '6':
			clrtobot(20);
			moveto(20, 0);
			if (money_withdraw(20) <= 0)
			{
				prints("糖果不足!!");
				press_any_key();
				break;
			}
			food += 5;
			prints("\r\n食物有 [33;41m %d [m份", food);
			prints("   剩下 [33;41m %d [m糖", money_balance());
			press_any_key();
			break;
		case '7':
			clrtobot(20);
			moveto(20, 0);
			if (money_withdraw(30) <= 0)
			{
				prints("糖果不足!!");
				press_any_key();
				break;
			}
			zfood += 5;
			prints("\r\n零食有 [33;41m %d [m份", zfood);
			prints("  剩下 [33;41m %d [m糖", money_balance());
			press_any_key();
			break;
		case '8':
			if (oo > 0)
			{
				clrtobot(10);
				moveto(10, 0);
				prints("\r\n"
					   "               □□□□\r\n"
					   "               □□□□\r\n"
					   "               □□□□\r\n"
					   "                          偷吃禁药大补丸.....");
				tiredstrong = 0;
				happy += 3;
				oo--;
				pressany(6);
				break;
			}
			clrtobot(20);
			moveto(20, 4);
			prints("没大补丸啦!!");
			press_any_key();
			break;
		case '9':
			if (age < 5)
			{
				clrtobot(20);
				moveto(20, 4);
				prints("太小了...不得贩售未成年小鸡.....");
				press_any_key();
				break;
			}
			sell();
			break;
		case 'k':
			death();
			break;
		case 't':
			if (money_withdraw(50) <= 0)
			{
				clrtobot(20);
				moveto(20, 4);
				prints("糖果不足!!");
				press_any_key();
				break;
			}
			else
			{
				tiredstrong = 0;
			}
			break;
		case 'c':
			strncpy(name_tmp, chicken_name, sizeof(name_tmp) - 1);
			name_tmp[sizeof(name_tmp) - 1] = '\0';

			clrline(22, 22);

			if (get_data(22, 1, "帮小鸡取个好名字: ", name_tmp, sizeof(name_tmp), CHICKEN_NAME_LEN / 2) > 0)
			{
				if ((ret = check_badwords(name_tmp, '*')) < 0)
				{
					log_error("check_badwords(name) error\n");
				}
				else if (ret == 0)
				{
					strncpy(chicken_name, name_tmp, sizeof(chicken_name) - 1);
					chicken_name[sizeof(chicken_name) - 1] = '\0';
				}
			}
			break;
		case 'q':
			loop = 0;
			break;
		}

		if (loop)
		{
			show_chicken();
		}
	}
	return 0;
}

int death()
{
	FILE *fp;
	struct tm ptime;
	time_t now;

	time(&now);
	localtime_r(&now, &ptime);
	clearscr();
	clrtobot(5);
	if ((fp = fopen(LOG_FILE, "a")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", LOG_FILE);
		return -1;
	}
	fprintf(fp, "[32m%s[m 在 [34;43m[%d/%d  %d:%02d][m  的小鸡 [33m%s  [36m挂了~~[m \r\n",
			BBS_username, ptime.tm_mon + 1, ptime.tm_mday,
			ptime.tm_hour, ptime.tm_min, chicken_name);
	fclose(fp);
	prints("呜...小鸡挂了....");
	prints("\r\n笨史了...赶出系统...");
	press_any_key();

	unlink(fname);
	load_chicken();

	return 0;
}

int pressany(int i)
{
	int ch;
	moveto(23, 0);
	prints("[33;46;1m                           [34m%s[37m                         [0m", cstate[i]);
	iflush();
	ch = igetch_t(MIN(BBS_max_user_idle_time, 60));
	moveto(23, 0);
	clrtoeol();
	iflush();
	return ch;
}

int guess()
{
	int ch, com;

	moveto(23, 0);
	prints("[1]-剪刀 [2]-石头 [3]-布: ");
	clrtoeol();
	iflush();

	ch = igetch_t(MIN(BBS_max_user_idle_time, 60));
	if ((ch != '1') && (ch != '2') && (ch != '3'))
	{
		return -1; // error input
	}

	srand((unsigned int)time(NULL));
	com = rand() % 3;

	moveto(21, 35);
	switch (com)
	{
	case 0:
		prints("小鸡:剪刀");
		break;
	case 1:
		prints("小鸡:石头");
		break;
	case 2:
		prints("小鸡:布");
		break;
	}
	clrtoeol();

	moveto(19, 0);

	switch (ch)
	{
	case '1':
		prints("笨鸡---看我捡来的刀---");
		if (com == 0)
			tie();
		else if (com == 1)
			lose();
		else if (com == 2)
			win_c();
		break;
	case '2':
		prints("呆鸡---砸你一块石头!!---");
		if (com == 0)
			win_c();
		else if (com == 1)
			tie();
		else if (com == 2)
			lose();
		break;
	case '3':
		prints("蠢鸡---送你一堆破布!---");
		if (com == 0)
			lose();
		else if (com == 1)
			win_c();
		else if (com == 2)
			tie();
		break;
	}
	clrtoeol();

	press_any_key();
	return 0;
}

int win_c()
{
	winn++;
	clrtobot(20);
	moveto(20, 0);
	prints("判定:小鸡输了....    >_<~~~~~\r\n"
		   "\r\n"
		   "                                 ");
	return 0;
}
int tie()
{
	clrtobot(20);
	moveto(20, 0);
	prints("判定:平手                    -_-\r\n"
		   "\r\n"
		   "                                              ");
	return 0;
}
int lose()
{
	losee++;
	happy += 2;
	clrtobot(20);
	moveto(20, 0);
	prints("小鸡赢罗                      ∩∩\r\n"
		   "                               □       ");
	return 0;
}

int situ()
{
	clrtobot(16);
	moveto(16, 0);
	prints("           ");
	moveto(17, 0);
	prints("你:[44m %d胜 %d负[m                   ", winn, losee);
	moveto(18, 0);
	prints("鸡:[44m %d胜 %d负[m                   ", losee, winn);

	if (winn >= losee)
		pressany(4);
	else
		pressany(3);

	return 0;
}

int sell()
{
	int sel = 0;
	char ans[2];
	struct tm ptime;
	FILE *fp;
	time_t now;

	time(&now);
	localtime_r(&now, &ptime);

	ans[0] = '\0';

	sel += (happy * 10);
	sel += (satis * 7);
	sel += ((ptime.tm_sec % 9) * 10);
	sel += weight;
	sel += age * 10;

	clrtobot(20);
	moveto(20, 0);
	prints("小鸡值[33;45m$$ %d [m糖糖", sel);
	if (get_data(19, 1, "真的要卖掉小鸡?[y/N]", ans, sizeof(ans), 1) < 0)
	{
		return -1; // input timeout
	}
	if (ans[0] != 'y')
	{
		return -1;
	}

	if (money_deposit(sel) <= 0)
	{
		log_error("Cannot deposit money %d\n", sel);
		moveto(21, 0);
		prints("无法存钱，放弃交易！");
		return -2;
	}

	if ((fp = fopen(LOG_FILE, "a")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", LOG_FILE);
		return -2;
	}
	fprintf(fp, "[32m%s[m 在 [34;43m[%d/%d  %d:%02d][m  把小鸡 [33m%s  [31m以 [37;44m%d[m [31m糖果卖了[m\r\n",
			BBS_username, ptime.tm_mon + 1, ptime.tm_mday,
			ptime.tm_hour, ptime.tm_min, chicken_name, sel);
	fclose(fp);
	clearscr();

	unlink(fname);
	load_chicken();

	return 0;
}
