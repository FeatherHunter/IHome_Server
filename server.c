#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sqlite3.h>
#include "socket_server.h"
#include "server.h"
#include "idebug.h"
//int user_fd = -1;
//int home_fd = -1;

//pthread_mutex_t auth_mutex;
#define SQLMAX 128
#define INFOFILEPATH "users"
#define RANK_ACCOUNT 0
#define RANK_USERFD     1
#define RANK_HOMEFD   2
int main()
{
    int socket_fd;
    int userfd;
    pthread_t pid;
    pthread_t manager_pid;

    struct sockaddr_in user_addr;
    ///*init mutex for auth*/
    //pthread_mutex_init(&auth_mutex, 0);
    /*socket bind listen*/
    socket_fd = init_server(AF_INET, SERVER_PORT, SERVER_ADDR);
    printf("IP: %s PORT: %d\n", SERVER_ADDR, SERVER_PORT);
    if(pthread_create(&manager_pid, NULL, (void *)server_manager, (void *)NULL) != 0)
    {
        fprintf(stderr, "server manager create error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    while(1)
    {
        int user_len = sizeof(user_addr);
        /*wait client's requst*/
        printf("accepting the customer!\n");
        userfd = accept(socket_fd, (struct sockaddr *)&user_addr, &user_len);
        if(userfd == -1)
        {
            fprintf(stderr, "accept error!%s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        printf("accept the request of %s  fd:%d\n", inet_ntoa(user_addr.sin_addr), userfd);

        /*
         *  provide services for every user
         */
        if( pthread_create(&pid, NULL, (void *)user_handler, (void *)&userfd) != 0)
        {
            printf("%s offline", inet_ntoa(user_addr.sin_addr));
            close(userfd);
        }
    }

    return 0;
}
/* @Function: void * server_manager(voidr * arg)
 * @Description:   To help server's manager manage server, such as :
 *                                  1. Looking at overview of the online list,
 *                                  2. Kick out specific user
 *                                  3. Broadcast some informations
 */
void * server_manager(void * arg)
{
    int choose;
    while(1)
    {
        menu();
        scanf("%d", &choose);
        switch(choose)
        {
            case 1: display_online();
                break;
        }
    }
}

void menu()
{
    printf("  ---------------------服务器--------------------- \n");
    printf("|            1. 显示在线用户列表              |\n");
    printf("|            2. 注册新用户信息                  |\n");
    printf("|            3. 踢出用户                             |\n");
    printf("|            4. 广播消息                             |\n");
    printf("  ---------------------------------------------------\n");
    printf("|             请选择(1~4):                          |\n");
    printf("  ---------------------------------------------------\n");
}

void display_online()
{
    int res;
    sqlite3 * userinfo_db;
    sqlite3_stmt * stmt;
    char sql[SQLMAX];
    if(sqlite3_open(INFOFILEPATH, &userinfo_db) != SQLITE_OK)
    {
            fprintf(stderr, "%s %d usersinfo open err:%s", __FILE__, __LINE__, strerror(errno));
            exit(EXIT_FAILURE);
    }
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select account,userfd,homefd from online"); //查询所有数据
    if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "%s %d sqlite3_prepare err:%s", __FILE__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    res = sqlite3_step(stmt);
    printf("************  当前在线用户列表 **********************\n ");
    while(res == SQLITE_ROW) //有数据
    {
        printf("家庭组用户名： %s  Home端 fd: %d  User端 fd: %d\n", sqlite3_column_text(stmt, RANK_ACCOUNT),
                                                    sqlite3_column_int(stmt, RANK_HOMEFD), sqlite3_column_int(stmt, RANK_USERFD));
        res = sqlite3_step(stmt);
    }
    sqlite3_close(userinfo_db);
}

/*
 * funtion: service every user
 *
 */
void * user_handler(void * arg)
{
    sqlite3 * userinfo_db;
    sqlite3_stmt * stmt;
    char buff[MESIZE];
    char sql[SQLMAX];
    int fd = *((int *)arg);
    int rn;
    int i;
    int res;
//    int userhome_flag = -1; //0:home,1:user

    while(1)
    {
        memset(buff, 0, sizeof(buff));
        rn = read(fd, buff, MESIZE);
#if _DEBUG
        printf("\n---------- rev msg --------------\n");
        for(i = 0; i < rn; i++)
        {
            if((buff[i] >= '0' && buff[i] <= '9')||(buff[i] >= 'a' && buff[i] <= 'z') )
                DEBUG("%c ", buff[i]);
            else
                DEBUG("%d ", buff[i]);
        }
        printf("\n----------------------------------\n");
#endif // DEBUG
        if(rn == -1)
        {
            if(sqlite3_open(INFOFILEPATH, &userinfo_db) != SQLITE_OK)
            {
                fprintf(stderr, "%s %d usersinfo open err:%s", __FILE__, __LINE__, strerror(errno));
                exit(EXIT_FAILURE);
            }
            memset(sql, 0, sizeof(sql));
            sprintf(sql, "select account,userfd,homefd from online where homefd='%d' OR userfd='%d'", fd, fd); //取出userfd和account
            if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
            {
                fprintf(stderr, "%s %d sqlite3_prepare err:%s", __FILE__, __LINE__, strerror(errno));
                exit(EXIT_FAILURE);
            }
            res = sqlite3_step(stmt);
            if(res == SQLITE_ROW) //是在线用户的一员
            {
                printf("%s ", sqlite3_column_text(stmt, RANK_ACCOUNT)); //查询结果中第RANK_ACCOUNT就是用户名
                if(sqlite3_column_int(stmt, RANK_USERFD) == fd) //表示是用户下线
                {
                    printf("user(fd=%d)  offline\n", fd);
                    if(sqlite3_column_int(stmt, RANK_HOMEFD) == -1) //如果homefd=-1表示其下线，user也下线则在online list中注销整个用户组
                    {
                        memset(sql, 0, sizeof(sql));
                        sprintf(sql, "delete from online where account='%s'", sqlite3_column_text(stmt, RANK_ACCOUNT)); //删除整个用户组
                        if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
                        {
                            fprintf(stderr, "%s %d sqlite3_prepare err:%s", __FILE__, __LINE__, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                        res = sqlite3_step(stmt);
                        if(res!= SQLITE_DONE)                 //返回SQLITE_DONE 表示执行成功
                        {
                            fprintf(stderr, "%s %d step err:%s", __FILE__, __LINE__, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                    }
                    else //仅仅是用户注销
                    {
                        memset(sql, 0, sizeof(sql));
                        sprintf(sql, "update online set userfd=-1, where account='%s'", sqlite3_column_text(stmt, RANK_ACCOUNT)); //注销用户端,
                        if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
                        {
                            fprintf(stderr, "%s %d sqlite3_prepare err:%s", __FILE__, __LINE__, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                        res = sqlite3_step(stmt);
                        if(res != SQLITE_DONE)                 //返回SQLITE_DONE 表示执行成功
                        {
                            fprintf(stderr, "%s %d step err:%s", __FILE__, __LINE__, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                    }
                }
                else
                {
                    printf("home(fd=%d)  offline\n", fd);
                    if(sqlite3_column_int(stmt, RANK_USERFD) == -1) //如果userfd=-1表示其下线，homefd也下线,则在online list中注销整个用户组
                    {
                        memset(sql, 0, sizeof(sql));
                        sprintf(sql, "delete from online where account='%s'", sqlite3_column_text(stmt, RANK_ACCOUNT)); //删除整个用户组
                        if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
                        {
                            fprintf(stderr, "%s %d sqlite3_prepare err:%s", __FILE__, __LINE__, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                        res = sqlite3_step(stmt);
                        if(res != SQLITE_DONE)                 //返回SQLITE_DONE 表示执行成功
                        {
                            fprintf(stderr, "%s %d step err:%s", __FILE__, __LINE__, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                    }
                    else //仅仅是home注销
                    {
                        memset(sql, 0, sizeof(sql));
                        sprintf(sql, "update online set homefd=-1, where account='%s'", sqlite3_column_text(stmt, RANK_ACCOUNT)); //注销home端,
                        if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
                        {
                            fprintf(stderr, "%s %d sqlite3_prepare err:%s", __FILE__, __LINE__, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                        res = sqlite3_step(stmt);
                        if(res != SQLITE_DONE)                 //返回SQLITE_DONE 表示执行成功
                        {
                            fprintf(stderr, "%s %d step err:%s", __FILE__, __LINE__, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                    }//end of RANK_USERFD
                }

            }
            else //莫名其妙的用户
            {
                printf("no online list's user offline, fd = %d\n", fd);
            }
            sqlite3_close(userinfo_db); //关闭数据库
            break;
/*
            if(userhome_flag == 0)
            {
		        printf("home offline\n");
                home_fd = -1;
            }
            else if(userhome_flag == 1)
            {
		        printf("user offline\n");
                user_fd = -1;
            }
	        else
	        {
		         printf("%d offline\n", userhome_flag);
	        }
            break;
*/
        }
        else if(rn > 0)
        {
            service(buff, fd, rn);
        }
    }
    close(fd);
}

void service(char * rev_msg, int fd, int rn)
{
    int type;
    int subtype;
    int home_fd;
    int user_fd;

    sqlite3 * userinfo_db;
    sqlite3_stmt * stmt;
    char sql[SQLMAX];

    char account[ACCOUNT_MAX + 1]; //account's num max = 32
    char password[ACCOUNT_MAX + 1];
    char destination[37]; //des's num max = 36
    char content[MESIZE];

    char tempbuf[MESIZE];

    int i;
    int j;
    int cmd_start;
    int res;
    int rev_len = strlen(rev_msg);
    if(sqlite3_open(INFOFILEPATH, &userinfo_db) != SQLITE_OK)
    {
        fprintf(stderr, "%s %d usersinfo open err:%s", __FILE__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select account,userfd,homefd from online where homefd='%d' OR userfd='%d'", fd, fd); //取出userfd和account
    if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "%s %d sqlite3_prepare err:%s", __FILE__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    res = sqlite3_step(stmt);
    if(res != SQLITE_ROW) //查询失败
    {
        fprintf(stderr, "sevice 没有找到online中有该在线用户");
        return; //结束处理
    }
    user_fd = sqlite3_column_int(stmt, RANK_USERFD);
    home_fd = sqlite3_column_int(stmt, RANK_HOMEFD);
    if(user_fd == fd)
    {
        if(home_fd != -1)//home在线
        {
            res = write(home_fd, rev_msg, rn);
            if(res <= 0)
            {
                fprintf(stderr, "write result error! %s %d", __FILE__, __LINE__);
            }
            else
            {
                printf("user: %d has sent to home: %d\n", user_fd, home_fd);
            }
        }
        else//home offline
        {
            printf("user: %d  home offline\n", user_fd);
        }
    }
    else if(home_fd == fd)
    {
        if(user_fd != -1)//home在线
        {
            res = write(user_fd, rev_msg, rn);
            if(res <= 0)
            {
                fprintf(stderr, "write result error! %s %d", __FILE__, __LINE__);
            }
            else
            {
                printf("home: %d has sent to user: %d\n", home_fd ,user_fd);
            }
        }
        else//home offline
        {
            printf("home: %d  user offline\n", home_fd);
        }
    }
    i = 0;
    while((rev_msg[i] != '\0')&&(i < rev_len))
    {
        cmd_start = i;
        /*get type*/
        if(rev_msg[i + 1] == COMMAND_SEPERATOR)
        {
            type = rev_msg[i];
            i+=2;
#if _DEBUG
            switch(type)
            {
                case COMMAND_MANAGE: printf("manage\n");break;
                case COMMAND_CONTRL: printf("contrl\n");break;
                case COMMAND_RESULT: printf("result\n");break;
                default: printf("unknown type:%d\n", type);
            }
#endif
            /*为心跳*/
            if(type == COMMAND_PULSE)
            {
                sprintf(tempbuf, "%c%c%c",COMMAND_PULSE,COMMAND_SEPERATOR, COMMAND_END);
                res = write(fd, tempbuf, strlen(tempbuf));
                if(res <= 0)
                {
                    fprintf(stderr, "write result error! %s %d", __FILE__, __LINE__);
                    exit(EXIT_FAILURE);
                }
                while((i < rev_len) && (rev_msg[i] != COMMAND_END) )//msg[i]=END
                {
                    i++;
                }
                i++;
                continue;
            }
        }
        else
        {
            /*this cmd is invalid, to find next cmd*/
            while((i < rev_len) && (rev_msg[i] != COMMAND_END) )//msg[i]=END
            {
                i++;
            }
            i++;
            continue;
        }
        /*get account*/
        for(j = 0; (rev_msg[i] != COMMAND_SEPERATOR) && (rev_msg[i] != '\0') && j <= ACCOUNT_MAX; i++, j++)
        {
            account[j] = rev_msg[i];
        }
        DEBUG("account:%s\n", account);
        i++;
        account[j] = '\0';
        /*get type*/
        if(rev_msg[i + 1] == COMMAND_SEPERATOR)
        {
            subtype = rev_msg[i];
            i+=2;
#if _DEBUG
            switch(subtype)
            {
                case MAN_LOGIN: printf("MAN_login\n");break;
                case CTL_LAMP: printf("CTL_lamp\n");break;
                case CTL_GET: printf("CTL_get\n");break;
                case RES_LOGIN: printf("RES_login\n");break;
                case RES_LAMP: printf("RES_lamp\n");break;
                case RES_TEMP: printf("RES_temp\n");break;
                case RES_HUMI: printf("RES_humi\n");break;
                default: printf("unknown subtype:%d\n", type);
            }
#endif
        }
        else
        {
            /*this cmd is invalid, to find next cmd*/
            while((i < rev_len) && (rev_msg[i] != COMMAND_END) )//msg[i]=END
            {
                i++;
            }
            i++;
            continue;
        }
        if(type == COMMAND_MANAGE)
        {
            DEBUG("arrive COMMAND_manage\n");
            if(subtype == MAN_LOGIN)
            {
                DEBUG("arrive MAN_login\n");
                /*get password*/
                for(j = 0; (rev_msg[i] != COMMAND_SEPERATOR) && (rev_msg[i] != '\0') && j <= ACCOUNT_MAX; i++, j++)
                {
                    password[j] = rev_msg[i];
                }
                i++;
                password[j] = '\0';
                /*authentication*/
                authentication(account, password, fd);
            }
        }
	else
	{
	    /*this cmd is invalid, to find next cmd*/
            while((i < rev_len) && (rev_msg[i] != COMMAND_END) )//msg[i]=END
            {
                i++;
            }
            i++;

	}

#if 0
        else if(type == COMMAND_CONTRL)
        {
            DEBUG("arrive COMMAND_contrl\n");
            /*it's user*/
            if(account[j-1] != 'h')
            {
                if(home_fd < 0)
                {
                    DEBUG("not found home!\n");
                }
                else
                {
                    /*this cmd is invalid, to find next cmd*/
                    j = 0;
                    while((cmd_start < rev_len) && (rev_msg[cmd_start] != COMMAND_END) )//msg[i]=END
                    {
                        tempbuf[j++] = rev_msg[cmd_start++];
                        i++;
                    }
                    i++;
                    tempbuf[j] = '\0';
                    /*home fd*/
                    res = write(home_fd, tempbuf, strlen(tempbuf));
                    if(res <= 0)
                    {
                        fprintf(stderr, "write result error! %s %d", __FILE__, __LINE__);
                        exit(EXIT_FAILURE);
                    }
                    DEBUG("has sent to home\n");
                }
            }//end of account
        }
        else if(type == COMMAND_RESULT)
        {
            DEBUG("arrive COMMAND_result\n");
            /*it's home*/
            if(account[j-1]='h')
            {
                if(user_fd < 0)
                {
                    printf("not found user!\n");
                }
                else
                {
                    /*this cmd is invalid, to find next cmd*/
                    j = 0;
                    while((cmd_start < rev_len) && (rev_msg[cmd_start] != COMMAND_END) )//msg[i]=END
                    {
                        tempbuf[j++] = rev_msg[cmd_start++];
                        i++;
                    }
                    i++;
                    tempbuf[j] = '\0';
                    /*user fd*/
                    res = write(user_fd, tempbuf, strlen(tempbuf));
                    if(res <= 0)
                    {
                        fprintf(stderr, "write result error! %s %d", __FILE__, __LINE__);
                        exit(EXIT_FAILURE);
                    }
                    DEBUG("has sent to user\n");
                }
            }

        }//end of command_result
#endif
    }
}
void authentication(char * account, char * password, int fd)
{
    int is_home = 0;
    int res;
    int len;
    char tempbuf[MESIZE];
    char real_account[ACCOUNT_MAX + 1];
    char sql[MESIZE];
    sqlite3 * userinfo_db;
    sqlite3_stmt * stmt;
    len = strlen(account);
    if(account[len - 1] == 'h')
    {
        is_home = 1;
        strncpy(real_account, account, len - 1);
        real_account[len - 1] = '\0';
    }
    else
    {
        is_home = 0;
        strncpy(real_account, account, len);
        real_account[len] = '\0';
    }

    DEBUG("AUTHENTICATION account:%s pssword:%s\n",account, password);
     if( sqlite3_open(INFOFILEPATH, &userinfo_db) != SQLITE_OK)
    {
            fprintf(stderr, "%s %d usersinfo open err:%s", __FILE__, __LINE__, strerror(errno));
            exit(EXIT_FAILURE);
    }
    sprintf(sql, "select password from account where account='%s'", real_account);
    if( sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
            fprintf(stderr, "%s %d auth prepare err:%s", __FILE__, __LINE__, strerror(errno));
            exit(EXIT_FAILURE);
    }
    res = sqlite3_step(stmt);
    if(res == SQLITE_ROW) // account success
    {
        if(is_home == 1) //is control center
        {
            if(strcmp(password, real_account) == 0) //real_account == password
            {
                /*确定在线列表中是否有该家庭*/
                memset(sql, 0, sizeof(sql)); //清空缓冲区
                sprintf(sql, "select  account from online where account='%s'", real_account);
                if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
                {
                    fprintf(stderr, "%s %d check whether online table has account prepare err:%s\n", __FILE__, __LINE__, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                res = sqlite3_step(stmt);
                if(res != SQLITE_ROW) //在线列表没有该用户，因此在在线列表新建用户
                {
                     memset(sql, 0, sizeof(sql)); //清空缓冲区
                     sprintf(sql, "insert into online(account, userfd, homefd) values('%s', -1, %d)", real_account, fd); //插入account
                     if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
                     {
                         fprintf(stderr, "%s %d insert new account prepare err:%s\n", __FILE__, __LINE__, strerror(errno));
                         exit(EXIT_FAILURE);
                     }
                     res = sqlite3_step(stmt);
                     if(res != SQLITE_DONE)
                     {
                        fprintf(stderr, "%s %d insert new account err:%s\n", __FILE__, __LINE__, strerror(errno));
                         exit(EXIT_FAILURE);
                     }
                }
                else //该家庭组在线，直接更新homefd即可
                {
                    /*更新家庭fd*/
                    memset(sql, 0, sizeof(sql)); //清空缓冲区
                    sprintf(sql, "update online set homefd=%d,where account='%s'", fd, real_account); //更新home_fd
                    if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
                    {
                        fprintf(stderr, "%s %d insert home_fd prepare err:%s\n", __FILE__, __LINE__, strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    res = sqlite3_step(stmt);
                    if(res != SQLITE_DONE)
                    {
                        fprintf(stderr, "%s %d insert home_fd err:%s\n", __FILE__, __LINE__, strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                // home_fd = fd; //记录家庭fd
                // *userhome_flag = 0;
                sprintf(tempbuf, "%c%cSERVER%c%c%c%c%c%c",COMMAND_RESULT,COMMAND_SEPERATOR,COMMAND_SEPERATOR,
                                RES_LOGIN,COMMAND_SEPERATOR,
                                        LOGIN_SUCCESS,COMMAND_SEPERATOR, COMMAND_END);
                res = write(fd, tempbuf, strlen(tempbuf));
               if(res <= 0)
               {
                    fprintf(stderr, "write result error! %s %d", __FILE__, __LINE__);
                    exit(EXIT_FAILURE);
               }
               DEBUG("%s home login success!\n", account);
            }
            else//验证失败
            {
                sprintf(tempbuf, "%c%cSERVER%c%c%c%c%c%c",COMMAND_RESULT,COMMAND_SEPERATOR,COMMAND_SEPERATOR,RES_LOGIN,COMMAND_SEPERATOR, LOGIN_FAILED,COMMAND_SEPERATOR, COMMAND_END);
                res = write(fd, tempbuf, strlen(tempbuf));
                if(res <= 0)
                {
                    fprintf(stderr, "write result error! %s %d", __FILE__, __LINE__);
                    exit(EXIT_FAILURE);
                }
                DEBUG("%s home login failed!\n", account);
            }
        }
        else//is user
        {
            if(strcmp(password, sqlite3_column_text(stmt, 0)) == 0)//compare password success
            {
                /*确定在线列表中是否有该家庭用户组*/
                memset(sql, 0, sizeof(sql)); //清空缓冲区
                sprintf(sql, "select  account from online where account='%s'", real_account);
                if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
                {
                    fprintf(stderr, "%s %d check whether online table has account prepare err:%s\n", __FILE__, __LINE__, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                res = sqlite3_step(stmt);
                if(res != SQLITE_ROW) //在线列表没有该用户，因此在在线列表新建用户,并且加入userfd
                {
                     memset(sql, 0, sizeof(sql)); //清空缓冲区
                     //sprintf(sql, "insert into online(account) values('%s')", real_account); //插入account
                     sprintf(sql, "insert into online(account, userfd, homefd) values('%s', %d, -1)", real_account, fd); //插入account
                     if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
                     {
                         fprintf(stderr, "%s %d insert new account prepare err:%s\n", __FILE__, __LINE__, strerror(errno));
                         exit(EXIT_FAILURE);
                     }
                     res = sqlite3_step(stmt);
                     if(res != SQLITE_DONE)
                     {
                        fprintf(stderr, "%s %d insert new account err:%s\n", __FILE__, __LINE__, strerror(errno));
                         exit(EXIT_FAILURE);
                     }
                }
                else //如果家庭组在在线列表中，直接更新userfd
                {
                    /*update 用户fd*/
                    memset(sql, 0, sizeof(sql)); //清空缓冲区
                    //sprintf(sql, "insert into online(homefd) values('%d') where account='%s'", fd, real_account); //插入home_fd
                    sprintf(sql, "update online set  userfd=%d,where account='%s'", fd, real_account); //更新user_fd
                    if(sqlite3_prepare(userinfo_db, sql, -1, &stmt, NULL) != SQLITE_OK)
                    {
                        fprintf(stderr, "%s %d insert home_fd prepare err:%s\n", __FILE__, __LINE__, strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    res = sqlite3_step(stmt);
                    if(res != SQLITE_DONE)
                    {
                        fprintf(stderr, "%s %d insert home_fd err:%s\n", __FILE__, __LINE__, strerror(errno));
                        exit(EXIT_FAILURE);
                    }

                }

 //               user_fd = fd; //记录用户fd
 //               *userhome_flag = 1;
                sprintf(tempbuf, "%c%cSERVER%c%c%c%c%c%c",COMMAND_RESULT,COMMAND_SEPERATOR,COMMAND_SEPERATOR,
                                                                                                        RES_LOGIN,COMMAND_SEPERATOR,
                                                                                                                    LOGIN_SUCCESS,COMMAND_SEPERATOR, COMMAND_END);
                res = write(fd, tempbuf, strlen(tempbuf));
                if(res <= 0)
                {
                    fprintf(stderr, "write result error! %s %d", __FILE__, __LINE__);
                    exit(EXIT_FAILURE);
                }
                DEBUG("%s user login success!\n", account);
            }
            else//failed
            {
                sprintf(tempbuf, "%c%cSERVER%c%c%c%c%c%c",COMMAND_RESULT,COMMAND_SEPERATOR,COMMAND_SEPERATOR,RES_LOGIN,COMMAND_SEPERATOR, LOGIN_FAILED,COMMAND_SEPERATOR, COMMAND_END);
                res = write(fd, tempbuf, strlen(tempbuf));
                if(res <= 0)
                {
                    fprintf(stderr, "write result error! %s %d", __FILE__, __LINE__);
                    exit(EXIT_FAILURE);
                }
                DEBUG("%s user login falied!\n", account);
            }
        }
    }
    else//account error
    {
        sprintf(tempbuf, "%c%cSERVER%c%c%c%c%c%c",COMMAND_RESULT,COMMAND_SEPERATOR,COMMAND_SEPERATOR,RES_LOGIN,COMMAND_SEPERATOR, LOGIN_FAILED,COMMAND_SEPERATOR, COMMAND_END);
        res = write(fd, tempbuf, strlen(tempbuf));
        if(res <= 0)
        {
            fprintf(stderr, "write result error! %s %d", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
        DEBUG("%s login falied!\n", account);
    }
    sqlite3_close(userinfo_db); //关闭数据库文件
}
