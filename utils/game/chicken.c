/* µÁ◊”º¶ –°¬Î¡¶..º∏aº∏b”Œœ∑.°ı */

/* Writed by Birdman From 140.116.102.125 ¥¥“‚Õ€π˛π˛*/



#include "bbs.h"
#include "common.h"
#include "io.h"
#include "money.h"
#include <stdio.h>
#define DATA_FILE  "chicken"

char
*cstate[10]={"Œ“‘⁄≥‘∑π","Õµ≥‘¡„ ≥","¿≠±„±„","±øµ∞.. ‰∏¯º¶?","π˛..”Æ–°º¶“≤√ª∂‡π‚»Ÿ"
             ,"√ª ≥ŒÔ¿≤..","∆£¿Õ»´œ˚!"};
char *cage[9]={"µÆ…˙","÷‹ÀÍ","”◊ƒÍ","…ŸƒÍ","«‡ƒÍ","ªÓ¡¶","◊≥ƒÍ","÷–ƒÍ"};
char *menu[8]={"”Œœ∑","‘À∂Ø","µ˜ΩÃº∆ƒ‹","¬Ú¬Ùπ§æﬂ","«Â¿Ìº¶…·"};

time_t birth;
int weight,satis,mon,day,age,angery,sick,oo,happy,clean,tiredstrong,play;
int winn,losee,last,chictime,agetmp,food,zfood;
char Name[20];
FILE *cfp;
int gold,x[9]={0},ran,q_mon,p_mon;
unsigned long int bank;
char buf[1],buf1[6];

static	int	creat_a_egg(void);
static	int death(void) ;
static	int doit(void);
static	int guess(void) ;
static	int lose(void) ;
static	int	pressany(int i) ;
static	int sell(void) ;
static	int	show_chicken(void) ;
static	int show_m(void);
static	int situ(void) ;
static	int select_menu(void);
static	int tie(void) ;
static	int compare(char ans[],char buf[],int c);
static	int ga(char buf[],int l);
static	int win_c(void) ;

int chicken_main() {
    FILE *fp;
    time_t now = time(0);
    struct tm *ptime;
    char fname[50];

    agetmp=1;
//  modify_user_mode(CHICK);
    time(&now);
    ptime = localtime(&now);
    setuserfile(fname, DATA_FILE);
    if ((fp = fopen(fname, "r+")) == NULL) {
        creat_a_egg();
        last=1;
        fp = fopen(fname, "r");
        fscanf(fp,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s "
               ,&weight,&mon,&day,&satis,&age,&oo,&happy,&clean,&tiredstrong,&play
               ,&winn,&losee,&food,&zfood,Name);
        fclose(fp);
    } else {
        last=0;
        fscanf(fp,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s "
               ,&weight,&mon,&day,&satis,&age,&oo,&happy,&clean,&tiredstrong,&play
               ,&winn,&losee,&food,&zfood,Name);
        fclose(fp);
    }
    /*°ı*/
    if(day<(ptime->tm_mon+1)) {
        age=ptime->tm_mday;
        age=age+31-mon;
    } else
        age=ptime->tm_mday-mon;

    show_chicken();
    select_menu();
    fp = fopen(fname, "r+");
    fprintf(fp,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s "
            ,weight,mon,day,satis,age,oo,happy,clean,tiredstrong,play
            ,winn,losee,food,zfood,Name);

    fclose(fp);
    return 0;
}

static int creat_a_egg() {
    char fname[50];
    struct tm *ptime;
    FILE *fp;
    time_t now;

    time(&now);
    ptime = localtime(&now);
    clrtobot (2);
    while(strlen(Name)<1) {
        strcpy(Name,"±¶±¶");
        get_data (2, 0, "∞Ô–°º¶»°∏ˆ∫√√˚◊÷£∫", Name, 21, DOECHO);
    }
    setuserfile(fname, DATA_FILE);
    fp = fopen(fname, "w");
    fprintf(fp,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s "
            ,ptime->tm_hour*2,ptime->tm_mday,ptime->tm_mon+1
            ,0,0,0,0,0,0,0,0,0,10,5,Name);
    fclose(fp);
    if((fp = fopen("game/chicken", "a"))==NULL) {
        prints("Error!!cannot open then 'game/chicken'!\r\n");
        return  -1;
    }
    fprintf(fp,"[32m%s[m ‘⁄ [34;43m[%d/%d  %d:%02d][m  —¯¡À“ª÷ªΩ– [33m%s[m µƒ–°º¶\r\n",
            BBS_username,ptime->tm_mon + 1, ptime->tm_mday,
            ptime->tm_hour, ptime->tm_min,Name);
    fclose(fp);
    return	0;
}

static int show_chicken() {
    //int diff;
    /*int chictime;*/

    //diff = (time(0)/* - login_start_time*/) / 120;

    if(chictime>=200) {
        weight-=5;
        clean+=3;
        if(tiredstrong>2)
            tiredstrong-=2;
    }
    /*food=food-diff*3;*/
    if(weight<0)
        death();
    /*  if((diff-age)>1 && agetmp) ªπ «”–Œ Ã‚
       {age++;
        agetmp=0;}
    */
    clrtobot (1);
    prints(
        "[33mName:%s[m"
        "  [45mAge :%dÀÍ[m"
        "  ÷ÿ¡ø:%d"
        "   ≥ŒÔ:%d"
        "  ¡„ ≥:%d"
        "  ∆£¿Õ:%d"
        "  ±„±„:%d\r\n"
        "  …˙»’:%d‘¬%d»’"
        "  Ã«Ã«:%8d"
        "  øÏ¿÷∂»:%d"
        "  ¬˙“‚∂»:%d",
        // "  ¥Û≤πÕË:%d\r\n",
        Name,age,weight,food,zfood,tiredstrong,clean,day
        ,mon,BBS_user_money,happy,satis);//,oo);

    moveto (3,0);
    if(age<=16) {
        switch(age) {
        case 0:
        case 1:
            prints ("  °Ò°Ò°Ò°Ò\r\n"
                 "°Ò  °Ò°Ò  °Ò\r\n"
                 "°Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "°Ò°Ò    °Ò°Ò\r\n"
                 "°Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "  °Ò°Ò°Ò°Ò   ");

            break;
        case 2:
        case 3:
            prints ("    °Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "  °Ò            °Ò\r\n"
                 "°Ò    °Ò    °Ò    °Ò\r\n"
                 "°Ò                °Ò\r\n"
                 "°Ò      °Ò°Ò      °Ò\r\n"
                 "°Ò                °Ò\r\n"
                 "°Ò                °Ò\r\n"
                 "  °Ò            °Ò\r\n"
                 "    °Ò°Ò°Ò°Ò°Ò°Ò   ");

            break;
        case 4:
        case 5:

            prints ("      °Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "    °Ò            °Ò\r\n"
                 "  °Ò  °Ò        °Ò  °Ò\r\n"
                 "  °Ò                °Ò\r\n"
                 "  °Ò      °Ò°Ò      °Ò\r\n"
                 "°Ò°Ò°Ò    °Ò°Ò      °Ò°Ò\r\n"
                 "  °Ò                °Ò\r\n"
                 "  °Ò                °Ò\r\n"
                 "    °Ò  °Ò°Ò°Ò°Ò  °Ò\r\n"
                 "      °Ò      °Ò  °Ò\r\n"
                 "                °Ò    ");
            break;
        case 6:
        case 7:
            prints ("   °Ò    °Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "°Ò  °Ò°Ò  °Ò        °Ò\r\n"
                 "°Ò              °Ò    °Ò\r\n"
                 "  °Ò°Ò°Ò                °Ò\r\n"
                 "°Ò                      °Ò\r\n"
                 "°Ò  °Ò°Ò                °Ò\r\n"
                 "  °Ò  °Ò                °Ò\r\n"
                 "      °Ò                °Ò\r\n"
                 "        °Ò            °Ò\r\n"
                 "          °Ò°Ò°Ò°Ò°Ò°Ò        ");
            break;

        case 8:
        case 9:
        case 10:
            prints ("    °Ò°Ò          °Ò°Ò\r\n"
                 "  °Ò°Ò°Ò°Ò      °Ò°Ò°Ò°Ò\r\n"
                 "  °Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "  °Ò                  °Ò\r\n"
                 "  °Ò    °Ò      °Ò    °Ò\r\n"
                 "°Ò                      °Ò\r\n"
                 "°Ò        °Ò°Ò°Ò        °Ò\r\n"
                 "  °Ò                  °Ò\r\n"
                 "°Ò    °Ò          °Ò  °Ò\r\n"
                 "  °Ò°Ò            °Ò°Ò°Ò\r\n"
                 "  °Ò                  °Ò\r\n"
                 "    °Ò              °Ò\r\n"
                 "      °Ò  °Ò°Ò°Ò  °Ò\r\n"
                 "      °Ò  °Ò    °Ò\r\n"
                 "        °Ò               ");

            break;

        case 11:
        case 12:
            prints ("        °Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "      °Ò    °Ò    °Ò°Ò\r\n"
                 "    °Ò  °Ò      °Ò  °Ò°Ò\r\n"
                 "  °Ò°Ò              °Ò°Ò°Ò\r\n"
                 "°Ò              °Ò    °Ò°Ò\r\n"
                 "°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò      °Ò°Ò\r\n"
                 "  °Ò                  °Ò°Ò\r\n"
                 "    °Ò        °Ò  °Ò    °Ò\r\n"
                 "    °Ò        °Ò  °Ò    °Ò\r\n"
                 "    °Ò          °Ò      °Ò\r\n"
                 "      °Ò              °Ò\r\n"
                 "        °Ò  °Ò°Ò°Ò  °Ò\r\n"
                 "        °Ò  °Ò  °Ò  °Ò\r\n"
                 "          °Ò      °Ò             ");

            break;
        case 13:
        case 14:
            prints ("              °Ò°Ò°Ò°Ò\r\n"
                 "      °Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "    °Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "  °Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "  °Ò    °Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "°Ò°Ò    °Ò            °Ò°Ò\r\n"
                 "°Ò°Ò°Ò°Ò                °Ò\r\n"
                 "  °Ò                    °Ò\r\n"
                 "    °Ò°Ò            °Ò°Ò\r\n"
                 "  °Ò    °Ò°Ò°Ò°Ò°Ò°Ò  °Ò\r\n"
                 "  °Ò                  °Ò\r\n"
                 "    °Ò                  °Ò\r\n"
                 "      °Ò                °Ò\r\n"
                 "    °Ò°Ò°Ò            °Ò°Ò°Ò        ");
            break;
        case 15:
        case 16:
            prints ("  °Ò    °Ò°Ò°Ò°Ò°Ò°Ò\r\n"
                 "°Ò  °Ò°Ò  °Ò        °Ò\r\n"
                 "°Ò              °Ò    °Ò\r\n"
                 "  °Ò°Ò°Ò                °Ò\r\n"
                 "°Ò                      °Ò\r\n"
                 "°Ò  °Ò°Ò                °Ò\r\n"
                 "  °Ò  °Ò                °Ò\r\n"
                 "      °Ò        °Ò  °Ò    °Ò\r\n"
                 "      °Ò          °Ò      °Ò\r\n"
                 "      °Ò                  °Ò\r\n"
                 "        °Ò              °Ò\r\n"
                 "        °Ò  °Ò°Ò  °Ò°Ò°Ò\r\n"
                 "        °Ò  °Ò°Ò  °Ò\r\n"
                 "          °Ò    °Ò             ");

            break;
        }
    } else {
        prints ("          °Ò°Ò°Ò°Ò°Ò°Ò°Ò\r\n"
             "        °Ò          °Ò°Ò°Ò\r\n"
             "      °Ò    °Ò    °Ò  °Ò°Ò°Ò\r\n"
             "  °Ò°Ò°Ò°Ò°Ò°Ò°Ò        °Ò°Ò\r\n"
             "  °Ò          °Ò          °Ò\r\n"
             "  °Ò°Ò°Ò°Ò°Ò°Ò°Ò          °Ò            [1;5;31mŒ“ «¥Ûπ÷ƒÒ[m\r\n"
             "  °Ò        °Ò            °Ò\r\n"
             "  °Ò°Ò°Ò°Ò°Ò°Ò            °Ò\r\n"
             "  °Ò                    °Ò\r\n"
             "  °Ò                    °Ò\r\n"
             "    °Ò                °Ò\r\n"
             "°Ò°Ò  °Ò            °Ò\r\n"
             "°Ò      °Ò°Ò°Ò°Ò°Ò°Ò  °Ò°Ò\r\n"
             "  °Ò                      °Ò\r\n"
             "°Ò°Ò°Ò    Œ“ «¥Ûπ÷ƒÒ       °Ò°Ò°Ò ");

    }
    if(clean>10) {
        moveto (10,30);
        prints ("±„±„∫√∂‡..≥Ù≥Ù...");
        if(clean>15)
            death();
        press_any_key ();
    }

    moveto (17,0);
    prints ("[32m[1]-≥‘∑π     [2]-≥‘¡„ ≥   [3]-«Â¿Ìº¶…·   [4]-∏˙–°º¶≤¬»≠  [5]-ƒø«∞’Ωº®[m");
    prints ("\r\n[32m[6]-¬Úº¶À«¡œ$20  [7]-¬Ú¡„ ≥$30  [8]-≥‘¥Û≤πÕË  [9]-¬Ùº¶‡∏ [m");
    return	0;
}

static int select_menu() {
    char inbuf[80];
    //int diff;
    struct tm *ptime;
    time_t now;

    time(&now);
    ptime = localtime(&now);
    //diff = (time(0) /*- login_start_time*/) / 60;
    while(1) {
        moveto (23,0);
        prints("[0;46;31m   π”√∞Ô÷˙  [0;47;34m c ∏ƒ√˚◊÷   k …±º¶   t œ˚≥˝∑«∆£¿Õ($50)        [m");
    	strcpy (inbuf, "");
        get_data (22, 0, "“™◊ˆ–© ≤˜·ƒÿ?£∫[0]", inbuf, 4, DOECHO);
        if(tiredstrong>20) {
            clearscr ();
            moveto (15,30);
            prints ("Œÿ~~~–°º¶ª·¿€ªµµƒ...“™œ»»•–›œ¢“ªœ¬..");
            prints ("\r\n\r\n–›    —¯     ÷–");
        }
        switch(inbuf[0]) {
        case '1':
            if (food<=0) {
                pressany(5);
                break;
            }
            moveto (10,0);
            prints ("       °ı°ı°ı°ı°ı°ı\r\n"
                 "         °ﬂ°‡ °ı  °ı\r\n"
                 "              °ı  °ı                             °ı°ı°ı°ı  °ı          \r\n"
                 "              °ı  °ı     °ı              °ı      °ı°ı°ı°ı°ı°ı°ı            \r\n"
                 "         £√£Ô£Î£Â °ı    _°ı°ı°ı°ı°ı°ı°ı°ı°ı_    °ı°ı°ı°ı°ı°ı°ı°ı                  \r\n"
                 "             °ı   °ı     £•£•£•£•£•£•£•£•£•       °ı°™°…°…°™°ı          \r\n"
                 "            °ı    °ı     °ı°ı°ı°ı°ı°ı°ı°ı°ı       ©¶Mcdonald©¶      °°°°°° °°\r\n"
                 "           °ı     °ı     °˘°˘°˘°˘°˘°˘°˘°˘°˘°°     °ı°™°™°™°™°ı          \r\n"
                 "       °ı°ı°ı°ı°ı°ı      °ı°ı°ı°ı°ı°ı°ı°ı°ı     °ı°ı°ı°ı°ı°ı°ı°ı ");

            pressany(0);
            iflush ();
            sleep(1);
            food--;
            tiredstrong++;
            satis++;
            if(age<5)
                weight=weight+(5-age);
            else
                weight++;
            if(weight>100) {
                moveto (9,30);
                prints ("Ã´÷ÿ¡À¿≤..∑ º¶~~ƒ„œÎ≥≈À¿º¶∞°£ø....Õ€ﬂ÷°°Ò°¡°¡");
                press_any_key ();
            }
            if(weight>150) {
                moveto (9,30);
                prints ("º¶≥≈‘Œ¡À~~");
                press_any_key ();
            }
            if(weight>200)
                death();
            break;
        case '2':
            if (zfood<=0) {
                pressany(5);
                break;
            }
            moveto (10,0);
            prints ("             °ı\r\n"
                 "       [33;1m°ı[m°ı°\r\n"
                 "       [37;42m°ˆ°ˆ[m\r\n"
                 "       [32m°ı°ı[m\r\n"
                 "       [32;40;1m°ı°ı[m\r\n"
                 "       [31m °ı [m\r\n"
                 "      [31m °ı°ı[m   [32;1mÀÆπ˚æ∆±˘‰ø¡‹À’¥Ú[m   ‡≈!∫√∫»!   ");
            pressany(1);
            zfood--;
            tiredstrong++;
            happy++;
            weight+=2;
            if(weight>100) {
                moveto (9,30);
                prints ("Ã´÷ÿ¡À¿≤..∑ º¶~~");
                press_any_key ();
            }
            if(weight>200)
                death();
            break;
        case '3':
            moveto (10,0);
            prints ("[1;36m                              °ı°ı°ı°ı°ı[m\r\n"
                 "[1;33m                             °∫[37m°ı°ı°ı°ı[m\r\n"
                 "[1;37m                               °ı°ı°ı°ı[m\r\n"
                 "[1;37m             °ı°ı°ı°ı°ı°ı°ı°ı[32m°Ú[37m°ı°ı°ı°ı[m\r\n"
                 "[37m             °ı°ı°ı°ı°ı°ı°ı°ı°ı[1;37m°ı°ı°ı°ı[m\r\n"
                 "[37m             °ı°ı°ı°ı°ı°ı°ı°ı°ı[1;33m °ı[m\r\n"
                 "[36m                  °ı°ı°ı°ı°ı°ı[1;33m°ı°ı[m\r\n"
                 "[1;36m                  °ı°ı°ı°ı°ı°ı[m\r\n"
                 "  [1;37m                °ı°ı°ı°ı°ı°ı[m\r\n"
                 "                  “Æ“Æ“Æ...±„±„¿≠π‚π‚...                              ");

            pressany(2);
            tiredstrong+=5;
            clean=0;
            break;
        case '4':
            guess();
            satis+= (ptime->tm_sec%2);
            tiredstrong++;
            break;
        case '5':
            situ();
            break;
        case '6':
            moveto (20,0);
            if(BBS_user_money<20) {
                prints ("Ã«π˚≤ª◊„!!");
                press_any_key ();
                break;
            }
            food+=5;
            prints("\r\n ≥ŒÔ”– [33;41m %d [m∑›",food);
            prints("    £œ¬ [33;41m %d [mÃ«",demoney(20));
            press_any_key ();
            break;

        case '7':
            moveto (20,0);
            if(BBS_user_money<30) {
                prints ("Ã«π˚≤ª◊„!!");
                press_any_key ();
                break;
            }
            zfood+=5;
            prints("\r\n¡„ ≥”– [33;41m %d [m∑›",zfood);
            prints("   £œ¬ [33;41m %d [mÃ«",demoney(30));
            press_any_key ();
            break;
        case '8':
            moveto (21,0);
            if(oo>0) {
                moveto (10,0);
                prints ("\r\n"
                     "               °ı°ı°ı°ı\r\n"
                     "               °ı°ı°ı°ı\r\n"
                     "               °ı°ı°ı°ı\r\n"
                     "                          Õµ≥‘Ω˚“©¥Û≤πÕË.....");
                tiredstrong = 0;
                happy+=3;
                oo--;
                pressany(6);
                break;
            }
            moveto (20,4);
            prints ("√ª¥Û≤πÕË¿≤!!");
            press_any_key ();
            break;

        case '9':
            if(age<5) {
                moveto (20,4);
                prints("Ã´–°¡À...≤ªµ√∑∑ €Œ¥≥…ƒÍ–°º¶.....");
                press_any_key ();
                break;
            }
            sell();
            break;
        case 'k':
            death();
            break;
        case 't':
            tiredstrong = 0;
            BBS_user_money-=50;
            break;
        case 'c':
            get_data (22, 0, "∞Ô–°º¶»°∏ˆ∫√√˚◊÷£∫", Name, 21, DOECHO);
            break;
        default:
            return	-1;
            break;
        }
        show_chicken();
    }
    return	0;
}

int death() {
    char fname[50];
    FILE *fp;
    struct tm *ptime;

    time_t now;

    time(&now);
    ptime = localtime(&now);
    clearscr ();
    clrtobot (5);
    if((fp = fopen("game/chicken", "a"))!=NULL)
        prints("Error!\r\n");
    /*fp=fopen("game/chicken,"ab");*/
    fprintf(fp,"[32m%s[m ‘⁄ [34;43m[%d/%d  %d:%02d][m  µƒ–°º¶ [33m%s  [36mπ“¡À~~[m \r\n",
            BBS_username,ptime->tm_mon + 1, ptime->tm_mday,
            ptime->tm_hour, ptime->tm_min,Name);
    fclose(fp);
    prints ("Œÿ...–°º¶π“¡À....");
    prints ("\r\n±ø ∑¡À...∏œ≥ˆœµÕ≥...");
    press_any_key ();
    setuserfile(fname, DATA_FILE);

    unlink(fname);
    //strcpy(Name,"");
    creat_a_egg();
    chicken_main();
    //abort_bbs();
	return 0;
}

/*int comeclearscr ()
{
   extern struct commands cmdlist[];
 
  domenu(MMENU, "÷˜π¶\ƒ‹±Ì", (chkmail(0) ? 'M' : 'C'), cmdlist);
}
*/

int	pressany(i) {
    int ch;
    moveto (23,0);
    prints ("[33;46;1m                           [34m%s[37m                         [0m",cstate[i]);
    iflush ();
    do {
        ch = igetch ();
/*
        if (ch == KEY_ESC && KEY_ESC_arg == 'c')
            // capture_screen()
            clearscr ();
*/
    } while ((ch != ' ') && (ch != KEY_LEFT) && (ch != '\r') && (ch != '\n'));
    moveto (23, 0);
    clrtoeol();
    iflush ();
	return 0;
}

int guess() {
    int ch,com;
    struct tm *qtime;
    time_t now;

    time(&now);
    qtime = localtime(&now);

    do {
        /*getdata(22, 0, "[1]-ºÙµ∂ [2]- ØÕ∑ [3]-≤º£∫", inbuf, 4,
        DOECHO,NULL);*/
        moveto (23,0);
        prints ("[1]-ºÙµ∂ [2]- ØÕ∑ [3]-≤º£∫");
        ch = igetch ();
    } while((ch!='1')&&(ch!='2')&&(ch!='3'));

    /* com=qtime->tm_sec%3;*/
    com=rand()%3;
    moveto (21,35);
    switch(com) {
    case 0:
        prints ("–°º¶:ºÙµ∂");
        break;
    case 1:
        prints ("–°º¶: ØÕ∑");
        break;
    case 2:
        prints ("–°º¶:≤º");
        break;
    }

    moveto (19,0);


    switch(ch) {
    case '1':
        prints ("±øº¶---ø¥Œ“ºÒ¿¥µƒµ∂---");
        if (com==0)
            tie();
        else  if (com==1)
            lose();
        else if (com==2)
            win_c();
        break;
    case '2':
        prints ("¥Ùº¶---‘“ƒ„“ªøÈ ØÕ∑!!---");
        if (com==0)
            win_c();
        else if (com==1)
            tie();
        else if (com==2)
            lose();
        break;
    case '3':
        prints ("¥¿º¶---ÀÕƒ„“ª∂—∆∆≤º!---");
        if (com==0)
            lose();
        else if (com==1)
            win_c();
        else if (com==2)
            tie();
        break;
    }
    /* sleep(1);*/
    press_any_key ();
	return 0;
}

int win_c() {
    winn++;
    /* sleep(1);*/
    moveto (20,0);
    prints ("≈–∂®:–°º¶ ‰¡À....    >_<~~~~~\r\n"
         "\r\n"
         "                                 ");
    return	0;
}
int tie() {
    /* sleep(0);*/
    moveto (21,0);
    prints ("≈–∂®:∆Ω ÷                    -_-\r\n"
         "\r\n"
         "                                              ");
    return	0;
}
int lose() {
    losee++;
    happy+=2;
    /*sleep(0);*/
    moveto (21,0);
    prints ("–°º¶”Æ¬ﬁ                      °…°…\r\n"
         "                               °ı       ");
    return	0;
}

int situ() {

    moveto (16,0);
    prints ("           ");
    moveto (17,0);
    prints("ƒ„:[44m %d § %d∏∫[m                   ",winn,losee);
    moveto (18,0);
    prints("º¶:[44m %d § %d∏∫[m                   ",losee,winn);

    if (winn>=losee)
        pressany(4);
    else
        pressany(3);

    return	0;
}

void	p_bf() {
    FILE *fp;
    char fname[50];
//  modify_user_mode(CHICK);
    clearscr ();
    moveto (21,0);
    if(BBS_user_money<100) {
        prints ("Ã«π˚≤ª◊„!!");
        press_any_key ();
        return;
    }
    setuserfile(fname, "chicken");
    if ((fp = fopen(fname, "r+")) == NULL) {
        prints ("√ª—¯º¶..≤ª∏¯ƒ„¬Ú..π˛π˛...");
        press_any_key ();
        return;
    } else {
        fp = fopen(fname, "r");
        fscanf(fp,"%d %d %d %d %d %d %d %d %d %d %s %d %d"
               ,&weight,&mon,&day,&satis,&age,&oo,&happy,&clean,&tiredstrong,&play,Name
               ,&winn,&losee);
        fclose(fp);
        oo++;
        prints("\r\n¥Û≤πÕË”– %d ø≈",oo);
        prints("   £œ¬ %d Ã«,ª®«Æ100",demoney(100));
        press_any_key ();
        fp = fopen(fname, "r+");
        /*if (last!=1)
          { */
        fprintf(fp,"%d %d %d %d %d %d %d %d %d %d %s %d %d"
                ,weight,mon,day,satis,age,oo,happy,clean,tiredstrong,play,Name
                ,winn,losee);
        fclose(fp);
    }
    return;
}

/*
int year(char *useri) {
    FILE *fp;
    char fname[50];
    getuser(useri);
    sethomefile(fname, useri, "chicken");
    if ((fp = fopen(fname, "r+")) == NULL) {
        return -1;
    }
    fscanf(fp,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s "
           ,&weight,&mon,&day,&satis,&age,&oo,&happy,&clean,&tiredstrong,&play
           ,&winn,&losee,&food,&zfood,Name);
    fclose(fp);
    return age;

}
*/

int sell() {
    int sel=0;
    char ans[5];
    struct tm *ptime;
    FILE *fp;
    time_t now;

    time(&now);
    ptime = localtime(&now);

    get_data(20, 0, "»∑∂®“™¬ÙµÙ–°º¶?[y/N]",ans,3,DOECHO);
    if(ans[0]!='y')
        return	-1;
    sel+=(happy*10);
    sel+=(satis*7);
    sel+= ((ptime->tm_sec%9)*10);
    sel+= weight;
    sel+=age*10;
    moveto (20,0);
    prints("–°º¶÷µ[33;45m$$ %d [mÃ«Ã«",sel);
    get_data(19, 0, "’Êµƒ“™¬ÙµÙ–°º¶?[y/N]",ans,3,DOECHO);
    if(ans[0]!='y')
        return	-2;

    if((fp = fopen("game/chicken", "a"))!=NULL)
        ;
    fprintf(fp,"[32m%s[m ‘⁄ [34;43m[%d/%d  %d:%02d][m  ∞—–°º¶ [33m%s  [31m“‘ [37;44m%d[m [31mÃ«π˚¬Ù¡À[m\r\n",
            BBS_username,ptime->tm_mon + 1, ptime->tm_mday,
            ptime->tm_hour, ptime->tm_min,Name,sel);
    fclose(fp);
    clearscr ();

    inmoney(sel);
    strcpy(Name,"");
    creat_a_egg();
    chicken_main();
	return 0;
}

int gagb_c() {
    char abuf[5],buf1[6];
    char ans[5];
    int i,k,flag[11],count=0,GET=0;
    int l=1,money=0;

    //setutmpmode(NANB);
    clearscr ();
    do {
        /* while(strlen(buf1)<1)*/
        get_data (0, 0, "“™—∫∂‡…ŸÃ«π˚∞°(◊Ó¥Û2000)£∫", buf1, 5, DOECHO);
        money=atoi(buf1);
        if(BBS_user_money<money) {
            prints ("≤ªπª$$");
            press_any_key ();
            return 0;
        }
    } while((money<=0)||(money>2000));
    demoney(money);
    for(i=0;i<11;i++)
        flag[i]=0;
    for(i=0;i<4;i++) {
        do {
            k=rand()%9;
        } while (flag[k]!=0);
        flag[k]=1;
        ans[i]=k+'0';
    }
    while(!GET) {
        ga(abuf,l);
        if (abuf[0]=='q'&&abuf[1]=='k') {
            prints("Õ∂Ωµ..≤¬¡À %d¥Œ",count);
            prints("\r\n¥∞∏ «:%s",ans);
            press_any_key ();
            /*return 0*/
            ;
        }
        if(abuf[0]=='q') {
            prints("\r\n¥∞∏ «:%s",ans);
            press_any_key ();
            return 0;
        }
        if(compare(ans,abuf,count))
            /* GET=1;*/
            break;
        if(count>8) {
            prints ("[1;32mÕ€ﬂ÷..≤¬ Æ¥Œªπ≤ª∂‘...Ã«Ã«√ª ’..[m");
            press_any_key ();
            return 0;
        }
        count++;
        l+=2;
    }
    count++;
    switch(count) {
    case 1:
        money*=10;
        break;
    case 2:
    case 3:
        money*=6;
        break;
    case 4:
    case 5:
        money*=3;
        break;
    case 6:
        money*=2;
        break;
    case 7:
        money*=2;
        break;
    case 8:
        money*=1.1;
        break;
    case 9:
        money+=10;
        break;
        /*   case 8:
             money*=2;
             break;*/
    default:
        /*    money/=2;*/
        money=1;
        break;
    }
    inmoney(money);

    prints("\r\n÷’Ï∂∂‘¡À..≤¬¡À[32m %d[m ¥Œ …ÕÃ«Ã« [33;45m%d[m ø≈",count,money);
    press_any_key ();

    return 0;
}

static int compare(char ans[],char buf[],int c) {
    int i,j,A,B;

    A=0;
    B=0;
    for(i=0;i<4;i++)
        if(ans[i]==buf[i])
            A++;
    for(i=0;i<4;i++)
        for(j=0;j<4;j++)
            if((ans[i]==buf[j])&&(i!=j))
                B++;
    prints("%s",buf);
    prints("  Ω·π˚: %d A %d B  £ %d ¥Œ\r\n",A,B,9-c);
    /*  press_any_key (); */
    if(A==4)
        return 1;
    else
        return 0;
}

static int ga(char buf[],int l) {
    int ok=0;

    get_data (l, 0, " ‰»ÎÀ˘≤¬µƒ ˝◊÷(ÀƒŒª≤ª÷ÿ∏≤)£∫", buf, 5, DOECHO);
    if (strlen(buf)!=4) {
        if (buf[0]=='z'&&buf[1]=='k')
            return 0;
        if (buf[0]=='q')
            return 0;
        prints ("¬“¿¥..≤ª◊„4Œª");
        /* press_any_key ();*/
        return 0;
    }
    if((buf[0]!=buf[1])&&(buf[0]!=buf[2])&&(buf[0]!=buf[3])
            &&(buf[1]!=buf[2])&&
            (buf[1]!=buf[3])&&(buf[2]!=buf[3]))
        ok=1;
    if(ok!=1) {
        prints ("÷ÿ∏≤¬ﬁ");
        /*press_any_key ();*/
        return 0;
    }

    return 0;
}
/*
int nam(char *useri) {
    FILE *fp;
    char fname[50];
    getuser(useri);
    sethomefile(fname, useri, "chicken");
    if ((fp = fopen(fname, "r+")) == NULL) {
        return -1;
    }
    fscanf(fp,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %s "
           ,&weight,&mon,&day,&satis,&age,&oo,&happy,&clean,&tiredstrong,&play
           ,&winn,&losee,&food,&zfood,Name);
    fclose(fp);
    //return Name;
    return 1;
}
*/


int mary_m() {
    FILE *fp;
//  modify_user_mode(MARY);
    if ((fp = fopen("game/bank", "r")) == NULL) {
        fp = fopen("game/bank", "w");
        fprintf(fp,"%ld",1000000L);
        fclose(fp);
    }
    fp = fopen("game/bank", "r");
    fscanf(fp,"%ld",&bank);
    fclose(fp);
    clearscr ();
    p_mon=0;
    q_mon=BBS_user_money;
    show_m();

    fp = fopen("game/bank", "r+");
    fprintf(fp,"%ld",bank);
    fclose(fp);
    return	0;
}

static int show_m() {
    int i,j,k,m;

    moveto (0,0);
    clearscr ();
    prints ("              °ı°ı    °ı°ı\r\n"
         "            £Ø    £‹£Ø    £‹\r\n"
         "           £¸ °ı°ı £¸ °ı°ı £¸\r\n"
         "            £‹___£Ø°°£‹°ı°ı£Ø\r\n"
         "            ©¶  ___  ___  ©¶\r\n"
         "          £®©¶°ı_°ˆ°ı_°ˆ  ©¶£©\r\n"
         "        (~~.©¶   £‹£˜£Ø    ©¶.~~)\r\n"
         "       `£‹£Ø £‹    £Ô    £Ø £‹£Ø\r\n"
         "   °°     £‹   £‹°ı°ı°ı£Ø   £Ø\r\n"
         "   °°       £‹£Ø£¸ £¸ £¸£‹£Ø\r\n"
         "     °°      ©¶  °ı£œ°ı  ©¶\r\n"
         "     °°     °ı___£Ø£œ£‹___°ı\r\n"
         "       °°      £‹__£¸__£Ø    [31;40mª∂”≠π‚¡Ÿ–°¬Í¿Ú..[m");

    moveto (13,0);
    prints ("œ÷”–Ã«π˚: %-d            ±æª˙Ã®ƒ⁄œ÷Ω: %-ld",q_mon,bank);
    moveto (14,0);


    prints("[36m°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™[m\r\n");

    prints("∆ªπ˚-1 bar-2  777-3  Õıπ⁄-4 BAR-5  ¡ÂÓı-6 Œ˜πœ-7 ÈŸ◊”-8 ¿Û÷¶-9\r\n");
    prints("x5     x40    x30    x25    x50    x20    x15    x10    x2±∂\r\n");
    prints("%-7d%-7d%-7d%-7d%-7d%-7d%-7d%-7d%-7d\r\n",x[0],x[1],x[2],x[3],x[4],x[5],
           x[6],x[7],x[8]);

    prints("\r\n[36m°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™°™∞¥a»´—π°™°™°™°™∞¥sø™ º°™°™∞¥q¿Îø™°™°™[m");
    get_data (20, 0, "“™—∫º∏∫≈(ø…—∫∂‡¥Œ)£∫", buf, 2, DOECHO);
    switch(buf[0]) {
    case 's':
        doit();
        return	0;
        break;
    case 'a':
        get_data (21, 0, "“™—∫∂‡…ŸÃ«£∫", buf1, 6, DOECHO);
        for(i=0;i<=8;i++)
            x[i]=atoi(buf1);
        j=(x[0]*9);
        j=abs(j);
        if(q_mon<j) {
            prints ("Ã«π˚≤ª◊„");
            press_any_key ();
            for(i=0;i<=8;i++)
                x[i]=0;
            show_m();
            return	-1;
        }
        /*    demoney(j);*/
        q_mon-=j;
        p_mon+=j;
        /*       strcpy(buf," ");*/
        show_m();
        return	0;
        break;
    case 'q':
        for(i=0;i<=8;i++)
            x[i]=0;
        return	0;
        break;
    case 't':
        m=10000000;
        for(i=1;i<=5;i++) {
            clearscr ();
            moveto (20,i);
            prints ("x");
            if(i%3==0)
                m*=10;
            for(j=1;j<=m;j++)
                k=0;

            iflush ();

        }
        return	0;
        break;
    default:
        i=atoi(buf);
        break;
    }
    k=x[i-1];
    do {
        get_data (21, 0, "“™—∫∂‡…ŸÃ«£∫", buf1, 6, DOECHO);
        x[i-1]+=atoi(buf1);
        j=atoi(buf1);
    } while(x[i-1]<0);

    /*   j=x[i-1];*/
    if(j<0)
        j=abs(j);
    if(q_mon<j) {
        prints ("Ã«π˚≤ª◊„");
        press_any_key ();
        x[i-1]=k;
        clearscr ();
        j=0;
    }
    q_mon-=j;
    p_mon+=j;
    /* demoney(j);*/
    show_m();
    return	0;
}

static int doit() {
    int i,j,k,m,seed,flag=0,flag1=0;
    int g[10]={5,40,30,25,50,20,15,10,2,0};

    clearscr ();
    moveto (0,0);
    /*   prints ("
                           °ı°ı°ı°ı°ı°ı°ı°ı°ı°ı°ı
                           °ı                  °ı
                           °ı                  °ı
                           °ı £Œ£…£Œ£‘£≈£Œ£ƒ£œ °ı
                           °ı  ’˝‘⁄◊™µ±÷–      °ı
                           °ı      ◊‘––œÎœÒ    °ı
                           °ı                  °ı
                           °ı°ı°ı°ı°ı°ı°ı°ı°ı°ı°ı
                                  NINTENDO
     
                              °¸
                            °˚°Ú°˙           °Ò
                              °˝          °Ò
                                   °ı  °ı    .....
                                            .......
                                            .....°ı
                                                                  ");
    */
    m=1000000;
    for(i=1;i<=30;i++) {
        clearscr ();
        moveto (10,i);
        prints ("°Ò");
        if(i%23==0)
            m*=10;
        for(j=1;j<=m;j++)
            k=0;

        iflush ();

    }
    demoney(p_mon);
    iflush ();
    sleep(1);
    clearscr ();
    moveto (10,31);
    gold=0;
    seed=time(0)%10000;
    //if(p_mon>=50000)
    // seed=1500;

    do {
        ran=rand()%seed;
        flag1=0;

        moveto (10,31);
        if(ran<=480) {
            prints ("¿Û÷¶");
            j=8;
        } else if(ran<=670) {
            prints ("∆ªπ˚");
            j=0;
        } else if(ran<=765) {
            prints ("ÈŸ◊”");
            j=7;
        } else if(ran<=830) {
            prints ("Œ˜πœ");
            j=6;
        } else if(ran<=875) {
            prints ("¡ÂÓı");
            j=5;
        } else if(ran<=910) {
            prints ("Õıπ⁄");
            j=3;
        } else if(ran<=940) {
            prints ("777!");
            j=2;
        } else if(ran<=960) {
            prints ("bar!");
            j=1;
        } else if(ran<=975) {
            prints ("BAR!");
            j=4;
        } else {
            /*  prints ("test          œ˚»•”“±ﬂ  ‘Ÿ≈‹“ª¥Œ\r\n");
              for(i=4;i<=8;i++)*/
            prints ("√˙–ªª›πÀ");
            /* for(i=0;i<=8;i++)
              x[i]=0;*/
            j=9;
        }
        gold=x[j]*g[j];
        if(!flag)
            if(gold>=10000) {
                flag=1;
                flag1=1;
            }
        /*    } while( ran>976 || flag1 );*/
    } while(flag1);
    iflush ();
    sleep(1);
    moveto (11,25);
    prints("[32;40mƒ„ø…µ√[33;41m %d [32;40mÃ«Ã«[m",gold);
    iflush ();
    if (gold>0) {
        bank-=gold;
        bank+=p_mon;
    } else
        bank+=p_mon;

    inmoney(gold);
    press_any_key ();
    for(i=0;i<=8;i++)
        x[i]=0;
    p_mon=0;
    q_mon=BBS_user_money;

    show_m();
	return 0;
}
