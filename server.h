#ifndef _H_IHOMESERVER
#define _H_IHOMESERVER

#define MESIZE 8192
#define ACCOUNT_MAX 32

#define COMMAND_PULSE  '0'
#define COMMAND_MANAGE 1
#define COMMAND_CONTRL 2
#define COMMAND_RESULT 3
#define MAN_LOGIN 11
#define CTL_LAMP  21
#define CTL_GET        22
#define RES_LOGIN      32
#define RES_LAMP       33
#define RES_TEMP       34
#define RES_HUMI       35

#define LAMP_ON        1
#define LAMP_OFF       2
#define LOGIN_SUCCESS 1
#define LOGIN_FAILED  2

#define COMMAND_END 30
#define COMMAND_SEPERATOR 31

void * user_handler(void * arg);//deal with ever user
void service(char * rev_msg, int fd, int rn);//
void authentication(char * account, char * password, int fd);

void * server_manager(void * arg);
void menu();
void display_online();

#endif // _H_IHOMESERVER
