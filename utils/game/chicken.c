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

#define DATA_FILE "var/chicken"
#define LOG_FILE "var/chicken/log"

char
	*cstate[10] = {"ÎÒÔÚ³Ô·¹", "Íµ³ÔÁãÊ³", "À­±ã±ã", "±¿µ°..Êä¸ø¼¦?", "¹ş..Ó®Ğ¡¼¦Ò²Ã»¶à¹âÈÙ", "Ã»Ê³ÎïÀ²..", "Æ£ÀÍÈ«Ïû!"};
char *cage[9] = {"µ®Éú", "ÖÜËê", "Ó×Äê", "ÉÙÄê", "ÇàÄê", "»îÁ¦", "×³Äê", "ÖĞÄê"};
char *menu[8] = {"ÓÎÏ·", "ÔË¶¯", "µ÷½Ì¼ÆÄÜ", "ÂòÂô¹¤¾ß", "ÇåÀí¼¦Éá"};

time_t birth;
int weight, satis, mon, day, age, angery, sick, oo, happy, clean, tiredstrong, play;
int winn, losee, last, chictime, agetmp, food, zfood;
char Name[20];
FILE *cfp;
int gold, x[9] = {0}, ran, q_mon, p_mon;
unsigned long int bank;
char buf[1], buf1[6];

static int creat_a_egg(void);
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
	FILE *fp;
	time_t now;
	struct tm *ptime;
	char fname[FILE_PATH_LEN];

	agetmp = 1;
	//  modify_user_mode(CHICK);
	time(&now);
	ptime = localtime(&now);
	setuserfile(fname, sizeof(fname), DATA_FILE);
	if ((fp = fopen(fname, "r+")) == NULL)
	{
		creat_a_egg();
		last = 1;
		fp = fopen(fname, "r");
		fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ", &weight, &mon, &day, &satis, &age, &oo, &happy, &clean, &tiredstrong, &play, &winn, &losee, &food, &zfood, Name);
		fclose(fp);
	}
	else
	{
		last = 0;
		fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ", &weight, &mon, &day, &satis, &age, &oo, &happy, &clean, &tiredstrong, &play, &winn, &losee, &food, &zfood, Name);
		fclose(fp);
	}
	/*¡õ*/
	if (day < (ptime->tm_mon + 1))
	{
		age = ptime->tm_mday;
		age = age + 31 - mon;
	}
	else
		age = ptime->tm_mday - mon;

	show_chicken();
	select_menu();
	fp = fopen(fname, "r+");
	fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ", weight, mon, day, satis, age, oo, happy, clean, tiredstrong, play, winn, losee, food, zfood, Name);

	fclose(fp);
	return 0;
}

static int creat_a_egg()
{
	char fname[50];
	struct tm *ptime;
	FILE *fp;
	time_t now;
	time(&now);
	ptime = localtime(&now);

	clrtobot(2);
	while (Name[0] == '\0')
	{
		strncpy(Name, "±¦±¦", sizeof(Name) - 1);
		Name[sizeof(Name) - 1] = '\0';

		get_data(2, 0, "°ïĞ¡¼¦È¡¸öºÃÃû×Ö£º", Name, 21, DOECHO);
	}

	setuserfile(fname, sizeof(fname), DATA_FILE);
	if ((fp = fopen(fname, "w")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", fname);
		return -1;
	}
	fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ", ptime->tm_hour * 2, ptime->tm_mday, ptime->tm_mon + 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 5, Name);
	fclose(fp);
	if ((fp = fopen(LOG_FILE, "a")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", LOG_FILE);
		return -1;
	}
	fprintf(fp, "[32m%s[m ÔÚ [34;43m[%d/%d  %d:%02d][m  ÑøÁËÒ»Ö»½Ğ [33m%s[m µÄĞ¡¼¦\r\n",
			BBS_username, ptime->tm_mon + 1, ptime->tm_mday,
			ptime->tm_hour, ptime->tm_min, Name);
	fclose(fp);
	return 0;
}

static int show_chicken()
{
	// int diff;
	/*int chictime;*/

	// diff = (time(0)/* - login_start_time*/) / 120;

	if (chictime >= 200)
	{
		weight -= 5;
		clean += 3;
		if (tiredstrong > 2)
			tiredstrong -= 2;
	}
	/*food=food-diff*3;*/
	if (weight < 0)
		death();
	/*  if((diff-age)>1 && agetmp) »¹ÊÇÓĞÎÊÌâ
	   {age++;
		agetmp=0;}
	*/
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
		Name, age, weight, food, zfood, tiredstrong, clean, day, mon, BBS_user_money, happy, satis); //,oo);

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
	char inbuf[80];
	// int diff;
	struct tm *ptime;
	time_t now;

	time(&now);
	ptime = localtime(&now);
	// diff = (time(0) /*- login_start_time*/) / 60;
	while (1)
	{
		moveto(23, 0);
		prints("[0;46;31m  Ê¹ÓÃ°ïÖú  [0;47;34m c ¸ÄÃû×Ö   k É±¼¦   t Ïû³ı·ÇÆ£ÀÍ($50)        [m");
		inbuf[0] = '\0';
		get_data(22, 0, "Òª×öĞ©Ê²÷áÄØ?£º[0]", inbuf, 4, DOECHO);
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
			sleep(1);
			food--;
			tiredstrong++;
			satis++;
			if (age < 5)
				weight = weight + (5 - age);
			else
				weight++;
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
				death();
			break;
		case '2':
			if (zfood <= 0)
			{
				pressany(5);
				break;
			}
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
				death();
			break;
		case '3':
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
			moveto(20, 0);
			if (BBS_user_money < 20)
			{
				prints("ÌÇ¹û²»×ã!!");
				press_any_key();
				break;
			}
			food += 5;
			prints("\r\nÊ³ÎïÓĞ [33;41m %d [m·İ", food);
			prints("   Ê£ÏÂ [33;41m %d [mÌÇ", demoney(20));
			press_any_key();
			break;

		case '7':
			moveto(20, 0);
			if (BBS_user_money < 30)
			{
				prints("ÌÇ¹û²»×ã!!");
				press_any_key();
				break;
			}
			zfood += 5;
			prints("\r\nÁãÊ³ÓĞ [33;41m %d [m·İ", zfood);
			prints("  Ê£ÏÂ [33;41m %d [mÌÇ", demoney(30));
			press_any_key();
			break;
		case '8':
			moveto(21, 0);
			if (oo > 0)
			{
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
			moveto(20, 4);
			prints("Ã»´ó²¹ÍèÀ²!!");
			press_any_key();
			break;

		case '9':
			if (age < 5)
			{
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
			tiredstrong = 0;
			BBS_user_money -= 50;
			break;
		case 'c':
			get_data(22, 0, "°ïĞ¡¼¦È¡¸öºÃÃû×Ö£º", Name, 21, DOECHO);
			break;
		default:
			return -1;
			break;
		}
		show_chicken();
	}
	return 0;
}

int death()
{
	char fname[50];
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
			ptime->tm_hour, ptime->tm_min, Name);
	fclose(fp);
	prints("ÎØ...Ğ¡¼¦¹ÒÁË....");
	prints("\r\n±¿Ê·ÁË...¸Ï³öÏµÍ³...");
	press_any_key();
	setuserfile(fname, sizeof(fname), DATA_FILE);

	unlink(fname);
	creat_a_egg();
	chicken_main();
	// abort_bbs();
	return 0;
}

/*int comeclearscr ()
{
   extern struct commands cmdlist[];

  domenu(MMENU, "Ö÷¹¦\ÄÜ±í", (chkmail(0) ? 'M' : 'C'), cmdlist);
}
*/

int pressany(int i)
{
	int ch;
	moveto(23, 0);
	prints("[33;46;1m                           [34m%s[37m                         [0m", cstate[i]);
	iflush();
	do
	{
		ch = igetch(0);
		/*
				if (ch == KEY_ESC && KEY_ESC_arg == 'c')
					// capture_screen()
					clearscr ();
		*/
	} while ((ch != ' ') && (ch != KEY_LEFT) && (ch != '\r') && (ch != '\n'));
	moveto(23, 0);
	clrtoeol();
	iflush();
	return 0;
}

int guess()
{
	int ch, com;

	do
	{
		/*getdata(22, 0, "[1]-¼ôµ¶ [2]-Ê¯Í· [3]-²¼£º", inbuf, 4,
		DOECHO,NULL);*/
		moveto(23, 0);
		prints("[1]-¼ôµ¶ [2]-Ê¯Í· [3]-²¼£º");
		clrtoeol();
		iflush();
		ch = igetch(0);
	} while ((ch != '1') && (ch != '2') && (ch != '3'));

	/* com=qtime->tm_sec%3;*/
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
	/* sleep(1);*/
	press_any_key();
	return 0;
}

int win_c()
{
	winn++;
	/* sleep(1);*/
	moveto(20, 0);
	prints("ÅĞ¶¨:Ğ¡¼¦ÊäÁË....    >_<~~~~~\r\n"
		   "\r\n"
		   "                                 ");
	return 0;
}
int tie()
{
	/* sleep(0);*/
	moveto(21, 0);
	prints("ÅĞ¶¨:Æ½ÊÖ                    -_-\r\n"
		   "\r\n"
		   "                                              ");
	return 0;
}
int lose()
{
	losee++;
	happy += 2;
	/*sleep(0);*/
	moveto(21, 0);
	prints("Ğ¡¼¦Ó®ÂŞ                      ¡É¡É\r\n"
		   "                               ¡õ       ");
	return 0;
}

int situ()
{

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
	char ans[5];
	struct tm *ptime;
	FILE *fp;
	time_t now;

	time(&now);
	ptime = localtime(&now);

	get_data(20, 0, "È·¶¨ÒªÂôµôĞ¡¼¦?[y/N]", ans, 3, DOECHO);
	if (ans[0] != 'y')
	{
		return -1;
	}
	sel += (happy * 10);
	sel += (satis * 7);
	sel += ((ptime->tm_sec % 9) * 10);
	sel += weight;
	sel += age * 10;

	if (sel < 0)
	{
		return -2;
	}

	moveto(20, 0);
	prints("Ğ¡¼¦Öµ[33;45m$$ %d [mÌÇÌÇ", sel);
	get_data(19, 0, "ÕæµÄÒªÂôµôĞ¡¼¦?[y/N]", ans, 3, DOECHO);
	if (ans[0] != 'y')
		return -2;

	if ((fp = fopen(LOG_FILE, "a")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", LOG_FILE);
		return -1;
	}
	fprintf(fp, "[32m%s[m ÔÚ [34;43m[%d/%d  %d:%02d][m  °ÑĞ¡¼¦ [33m%s  [31mÒÔ [37;44m%d[m [31mÌÇ¹ûÂôÁË[m\r\n",
			BBS_username, ptime->tm_mon + 1, ptime->tm_mday,
			ptime->tm_hour, ptime->tm_min, Name, sel);
	fclose(fp);
	clearscr();

	inmoney((unsigned int)sel);
	Name[0] = '\0';
	creat_a_egg();
	chicken_main();
	return 0;
}
