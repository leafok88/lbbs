/* ���Ӽ� С����..��a��b��Ϸ.�� */

/* Writed by Birdman From 140.116.102.125 �����۹���*/

#include "bbs.h"
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

#define DATA_FILE "var/chicken"
#define LOG_FILE "var/chicken/log"
#define CHICKEN_NAME_LEN 20

char
	*cstate[10] = {"���ڳԷ�", "͵����ʳ", "�����", "����..�����?", "��..ӮС��Ҳû�����", "ûʳ����..", "ƣ��ȫ��!"};
char *cage[9] = {"����", "����", "����", "����", "����", "����", "׳��", "����"};
char *menu[8] = {"��Ϸ", "�˶�", "���̼���", "��������", "������"};

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

	agetmp = 1;
	//  modify_user_mode(CHICK);
	time(&now);
	localtime_r(&now, &ptime);

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
	fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ", weight, mon, day, satis, age, oo, happy, clean, tiredstrong, play, winn, losee, food, zfood, chicken_name);
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

	clrtobot(2);
	while (!SYS_server_exit && chicken_name[0] == '\0')
	{
		strncpy(chicken_name, "����", sizeof(chicken_name) - 1);
		chicken_name[sizeof(chicken_name) - 1] = '\0';

		strncpy(name_tmp, chicken_name, sizeof(name_tmp) - 1);
		name_tmp[sizeof(name_tmp) - 1] = '\0';

		if (get_data(2, 0, "��С��ȡ�������֣�", name_tmp, sizeof(name_tmp), DOECHO) > 0)
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
	fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s ", ptime.tm_hour * 2, ptime.tm_mday, ptime.tm_mon + 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 5, chicken_name);
	fclose(fp);

	if ((fp = fopen(LOG_FILE, "a")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", LOG_FILE);
		return -2;
	}
	fprintf(fp, "[32m%s[m �� [34;43m[%d/%d  %d:%02d][m  ����һֻ�� [33m%s[m ��С��\r\n",
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
		"  [45mAge :%d��[m"
		"  ����:%d"
		"  ʳ��:%d"
		"  ��ʳ:%d"
		"  ƣ��:%d"
		"  ���:%d\r\n"
		"  ����:%d��%d��"
		"  ����:%8d"
		"  ���ֶ�:%d"
		"  �����:%d",
		// "  ����:%d\r\n",
		chicken_name, age, weight, food, zfood, tiredstrong, clean, day, mon, money_balance(), happy, satis); //,oo);

	moveto(3, 0);
	if (age <= 16)
	{
		switch (age)
		{
		case 0:
		case 1:
			prints("  �����\r\n"
				   "��  ���  ��\r\n"
				   "�������\r\n"
				   "���    ���\r\n"
				   "�������\r\n"
				   "  �����   ");

			break;
		case 2:
		case 3:
			prints("    �������\r\n"
				   "  ��            ��\r\n"
				   "��    ��    ��    ��\r\n"
				   "��                ��\r\n"
				   "��      ���      ��\r\n"
				   "��                ��\r\n"
				   "��                ��\r\n"
				   "  ��            ��\r\n"
				   "    �������   ");

			break;
		case 4:
		case 5:

			prints("      �������\r\n"
				   "    ��            ��\r\n"
				   "  ��  ��        ��  ��\r\n"
				   "  ��                ��\r\n"
				   "  ��      ���      ��\r\n"
				   "����    ���      ���\r\n"
				   "  ��                ��\r\n"
				   "  ��                ��\r\n"
				   "    ��  �����  ��\r\n"
				   "      ��      ��  ��\r\n"
				   "                ��    ");
			break;
		case 6:
		case 7:
			prints("   ��    �������\r\n"
				   "��  ���  ��        ��\r\n"
				   "��              ��    ��\r\n"
				   "  ����                ��\r\n"
				   "��                      ��\r\n"
				   "��  ���                ��\r\n"
				   "  ��  ��                ��\r\n"
				   "      ��                ��\r\n"
				   "        ��            ��\r\n"
				   "          �������        ");
			break;

		case 8:
		case 9:
		case 10:
			prints("    ���          ���\r\n"
				   "  �����      �����\r\n"
				   "  ������������\r\n"
				   "  ��                  ��\r\n"
				   "  ��    ��      ��    ��\r\n"
				   "��                      ��\r\n"
				   "��        ����        ��\r\n"
				   "  ��                  ��\r\n"
				   "��    ��          ��  ��\r\n"
				   "  ���            ����\r\n"
				   "  ��                  ��\r\n"
				   "    ��              ��\r\n"
				   "      ��  ����  ��\r\n"
				   "      ��  ��    ��\r\n"
				   "        ��               ");

			break;

		case 11:
		case 12:
			prints("        �������\r\n"
				   "      ��    ��    ���\r\n"
				   "    ��  ��      ��  ���\r\n"
				   "  ���              ����\r\n"
				   "��              ��    ���\r\n"
				   "���������      ���\r\n"
				   "  ��                  ���\r\n"
				   "    ��        ��  ��    ��\r\n"
				   "    ��        ��  ��    ��\r\n"
				   "    ��          ��      ��\r\n"
				   "      ��              ��\r\n"
				   "        ��  ����  ��\r\n"
				   "        ��  ��  ��  ��\r\n"
				   "          ��      ��             ");

			break;
		case 13:
		case 14:
			prints("              �����\r\n"
				   "      ���������\r\n"
				   "    �����������\r\n"
				   "  �������������\r\n"
				   "  ��    ����������\r\n"
				   "���    ��            ���\r\n"
				   "�����                ��\r\n"
				   "  ��                    ��\r\n"
				   "    ���            ���\r\n"
				   "  ��    �������  ��\r\n"
				   "  ��                  ��\r\n"
				   "    ��                  ��\r\n"
				   "      ��                ��\r\n"
				   "    ����            ����        ");
			break;
		case 15:
		case 16:
			prints("  ��    �������\r\n"
				   "��  ���  ��        ��\r\n"
				   "��              ��    ��\r\n"
				   "  ����                ��\r\n"
				   "��                      ��\r\n"
				   "��  ���                ��\r\n"
				   "  ��  ��                ��\r\n"
				   "      ��        ��  ��    ��\r\n"
				   "      ��          ��      ��\r\n"
				   "      ��                  ��\r\n"
				   "        ��              ��\r\n"
				   "        ��  ���  ����\r\n"
				   "        ��  ���  ��\r\n"
				   "          ��    ��             ");

			break;
		}
	}
	else
	{
		prints("          ��������\r\n"
			   "        ��          ����\r\n"
			   "      ��    ��    ��  ����\r\n"
			   "  ��������        ���\r\n"
			   "  ��          ��          ��\r\n"
			   "  ��������          ��            [1;5;31m���Ǵ����[m\r\n"
			   "  ��        ��            ��\r\n"
			   "  �������            ��\r\n"
			   "  ��                    ��\r\n"
			   "  ��                    ��\r\n"
			   "    ��                ��\r\n"
			   "���  ��            ��\r\n"
			   "��      �������  ���\r\n"
			   "  ��                      ��\r\n"
			   "����    ���Ǵ����       ���� ");
	}
	if (clean > 10)
	{
		moveto(10, 30);
		prints("���ö�..����...");
		if (clean > 15)
			death();
		press_any_key();
	}

	moveto(17, 0);
	prints("[32m[1]-�Է�     [2]-����ʳ   [3]-������   [4]-��С����ȭ  [5]-Ŀǰս��[m");
	prints("\r\n[32m[6]-������$20  [7]-����ʳ$30  [8]-�Դ���  [9]-����� [m");
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

	while (!SYS_server_exit && loop)
	{
		moveto(23, 0);
		prints("[0;46;31m  ʹ�ð���  [0;47;34m c ������   k ɱ��   t ������ƣ��($50)   q �˳�     [m");
		inbuf[0] = '\0';
		if (get_data(22, 0, "Ҫ��Щʲô��?��", inbuf, sizeof(inbuf), DOECHO) < 0)
		{
			return 0; // input timeout
		}
		if (tiredstrong > 20)
		{
			clearscr();
			moveto(15, 30);
			prints("��~~~С�����ۻ���...Ҫ��ȥ��Ϣһ��..");
			prints("\r\n\r\n��    ��     ��");
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
			prints("       ������������\r\n"
				   "         �ߡ� ��  ��\r\n"
				   "              ��  ��                             ��������  ��          \r\n"
				   "              ��  ��     ��              ��      ��������������            \r\n"
				   "         �ã��� ��    _������������������_    ����������������                  \r\n"
				   "             ��   ��     ������������������       �����ɡɡ���          \r\n"
				   "            ��    ��     ������������������       ��Mcdonald��      ������ ��\r\n"
				   "           ��     ��     ��������������������     ������������          \r\n"
				   "       ������������      ������������������     ���������������� ");

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
				prints("̫������..�ʼ�~~�������������....���֡�����");
				press_any_key();
			}
			if (weight > 150)
			{
				moveto(9, 30);
				prints("��������~~");
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
			prints("             ��\r\n"
				   "       [33;1m��[m����\r\n"
				   "       [37;42m����[m\r\n"
				   "       [32m����[m\r\n"
				   "       [32;40;1m����[m\r\n"
				   "       [31m �� [m\r\n"
				   "      [31m ����[m   [32;1mˮ���Ʊ�����մ�[m   ��!�ú�!   ");
			pressany(1);
			zfood--;
			tiredstrong++;
			happy++;
			weight += 2;
			if (weight > 100)
			{
				moveto(9, 30);
				prints("̫������..�ʼ�~~");
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
			prints("[1;36m                              ����������[m\r\n"
				   "[1;33m                             ��[37m��������[m\r\n"
				   "[1;37m                               ��������[m\r\n"
				   "[1;37m             ����������������[32m��[37m��������[m\r\n"
				   "[37m             ������������������[1;37m��������[m\r\n"
				   "[37m             ������������������[1;33m ��[m\r\n"
				   "[36m                  ������������[1;33m����[m\r\n"
				   "[1;36m                  ������������[m\r\n"
				   "  [1;37m                ������������[m\r\n"
				   "                  ҮҮҮ...��������...                              ");

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
				prints("�ǹ�����!!");
				press_any_key();
				break;
			}
			food += 5;
			prints("\r\nʳ���� [33;41m %d [m��", food);
			prints("   ʣ�� [33;41m %d [m��", money_balance());
			press_any_key();
			break;
		case '7':
			clrtobot(20);
			moveto(20, 0);
			if (money_withdraw(30) <= 0)
			{
				prints("�ǹ�����!!");
				press_any_key();
				break;
			}
			zfood += 5;
			prints("\r\n��ʳ�� [33;41m %d [m��", zfood);
			prints("  ʣ�� [33;41m %d [m��", money_balance());
			press_any_key();
			break;
		case '8':
			if (oo > 0)
			{
				clrtobot(10);
				moveto(10, 0);
				prints("\r\n"
					   "               ��������\r\n"
					   "               ��������\r\n"
					   "               ��������\r\n"
					   "                          ͵�Խ�ҩ����.....");
				tiredstrong = 0;
				happy += 3;
				oo--;
				pressany(6);
				break;
			}
			clrtobot(20);
			moveto(20, 4);
			prints("û������!!");
			press_any_key();
			break;
		case '9':
			if (age < 5)
			{
				clrtobot(20);
				moveto(20, 4);
				prints("̫С��...���÷���δ����С��.....");
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
				prints("�ǹ�����!!");
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

			if (get_data(22, 0, "��С��ȡ�������֣�", name_tmp, sizeof(name_tmp), DOECHO) > 0)
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
	fprintf(fp, "[32m%s[m �� [34;43m[%d/%d  %d:%02d][m  ��С�� [33m%s  [36m����~~[m \r\n",
			BBS_username, ptime.tm_mon + 1, ptime.tm_mday,
			ptime.tm_hour, ptime.tm_min, chicken_name);
	fclose(fp);
	prints("��...С������....");
	prints("\r\n��ʷ��...�ϳ�ϵͳ...");
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
	prints("[1]-���� [2]-ʯͷ [3]-����");
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
		prints("С��:����");
		break;
	case 1:
		prints("С��:ʯͷ");
		break;
	case 2:
		prints("С��:��");
		break;
	}
	clrtoeol();

	moveto(19, 0);

	switch (ch)
	{
	case '1':
		prints("����---���Ҽ����ĵ�---");
		if (com == 0)
			tie();
		else if (com == 1)
			lose();
		else if (com == 2)
			win_c();
		break;
	case '2':
		prints("����---����һ��ʯͷ!!---");
		if (com == 0)
			win_c();
		else if (com == 1)
			tie();
		else if (com == 2)
			lose();
		break;
	case '3':
		prints("����---����һ���Ʋ�!---");
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
	prints("�ж�:С������....    >_<~~~~~\r\n"
		   "\r\n"
		   "                                 ");
	return 0;
}
int tie()
{
	clrtobot(20);
	moveto(20, 0);
	prints("�ж�:ƽ��                    -_-\r\n"
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
	prints("С��Ӯ��                      �ɡ�\r\n"
		   "                               ��       ");
	return 0;
}

int situ()
{
	clrtobot(16);
	moveto(16, 0);
	prints("           ");
	moveto(17, 0);
	prints("��:[44m %dʤ %d��[m                   ", winn, losee);
	moveto(18, 0);
	prints("��:[44m %dʤ %d��[m                   ", losee, winn);

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
	prints("С��ֵ[33;45m$$ %d [m����", sel);
	if (get_data(19, 0, "���Ҫ����С��?[y/N]", ans, sizeof(ans), DOECHO) < 0)
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
		prints("�޷���Ǯ���������ף�");
		return -2;
	}

	if ((fp = fopen(LOG_FILE, "a")) == NULL)
	{
		log_error("Error!!cannot open file '%s'!\n", LOG_FILE);
		return -2;
	}
	fprintf(fp, "[32m%s[m �� [34;43m[%d/%d  %d:%02d][m  ��С�� [33m%s  [31m�� [37;44m%d[m [31m�ǹ�����[m\r\n",
			BBS_username, ptime.tm_mon + 1, ptime.tm_mday,
			ptime.tm_hour, ptime.tm_min, chicken_name, sel);
	fclose(fp);
	clearscr();

	unlink(fname);
	load_chicken();

	return 0;
}
