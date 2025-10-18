
#include "bbs.h"
#include "database.h"
#include "display_user_info.h"
#include "io.h"
#include "ip_mask.h"
#include "log.h"
#include "screen.h"
#include "user_priv.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define ERR_MSG "查询用户出错\n"

int BBS_exp[]={0,50,200,500,1000,2000,5000,10000,20000,30000,50000,60000,70000,80000,90000,100000,INT_MAX};
char BBS_level[][16]={
	"新手上路",
	"初来乍练",
	"白手起家",
	"略懂一二",
	"小有作为",
	"对答如流",
	"精于此道",
	"博大精深",
	"登峰造极",
	"论坛砥柱",
	"☆☆☆☆☆",
	"★☆☆☆☆",
	"★★☆☆☆",
	"★★★☆☆",
	"★★★★☆",
	"★★★★★"
};
char BBS_Status[][16]={
	"受限",
	"游客",
	"普通用户",
	"副版主",
	"正版主",
	"副管理员",
	"正管理员"
};
char *getUserStatus(int level){
	if(level&P_ADMIN_M) return BBS_Status[6];
	else if(level&P_ADMIN_S) return BBS_Status[5];
	else if(level&P_MAN_M) return BBS_Status[4];
	else if(level&P_MAN_S) return BBS_Status[3];
	else if(level&P_USER) return BBS_Status[2];
	else if(level&P_GUEST) return BBS_Status[1];
	else return "未知";	
}

int getBBS_level_from_exp(int exp){		
	int l=0;
	int r=16;//sizeof(BBS_exp)/sizeof(BBS_exp[0])-1;
	while(l+1<r){
		int m=(l+r)/2;
		if(exp<BBS_exp[m]){
			r=m;
		}else{
			l=m;
		}
	}
	return l;
}

int iStrnCat(char *dest, const char *src){
	strncat(dest, src, sizeof(dest) - strlen(dest) - 1);
	return 0;
}

int display_user_info(int32_t uid)
{	
	MYSQL *db = NULL;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
    char username[BBS_username_max_len + 1];
    char nickname[BBS_nickname_max_len + 1];
	char sql[SQL_BUFFER_LEN];
	long user_life=0;
	int user_exp=0;
	int user_level=0;
	int visit_count=0;
	char user_gender='\0';
	int user_gender_pub=0;
	int gender_color=37;
	char user_introduction[BBS_inroduction_max_len+1]={'\0'};
	int user_post_count=0;
	char last_login_ip[16];
	time_t last_login_t;
	char last_login_date[20];	
	char user_reg_date[20];
	char user_status[16];
	int p_login=0;
	int p_post=0;
	int p_msg=0;
	char user_action[80]={'\0'};
    int ip_mask_level=2; //default mask level
    long leave_days=0;
    struct tm temp_tm;   
	time_t temp_t=0;
	BBS_user_priv q_user_priv;

	int line_no=1;
	
	clearscr();
	//moveto(line_no++, 1);

	db = db_open();
	if(db==NULL){
		log_error("db_open() error: %s\n", mysql_error(db));
		prints(ERR_MSG);			
		press_any_key();
		return -1;
	}
	if(load_priv(db, &q_user_priv, uid) != 0){
		log_error("load_priv() error: %d\n", uid);
		mysql_close(db);
		prints(ERR_MSG);
		press_any_key();
		return -1;
	}
	else{		
		//prints("%d %d", q_user_priv.uid, q_user_priv.level);
		//press_any_key();
		//return 0;
	}	

	snprintf(sql, sizeof(sql),
			 "SELECT u.username,u.p_login,u.p_post,u.p_msg,p.nickname,"
             "p.life,p.exp,p.visit_count,p.gender,p.gender_pub,"
             "UNIX_TIMESTAMP(r.signup_dt),r.signup_ip,p.introduction "
             //"r.signup_dt,r.signup_ip "
             "FROM user_list u "
             "Left Join user_pubinfo p ON u.uid=p.uid "
             "Left Join user_reginfo r ON u.uid=r.uid "
			 "WHERE u.uid = '%d'",
			 uid);
	if (mysql_query(db, sql) != 0){		
		log_error("mysql_query(db, sql) error: %s\n", mysql_error(db));
		mysql_close(db);
		prints(ERR_MSG);
		press_any_key();
		return 0;
	}
	
	if ((rs = mysql_store_result(db)) == NULL){
		log_error("mysql_store_result(db) error: %s\n", mysql_error(db));	
		mysql_close(db);	
		prints(ERR_MSG);
		press_any_key();
		return 0;	
	}

	if ((row = mysql_fetch_row(rs))){
		snprintf(username, sizeof(username),"%s",row[0]);	
		p_login=atoi(row[1]);	
		p_post=atoi(row[2]);	
		p_msg=atoi(row[3]);
		if(p_login&&p_post&&p_msg) snprintf(user_status, sizeof(user_status),"%s",getUserStatus(q_user_priv.level));
		else snprintf(user_status, sizeof(user_status),"%s",BBS_Status[0]);
		
        snprintf(nickname, sizeof(nickname),"%s",row[4]);		
		user_life = atoi(row[5]);
		user_exp = atoi(row[6]);
		user_level = getBBS_level_from_exp(user_exp);		
		visit_count=atoi(row[7]);

		user_gender=row[8][0];
		user_gender_pub=atoi(row[9]);
		gender_color=user_gender_pub==1?(user_gender=='M'?36:35):37;

		snprintf(user_introduction, sizeof(user_introduction),"%s",row[12]);
		
        //user_reg_tm=atol(row[10]);
        //temp_t = localtime(&user_reg_tm);
        //strftime(user_reg_date, sizeof(user_reg_date), "%Y-%m-%d %H:%M:%S", temp_t);

		//strptime(row[10],"%Y-%m-%d %H:%M:%S",&temp_tm);

		temp_t=atol(row[10]);
		localtime_r(&temp_t, &temp_tm);
		strftime(user_reg_date, sizeof(user_reg_date), "%Y-%m-%d %H:%M:%S", &temp_tm);
        //snprintf(user_reg_date, sizeof(user_reg_date),"%s",row[10]);
	}else{
		prints("用户不存在\n");
		mysql_close(db);	
		press_any_key();
		return 0;	
	}
	
	mysql_free_result(rs);
	rs = NULL;

	snprintf(sql, sizeof(sql),
			 "SELECT count(sid) FROM bbs "
			 "WHERE uid = '%d'",
			 uid);

	if (mysql_query(db, sql) != 0){		
		log_error("mysql_query(db, sql) error: %s\n", mysql_error(db));
		mysql_close(db);
		prints(ERR_MSG);
		press_any_key();
		return 0;
	}
	
	if ((rs = mysql_store_result(db)) == NULL){
		log_error("mysql_store_result(db) error: %s\n", mysql_error(db));		
		mysql_close(db);	
		prints(ERR_MSG);
		press_any_key();
		return 0;	
	}

	if ((row = mysql_fetch_row(rs))){
		user_post_count=atoi(row[0]);
	}	
	
	mysql_free_result(rs);
	rs = NULL;

	snprintf(sql, sizeof(sql),
			 "SELECT UNIX_TIMESTAMP(login_dt),login_ip FROM user_login_log "
			 "WHERE uid = '%d' ORDER BY id DESC",
			 uid);

	if (mysql_query(db, sql) != 0){		
		log_error("mysql_query(db, sql) error: %s\n", mysql_error(db));
		mysql_close(db);
		prints(ERR_MSG);
		press_any_key();
		return 0;
	}
	
	if ((rs = mysql_store_result(db)) == NULL){
		log_error("mysql_store_result(db) error: %s\n", mysql_error(db));		
		mysql_close(db);
		prints(ERR_MSG);	
		press_any_key();
		return 0;	
	}

	if ((row = mysql_fetch_row(rs))){
        last_login_t=atol(row[0]);
        if(user_life!=333&&user_life!=666&&user_life!=999){
            leave_days=(time(NULL)-last_login_t)/(60*60*24);
            user_life=user_life-leave_days-1;
        }

		localtime_r(&last_login_t, &temp_tm);
		strftime(last_login_date, sizeof(last_login_date), "%Y-%m-%d %H:%M:%S", &temp_tm);
		
		snprintf(last_login_ip, sizeof(last_login_ip),"%s",row[1]);
        ip_mask_level=(BBS_priv.level&P_ADMIN_M)||(BBS_priv.level&P_ADMIN_S)?1:2;
		ip_mask(last_login_ip, ip_mask_level, '*');
	}else{
        snprintf(last_login_date, sizeof(last_login_date),"从未登录");
        snprintf(last_login_ip, sizeof(last_login_ip),"-");
    }

	mysql_free_result(rs);
	rs = NULL;	

	//snprintf(sql, sizeof(sql),
	//		 "SELECT sid,current_action,last_tm FROM user_online "
	//		 "WHERE uid = '%d'",
	//		 uid);
	snprintf(sql, sizeof(sql),
			 "SELECT IF(last_tm < SUBDATE(NOW(), INTERVAL %d SECOND), 1, 0) AS timeout,"
			"sid, ip, last_tm, current_action FROM user_online WHERE UID = %d "
			"AND last_tm >= SUBDATE(NOW(), INTERVAL %d SECOND) "
			"ORDER BY last_tm DESC",BBS_user_off_line,uid,BBS_user_off_line);

	if (mysql_query(db, sql) != 0){		
		log_error("mysql_query(db, sql) error: %s\n", mysql_error(db));
		mysql_close(db);
		prints(ERR_MSG);
		press_any_key();
		return 0;
	}
	
	if ((rs = mysql_store_result(db)) == NULL){
		log_error("mysql_store_result(db) error: %s\n", mysql_error(db));		
		mysql_close(db);
		prints(ERR_MSG);	
		press_any_key();
		return 0;	
	}

	while ((row = mysql_fetch_row(rs))){
		if(strstr(row[1],"Telnet")&&row[4]){
			if(user_action[0]=='\0'){
				user_action[0]='[';
				iStrnCat(user_action,row[4]);
				iStrnCat(user_action,"]");
			}else{
				iStrnCat(user_action," [");
				iStrnCat(user_action,row[4]);
				iStrnCat(user_action,"]");
			}
		}else{
			if(user_action[0]=='\0'){
				user_action[0]='[';
				iStrnCat(user_action,"Web浏览]");		
			}else{
				iStrnCat(user_action,"[Web浏览]");			
			}
		}
	}

	moveto(line_no++, 1);
	prints("%s (%s) 上站 [%d] 发文 [%d]\n",username,nickname,visit_count,user_post_count);
	moveto(line_no++, 1);
	prints("上次在 [%s] 从 [%s] 访问本站 经验值 [%d]\n",last_login_date,last_login_ip,user_exp);
	moveto(line_no++, 1);
	prints("注册时间 [%s] 生命值 [%d] 等级 [%s(%d)] 身份 [\033[1;%dm%s\033[0m]\n", user_reg_date, user_life, BBS_level[user_level], user_level, gender_color, user_status);
	if(user_action[0]){
		moveto(line_no++, 1);
		prints("目前在站上，状态如下：");
		moveto(line_no++, 1);
		prints("%s\n",user_action);		
	}
	moveto(line_no++, 1);
	prints("\033[1;36m个人说明档如下：\033[0m\n");
	if(user_introduction[0]){
		char *token = strtok(user_introduction, "\n");  
		while(token != NULL && token[0]) {    		
			moveto(line_no++, 1);
			prints("%s", token);
    		token = strtok(NULL, "\n");
		}		
	}

	mysql_close(db);	
	press_any_key();
	return 0;
}