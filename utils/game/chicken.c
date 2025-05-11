/* µç×Ó¼¦ Ğ¡ÂëÁ¦..¼¸a¼¸bÓÎÏ·.¡õ */

/* Writed by Birdman From 140.116.102.125 ´´ÒâÍÛ¹ş¹ş*/

#include "bbs.h"
#include "common.h"
#include "io.h"
#include "log.h"
#include "screen.h"
#include "money.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>

#define DATA_FILE "var/chicken"
#define LOG_FILE "var/chicken/log"
#define CHICKEN_NAME_LEN 20

char
	*cstate[10] = {"ÎÒÔÚ³Ô·¹", "Íµ³ÔÁãÊ³", "À­±ã±ã", "±¿µ°..Êä¸ø¼¦?", "¹ş..Ó®Ğ¡¼¦Ò²Ã»¶à¹âÈÙ", "Ã»Ê³ÎïÀ²..", "Æ£ÀÍÈ«Ïû!"};
char *cage[9] = {"µ®Éú", "ÖÜËê", "Ó×Äê", "ÉÙÄê", "ÇàÄê", "»îÁ¦", "×³Äê", "ÖĞÄê"};
char *menu[8] = {"ÓÎÏ·", "ÔË¶¯", "µ÷½Ì¼ÆÄÜ", "ÂòÂô¹¤¾ß", "ÇåÀí¼¦Éá"};

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

	show_chicken();
	select_menu();
	save_chicken();

	return 0;
}

static int load_chicken()
{
	FILE *fp;
	time_t now;
	struct tm *ptime;

	agetmp = 1;
	//  modify_user_mode(CHICK);
	time(&now);
	ptime = localtime(&now);

	if ((fp = fopen(fname, "r+")) == NULL)
	{
		chicken_name[0] = '\0';
		if (create_a_egg() < 0)
		{
			return -1;
		}
		last = 1;
		fp = fopen(fname, "r");
		fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ", &weight, &mon, &day, &satis, &age, &oo, &happy, &clean, &tiredstrong, &play, &winn, &losee, &food, &zfood, chicken_name);
		fclose(fp);
	}
	else
	{
		last = 0;
		fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ", &weight, &mon, &day, &satis, &age, &oo, &happy, &clean, &tiredstrong, &play, &winn, &losee, &food, &zfood, chicken_name);
		fclose(fp);
	}

	if (day < (ptime->tm_mon + 1))
	{
		age = ptime->tm_mday;
		age = age + 31 - mon;
	}
	else
	{
		age = ptime->tm_mday - mon;
	}

	return 0;
}

int save_chicken()
{
	FILE *fp;

	fp = fopen(fname, "r+");
	fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ", weight, mon, day, satis, age, oo, happy, clean, tiredstrong, play, winn, losee, food, zfood, chicken_name);
	fclose(fp);

	return 0;
}

static int create_a_egg()
{
	FILE *fp;
	struct tm *ptime;
	time_t now;
	time(&now);
	ptime = localtime(&now);
	char name_tmp[CHICKEN_NAME_LEN + 1];

	clrtobot(2);
	while (!SYS_server_exit && chicken_name[0] == '\0')
	{
		strncpy(chicken_name, "±¦±¦", sizeof(chicken_name) - 1);
		chicken_name[sizeof(chicken_name) - 1] = '\0';

		strncpy(name_tmp, chicken_name, sizeof(name_tmp) - 1);
		name_tmp[sizeof(name_tmp) - 1] = '\0';

		if (get_data(2, 0, "°ïĞ¡¼¦È¡¸öºÃÃû×Ö£º", name_tmp, sizeof(name_tmp), DOECHO) > 0)
		{
			strncpy(chicken_name, name_tmp, sizeof(chicken_name) - 1);
			chicken_name[sizeof(chicken_name) - 1] = '\0';
		}
	}

	if ((fp = fopen(fname, "w")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", fname);
		return -2;
	}
	fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ", ptime->tm_hour * 2, ptime->tm_mday, ptime->tm_mon + 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 5, chicken_name);
	fclose(fp);

	if ((fp = fopen(LOG_FILE, "a")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", LOG_FILE);
		return -2;
	}
	fprintf(fp, "[32m%s[m ÔÚ [34;43m[%d/%d  %d:%02d][m  ÑøÁËÒ»Ö»½Ğ [33m%s[m µÄĞ¡¼¦\r\n",
			BBS_username, ptime->tm_mon + 1, ptime->tm_mday,
			ptime->tm_hour, ptime->tm_min, chicken_name);
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
		"  [45mAge :%dËê[m"
		"  ÖØÁ¿:%d"
		"  Ê³Îï:%d"
		"  ÁãÊ³:%d"
		"  Æ£ÀÍ:%d"
		"  ±ã±ã:%d\r\n"
		"  ÉúÈÕ:%dÔÂ%dÈÕ"
		"  ÌÇÌÇ:%8d"
		"  ¿ìÀÖ¶È:%d"
		"  ÂúÒâ¶È:%d",
		// "  ´ó²¹Íè:%d\r\n",
		chicken_name, age, weight, food, zfood, tiredstrong, clean, day, mon, money_balance(), happy, satis); //,oo);

	moveto(3, 0);
	if (age <= 16)
	{
		switch (age)
		{
		case 0:
		case 1:
			prints("  ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "¡ñ  ¡ñ¡ñ  ¡ñ\r\n"
				   "¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "¡ñ¡ñ    ¡ñ¡ñ\r\n"
				   "¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "  ¡ñ¡ñ¡ñ¡ñ   ");

			break;
		case 2:
		case 3:
			prints("    ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "  ¡ñ            ¡ñ\r\n"
				   "¡ñ    ¡ñ    ¡ñ    ¡ñ\r\n"
				   "¡ñ                ¡ñ\r\n"
				   "¡ñ      ¡ñ¡ñ      ¡ñ\r\n"
				   "¡ñ                ¡ñ\r\n"
				   "¡ñ                ¡ñ\r\n"
				   "  ¡ñ            ¡ñ\r\n"
				   "    ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ   ");

			break;
		case 4:
		case 5:

			prints("      ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "    ¡ñ            ¡ñ\r\n"
				   "  ¡ñ  ¡ñ        ¡ñ  ¡ñ\r\n"
				   "  ¡ñ                ¡ñ\r\n"
				   "  ¡ñ      ¡ñ¡ñ      ¡ñ\r\n"
				   "¡ñ¡ñ¡ñ    ¡ñ¡ñ      ¡ñ¡ñ\r\n"
				   "  ¡ñ                ¡ñ\r\n"
				   "  ¡ñ                ¡ñ\r\n"
				   "    ¡ñ  ¡ñ¡ñ¡ñ¡ñ  ¡ñ\r\n"
				   "      ¡ñ      ¡ñ  ¡ñ\r\n"
				   "                ¡ñ    ");
			break;
		case 6:
		case 7:
			prints("   ¡ñ    ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "¡ñ  ¡ñ¡ñ  ¡ñ        ¡ñ\r\n"
				   "¡ñ              ¡ñ    ¡ñ\r\n"
				   "  ¡ñ¡ñ¡ñ                ¡ñ\r\n"
				   "¡ñ                      ¡ñ\r\n"
				   "¡ñ  ¡ñ¡ñ                ¡ñ\r\n"
				   "  ¡ñ  ¡ñ                ¡ñ\r\n"
				   "      ¡ñ                ¡ñ\r\n"
				   "        ¡ñ            ¡ñ\r\n"
				   "          ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ        ");
			break;

		case 8:
		case 9:
		case 10:
			prints("    ¡ñ¡ñ          ¡ñ¡ñ\r\n"
				   "  ¡ñ¡ñ¡ñ¡ñ      ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "  ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "  ¡ñ                  ¡ñ\r\n"
				   "  ¡ñ    ¡ñ      ¡ñ    ¡ñ\r\n"
				   "¡ñ                      ¡ñ\r\n"
				   "¡ñ        ¡ñ¡ñ¡ñ        ¡ñ\r\n"
				   "  ¡ñ                  ¡ñ\r\n"
				   "¡ñ    ¡ñ          ¡ñ  ¡ñ\r\n"
				   "  ¡ñ¡ñ            ¡ñ¡ñ¡ñ\r\n"
				   "  ¡ñ                  ¡ñ\r\n"
				   "    ¡ñ              ¡ñ\r\n"
				   "      ¡ñ  ¡ñ¡ñ¡ñ  ¡ñ\r\n"
				   "      ¡ñ  ¡ñ    ¡ñ\r\n"
				   "        ¡ñ               ");

			break;

		case 11:
		case 12:
			prints("        ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "      ¡ñ    ¡ñ    ¡ñ¡ñ\r\n"
				   "    ¡ñ  ¡ñ      ¡ñ  ¡ñ¡ñ\r\n"
				   "  ¡ñ¡ñ              ¡ñ¡ñ¡ñ\r\n"
				   "¡ñ              ¡ñ    ¡ñ¡ñ\r\n"
				   "¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ      ¡ñ¡ñ\r\n"
				   "  ¡ñ                  ¡ñ¡ñ\r\n"
				   "    ¡ñ        ¡ñ  ¡ñ    ¡ñ\r\n"
				   "    ¡ñ        ¡ñ  ¡ñ    ¡ñ\r\n"
				   "    ¡ñ          ¡ñ      ¡ñ\r\n"
				   "      ¡ñ              ¡ñ\r\n"
				   "        ¡ñ  ¡ñ¡ñ¡ñ  ¡ñ\r\n"
				   "        ¡ñ  ¡ñ  ¡ñ  ¡ñ\r\n"
				   "          ¡ñ      ¡ñ             ");

			break;
		case 13:
		case 14:
			prints("              ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "      ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "    ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "  ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "  ¡ñ    ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "¡ñ¡ñ    ¡ñ            ¡ñ¡ñ\r\n"
				   "¡ñ¡ñ¡ñ¡ñ                ¡ñ\r\n"
				   "  ¡ñ                    ¡ñ\r\n"
				   "    ¡ñ¡ñ            ¡ñ¡ñ\r\n"
				   "  ¡ñ    ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ  ¡ñ\r\n"
				   "  ¡ñ                  ¡ñ\r\n"
				   "    ¡ñ                  ¡ñ\r\n"
				   "      ¡ñ                ¡ñ\r\n"
				   "    ¡ñ¡ñ¡ñ            ¡ñ¡ñ¡ñ        ");
			break;
		case 15:
		case 16:
			prints("  ¡ñ    ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
				   "¡ñ  ¡ñ¡ñ  ¡ñ        ¡ñ\r\n"
				   "¡ñ              ¡ñ    ¡ñ\r\n"
				   "  ¡ñ¡ñ¡ñ                ¡ñ\r\n"
				   "¡ñ                      ¡ñ\r\n"
				   "¡ñ  ¡ñ¡ñ                ¡ñ\r\n"
				   "  ¡ñ  ¡ñ                ¡ñ\r\n"
				   "      ¡ñ        ¡ñ  ¡ñ    ¡ñ\r\n"
				   "      ¡ñ          ¡ñ      ¡ñ\r\n"
				   "      ¡ñ                  ¡ñ\r\n"
				   "        ¡ñ              ¡ñ\r\n"
				   "        ¡ñ  ¡ñ¡ñ  ¡ñ¡ñ¡ñ\r\n"
				   "        ¡ñ  ¡ñ¡ñ  ¡ñ\r\n"
				   "          ¡ñ    ¡ñ             ");

			break;
		}
	}
	else
	{
		prints("          ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ\r\n"
			   "        ¡ñ          ¡ñ¡ñ¡ñ\r\n"
			   "      ¡ñ    ¡ñ    ¡ñ  ¡ñ¡ñ¡ñ\r\n"
			   "  ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ        ¡ñ¡ñ\r\n"
			   "  ¡ñ          ¡ñ          ¡ñ\r\n"
			   "  ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ          ¡ñ            [1;5;31mÎÒÊÇ´ó¹ÖÄñ[m\r\n"
			   "  ¡ñ        ¡ñ            ¡ñ\r\n"
			   "  ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ            ¡ñ\r\n"
			   "  ¡ñ                    ¡ñ\r\n"
			   "  ¡ñ                    ¡ñ\r\n"
			   "    ¡ñ                ¡ñ\r\n"
			   "¡ñ¡ñ  ¡ñ            ¡ñ\r\n"
			   "¡ñ      ¡ñ¡ñ¡ñ¡ñ¡ñ¡ñ  ¡ñ¡ñ\r\n"
			   "  ¡ñ                      ¡ñ\r\n"
			   "¡ñ¡ñ¡ñ    ÎÒÊÇ´ó¹ÖÄñ       ¡ñ¡ñ¡ñ ");
	}
	if (clean > 10)
	{
		moveto(10, 30);
		prints("±ã±ãºÃ¶à..³ô³ô...");
		if (clean > 15)
			death();
		press_any_key();
	}

	moveto(17, 0);
	prints("[32m[1]-³Ô·¹     [2]-³ÔÁãÊ³   [3]-ÇåÀí¼¦Éá   [4]-¸úĞ¡¼¦²ÂÈ­  [5]-Ä¿Ç°Õ½¼¨[m");
	prints("\r\n[32m[6]-Âò¼¦ËÇÁÏ$20  [7]-ÂòÁãÊ³$30  [8]-³Ô´ó²¹Íè  [9]-Âô¼¦à¸ [m");
	return 0;
}

static int select_menu()
{
	int loop = 1;
	char inbuf[2];
	struct tm *ptime;
	time_t now;
	time(&now);
	ptime = localtime(&now);
	char name_tmp[CHICKEN_NAME_LEN + 1];

	while (!SYS_server_exit && loop)
	{
		moveto(23, 0);
		prints("[0;46;31m  Ê¹ÓÃ°ïÖú  [0;47;34m c ¸ÄÃû×Ö   k É±¼¦   t Ïû³ı·ÇÆ£ÀÍ($50)   q ÍË³ö     [m");
		inbuf[0] = '\0';
		if (get_data(22, 0, "Òª×öĞ©Ê²Ã´ÄØ?£º", inbuf, sizeof(inbuf), DOECHO) < 0)
		{
			return 0; // input timeout
		}
		if (tiredstrong > 20)
		{
			clearscr();
			moveto(15, 30);
			prints("ÎØ~~~Ğ¡¼¦»áÀÛ»µµÄ...ÒªÏÈÈ¥ĞİÏ¢Ò»ÏÂ..");
			prints("\r\n\r\nĞİ    Ñø     ÖĞ");
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
			prints("       ¡õ¡õ¡õ¡õ¡õ¡õ\r\n"
				   "         ¡ß¡à ¡õ  ¡õ\r\n"
				   "              ¡õ  ¡õ                             ¡õ¡õ¡õ¡õ  ¡õ          \r\n"
				   "              ¡õ  ¡õ     ¡õ              ¡õ      ¡õ¡õ¡õ¡õ¡õ¡õ¡õ            \r\n"
				   "         £Ã£ï£ë£å ¡õ    _¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ_    ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ                  \r\n"
				   "             ¡õ   ¡õ     £¥£¥£¥£¥£¥£¥£¥£¥£¥       ¡õ¡ª¡É¡É¡ª¡õ          \r\n"
				   "            ¡õ    ¡õ     ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ       ©¦Mcdonald©¦      ¡¡¡¡¡¡ ¡¡\r\n"
				   "           ¡õ     ¡õ     ¡ù¡ù¡ù¡ù¡ù¡ù¡ù¡ù¡ù¡¡     ¡õ¡ª¡ª¡ª¡ª¡õ          \r\n"
				   "       ¡õ¡õ¡õ¡õ¡õ¡õ      ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ     ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ ");

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
				prints("Ì«ÖØÁËÀ²..·Ê¼¦~~ÄãÏë³ÅËÀ¼¦°¡£¿....ÍÛßÖ¡ğ¡ñ¡Á¡Á");
				press_any_key();
			}
			if (weight > 150)
			{
				moveto(9, 30);
				prints("¼¦³ÅÔÎÁË~~");
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
			prints("             ¡õ\r\n"
				   "       [33;1m¡õ[m¡õ¡ğ\r\n"
				   "       [37;42m¡ö¡ö[m\r\n"
				   "       [32m¡õ¡õ[m\r\n"
				   "       [32;40;1m¡õ¡õ[m\r\n"
				   "       [31m ¡õ [m\r\n"
				   "      [31m ¡õ¡õ[m   [32;1mË®¹û¾Æ±ùä¿ÁÜËÕ´ò[m   àÅ!ºÃºÈ!   ");
			pressany(1);
			zfood--;
			tiredstrong++;
			happy++;
			weight += 2;
			if (weight > 100)
			{
				moveto(9, 30);
				prints("Ì«ÖØÁËÀ²..·Ê¼¦~~");
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
			prints("[1;36m                              ¡õ¡õ¡õ¡õ¡õ[m\r\n"
				   "[1;33m                             ¡º[37m¡õ¡õ¡õ¡õ[m\r\n"
				   "[1;37m                               ¡õ¡õ¡õ¡õ[m\r\n"
				   "[1;37m             ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ[32m¡ò[37m¡õ¡õ¡õ¡õ[m\r\n"
				   "[37m             ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ[1;37m¡õ¡õ¡õ¡õ[m\r\n"
				   "[37m             ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ¡õ[1;33m ¡õ[m\r\n"
				   "[36m                  ¡õ¡õ¡õ¡õ¡õ¡õ[1;33m¡õ¡õ[m\r\n"
				   "[1;36m                  ¡õ¡õ¡õ¡õ¡õ¡õ[m\r\n"
				   "  [1;37m                ¡õ¡õ¡õ¡õ¡õ¡õ[m\r\n"
				   "                  Ò®Ò®Ò®...±ã±ãÀ­¹â¹â...                              ");

			pressany(2);
			tiredstrong += 5;
			clean = 0;
			break;
		case '4':
			guess();
			satis += (ptime->tm_sec % 2);
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
				prints("ÌÇ¹û²»×ã!!");
				press_any_key();
				break;
			}
			food += 5;
			prints("\r\nÊ³ÎïÓĞ [33;41m %d [m·İ", food);
			prints("   Ê£ÏÂ [33;41m %d [mÌÇ", money_balance());
			press_any_key();
			break;
		case '7':
			clrtobot(20);
			moveto(20, 0);
			if (money_withdraw(30) <= 0)
			{
				prints("ÌÇ¹û²»×ã!!");
				press_any_key();
				break;
			}
			zfood += 5;
			prints("\r\nÁãÊ³ÓĞ [33;41m %d [m·İ", zfood);
			prints("  Ê£ÏÂ [33;41m %d [mÌÇ", money_balance());
			press_any_key();
			break;
		case '8':
			if (oo > 0)
			{
				clrtobot(10);
				moveto(10, 0);
				prints("\r\n"
					   "               ¡õ¡õ¡õ¡õ\r\n"
					   "               ¡õ¡õ¡õ¡õ\r\n"
					   "               ¡õ¡õ¡õ¡õ\r\n"
					   "                          Íµ³Ô½ûÒ©´ó²¹Íè.....");
				tiredstrong = 0;
				happy += 3;
				oo--;
				pressany(6);
				break;
			}
			clrtobot(20);
			moveto(20, 4);
			prints("Ã»´ó²¹ÍèÀ²!!");
			press_any_key();
			break;
		case '9':
			if (age < 5)
			{
				clrtobot(20);
				moveto(20, 4);
				prints("Ì«Ğ¡ÁË...²»µÃ··ÊÛÎ´³ÉÄêĞ¡¼¦.....");
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
				prints("ÌÇ¹û²»×ã!!");
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

			if (get_data(22, 0, "°ïĞ¡¼¦È¡¸öºÃÃû×Ö£º", name_tmp, sizeof(name_tmp), DOECHO) > 0)
			{
				strncpy(chicken_name, name_tmp, sizeof(chicken_name) - 1);
				chicken_name[sizeof(chicken_name) - 1] = '\0';
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
	struct tm *ptime;
	time_t now;

	time(&now);
	ptime = localtime(&now);
	clearscr();
	clrtobot(5);
	if ((fp = fopen(LOG_FILE, "a")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", LOG_FILE);
		return -1;
	}
	fprintf(fp, "[32m%s[m ÔÚ [34;43m[%d/%d  %d:%02d][m  µÄĞ¡¼¦ [33m%s  [36m¹ÒÁË~~[m \r\n",
			BBS_username, ptime->tm_mon + 1, ptime->tm_mday,
			ptime->tm_hour, ptime->tm_min, chicken_name);
	fclose(fp);
	prints("ÎØ...Ğ¡¼¦¹ÒÁË....");
	prints("\r\n±¿Ê·ÁË...¸Ï³öÏµÍ³...");
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
	ch = igetch_t(MIN(MAX_DELAY_TIME, 60));
	moveto(23, 0);
	clrtoeol();
	iflush();
	return ch;
}

int guess()
{
	int ch, com;

	moveto(23, 0);
	prints("[1]-¼ôµ¶ [2]-Ê¯Í· [3]-²¼£º");
	clrtoeol();
	iflush();

	ch = igetch_t(MIN(MAX_DELAY_TIME, 60));
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
		prints("Ğ¡¼¦:¼ôµ¶");
		break;
	case 1:
		prints("Ğ¡¼¦:Ê¯Í·");
		break;
	case 2:
		prints("Ğ¡¼¦:²¼");
		break;
	}
	clrtoeol();

	moveto(19, 0);

	switch (ch)
	{
	case '1':
		prints("±¿¼¦---¿´ÎÒ¼ñÀ´µÄµ¶---");
		if (com == 0)
			tie();
		else if (com == 1)
			lose();
		else if (com == 2)
			win_c();
		break;
	case '2':
		prints("´ô¼¦---ÔÒÄãÒ»¿éÊ¯Í·!!---");
		if (com == 0)
			win_c();
		else if (com == 1)
			tie();
		else if (com == 2)
			lose();
		break;
	case '3':
		prints("´À¼¦---ËÍÄãÒ»¶ÑÆÆ²¼!---");
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
	prints("ÅĞ¶¨:Ğ¡¼¦ÊäÁË....    >_<~~~~~\r\n"
		   "\r\n"
		   "                                 ");
	return 0;
}
int tie()
{
	clrtobot(20);
	moveto(20, 0);
	prints("ÅĞ¶¨:Æ½ÊÖ                    -_-\r\n"
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
	prints("Ğ¡¼¦Ó®ÂŞ                      ¡É¡É\r\n"
		   "                               ¡õ       ");
	return 0;
}

int situ()
{
	clrtobot(16);
	moveto(16, 0);
	prints("           ");
	moveto(17, 0);
	prints("Äã:[44m %dÊ¤ %d¸º[m                   ", winn, losee);
	moveto(18, 0);
	prints("¼¦:[44m %dÊ¤ %d¸º[m                   ", losee, winn);

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
	struct tm *ptime;
	FILE *fp;
	time_t now;

	time(&now);
	ptime = localtime(&now);

	ans[0] = '\0';

	sel += (happy * 10);
	sel += (satis * 7);
	sel += ((ptime->tm_sec % 9) * 10);
	sel += weight;
	sel += age * 10;

	clrtobot(20);
	moveto(20, 0);
	prints("Ğ¡¼¦Öµ[33;45m$$ %d [mÌÇÌÇ", sel);
	if (get_data(19, 0, "ÕæµÄÒªÂôµôĞ¡¼¦?[y/N]", ans, sizeof(ans), DOECHO) < 0)
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
		prints("ÎŞ·¨´æÇ®£¬·ÅÆú½»Ò×£¡");
		return -2;
	}

	if ((fp = fopen(LOG_FILE, "a")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", LOG_FILE);
		return -2;
	}
	fprintf(fp, "[32m%s[m ÔÚ [34;43m[%d/%d  %d:%02d][m  °ÑĞ¡¼¦ [33m%s  [31mÒÔ [37;44m%d[m [31mÌÇ¹ûÂôÁË[m\r\n",
			BBS_username, ptime->tm_mon + 1, ptime->tm_mday,
			ptime->tm_hour, ptime->tm_min, chicken_name, sel);
	fclose(fp);
	clearscr();

	unlink(fname);
	load_chicken();

	return 0;
}
