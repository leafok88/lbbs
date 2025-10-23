#include "display_user_info.h"
#include "io.h"
#include "log.h"
#include "screen.h"
#include "user_info_display.h"
#define ERR_MSG "查询用户出错\n"

int display_user_info(int32_t uid)
{
	USER_INFO q_user_info;
	int x=query_user_info_by_uid(uid,&q_user_info);

	clearscr();	
	
	if(x!=1){
		log_error("query_user_info_by_uid() error: %d\n%d\n", uid,x);		
		prints(ERR_MSG);	
		press_any_key();
		return -1;
	}

	if(user_info_display(&q_user_info)!=0){

		log_error("user_info_display() error");
		prints(ERR_MSG);	
		press_any_key();
	}
	
	press_any_key();
	return 0;
}