/*************************************************************************
> File Name: client.c
	> Author: zhuxinquan
	> Mail: zhuxinquan61@gmail.com
	> Created Time: 2015年08月08日 星期六 17时15分06秒
 ************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include "inputkey_getch.h"
#include <time.h>

#define BUFSIZE 1024 

void my_err(const char *, int);         //错误处理函数
void print_menu(void);                  //打印主界面菜单函数
void quit(void);                        //退出函数
int register_num();                     //注册函数
int login();                            //登陆函数
void print_login_menu();                //登陆后菜单
void friend_management();               //好友管理菜单
void message_management();              //消息管理
void del_friend();                      //删除好友
void before_login_quit();               //登陆之后的退出函数
void add_friend();                      //添加好友函数
void show_all_friend();                 //查看所有好友函数
void show_online_friend();              //查看在线好友
void sign_pthread();                    //处理消息线程函数
void private_chat();                    //私聊函数
void public_chat();                     //群聊函数


struct info                             //个人信息
{
    char    username[10];
};


struct log                              //用户登陆注册、退出时使用
{
    char    flag;
    char    name[10];
    char    pwd[10];
};

struct users                            //用户链表结构体
{
    char            password[10];
    struct info     user;
    struct info     friends[10];        //好友
    struct users*   next;
};

/*在客户端只有一个用户在使用
 *所以可以将所有的用户的好友
 *数、群组数等设置为全局变量
 */
int                 conn_fd;
int                 friend_num;
char                password[10];
struct info         user;
struct info         friend[10];
pthread_mutex_t     mutex;

struct chat                             //聊天消息结构体
{
    int     flag;
    char    from[10];
    char    to[10];
    char    time[30];
    char    news[500];
};

int             message_num  = 0;               //消息数量
struct chat     message[100];                   //消息队列


void my_err(const char * err_string, int line)                      //错误处理函数
{
    fprintf(stderr, "line:%d,", line);
    perror(err_string);
    exit(1);
}

void cls()                                          //自定义清楚缓存函数
{
    char c;
    do{
        c = getch1();
    }while(c != 10 && c != EOF);
}

int main(int argc, char ** argv)                    //主函数
{
    int                  i;
    int                  ret;
    int                  serv_port;
    struct sockaddr_in   serv_addr;
    pthread_t            thid;
    if(argc != 5)                                   //检查参数个数
    {
        printf("usage : [-p] [serv_port] [-a] [serv_address]\n");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));        //初始化服务器端地址结构
    serv_addr.sin_family = AF_INET;
    for(i = 1; i < argc; i++)                                   //从命令行获取服务器的端口与地址
    {
        if(strcmp("-p", argv[i]) == 0)
        {
            serv_port = atoi(argv[i+1]);
            if(serv_port < 0 || serv_port > 65535)
            {
                printf("invalid serv_addr.sin_port\n");
                exit(1);
            }
            else 
            {
                serv_addr.sin_port = htons(serv_port);
            }
            continue;
        }
        if(strcmp("-a", argv[i]) == 0)
        {
            if(inet_aton(argv[i+1], &serv_addr.sin_addr) == 0)
            {
                printf("invalid server ip address\n");
                exit(1);
            }
            continue;
        }
    }
    
    if(serv_addr.sin_port == 0 || serv_addr.sin_addr.s_addr == 0)           //检测是否少输入了某项参数
    {
        printf("usage: [-p] [serv_addr.sin_port] [-a] [serv_address]\n");
        exit(1);
    }

    conn_fd = socket(AF_INET, SOCK_STREAM, 0);                              //创建一个tcp套接字
    if(conn_fd < 0){
        my_err("socket", __LINE__);
    }

    if(connect(conn_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0){    //向服务器端发送连接请求
        my_err("connect", __LINE__);
    }

    print_menu();                                                       //打印主菜单
    pthread_create(&thid, NULL, (void *)sign_pthread, NULL);            //创建线程用来接受并解析所有消息
    print_login_menu();                                                 //显示登陆之后的菜单    
    printf("任意键退出！\n");
    getchar();

}


/*
 *该线程负责在登陆成功之后接收所有来自
 *服务器的消息，所有消息都为chat结构体
 */
void sign_pthread()
{
    char        sign_buf[1024];
    struct chat chat;
    int         ret;
    FILE        *fp;

    while(1)
    {
        memset(sign_buf, 0, 1024);
        memset(&chat, 0, sizeof(struct chat)); 
        ret = recv(conn_fd, sign_buf, 1024, 0);
        if(ret != 1024)
        {
            perror("recv error");
            exit(1);
        }
        memcpy(&chat, sign_buf, 1024);                                  //将接收到的消息转换到chat消息结构体当中
        switch(chat.flag)
        {
            //case '0':
                //printf("消息已离线！\n");
                //break;
            case 'a':                                                   //添加好友请求的返回消息
                if(strcmp(chat.news, "n") == 0)
                {
                    printf("\n添加失败\n");
                }
                else{
                    printf("\n好友添加成功\n");
                }
                break;
            case 'l':                                                   //请求查看所有好友的返回消息
                printf("\n\n\t所有好友：%s\n", chat.news);
                break;
            case 'o':                                                   //请求查看在线好友的返回消息
                printf("\n\n\t在线好友：%s\n", chat.news);
	            break;
            case 'd':                                                   //请求删除好友的返回消息
                printf("\n\n\t删除成功\n");
                break;
            case 's':                                                   //接收到私聊消息
                printf("\n好友%s   %s:\n%s\n", chat.from, chat.time, chat.news);
                pthread_mutex_lock(&mutex);
                fp = fopen(user.username, "at+");
                if(fp == NULL)
                {
                    printf("cann't open file");
                    exit(1);
                }
                fwrite(&chat, sizeof(struct chat), 1, fp);
                fclose(fp);
                pthread_mutex_unlock(&mutex);
                break;
            case 'p':                                                   //接收到群聊消息
                printf("\n群聊  好友:%s  %s\n%s\n", chat.from, chat.time, chat.news);
                pthread_mutex_lock(&mutex);
                fp = fopen(user.username, "at+");
                if(fp == NULL)
                {
                    printf("cann't open file");
                    exit(1);
                }
                fwrite(&chat, sizeof(struct chat), 1, fp);
                fclose(fp);
                pthread_mutex_unlock(&mutex);
                break;
            case 'x':                                                   //接受到离线消息
                printf("\n离线  好友：%s  %s\n%s\n", chat.from, chat.time, chat.news);
                break;
            case 'e':                                                   //接受到服务器退出的消息
                printf("\n\n--------服务器已退出-------\n\n\n");
                exit(0);
        }
        
    }
}

void print_menu(void)                                       //打印注册登陆菜单      
{
    int num;
    int   ret;
    char  c[10];                                            //接收选项
    while(1)
    {
        system("clear");
        printf("\n");
        printf("\n\n\n");
        printf("\t\t            1.注册\n\n");
        printf("\t\t            2.登陆\n\n");
        printf("\t\t            3.退出\n\n");
        printf("\t\t----------------------------\n\n");
        printf("\t\t         请选择<1 ~ 3>：");
        setbuf(stdin, NULL);
        scanf("%s", c);
        if((strcmp(c, "1") != 0)  &&(strcmp(c, "2") != 0) && (strcmp(c, "3") != 0)){
            continue;
        }
        num = atoi(c);
        if(num == 1)
        {
            if(register_num())                              //注册返回值1,注册成功
            {
                quit();
                break;
            }
        }
        else if(num == 2)
        {
            ret = login();                                  //登陆函数
            if(ret == 1)
            {
                break;  
            }
        }
        else if(num == 3)
        {
            quit();
        }
        else{
            continue;
        }
    }
}


void quit()                                                 //客户端退出
{
    struct log log;
    int ret;
    char quitbuf[1024];                                     //接受退出时服务器的返回结果
    memset(&log, 0, sizeof(struct log));
    log.flag = 'q';
    memcpy(quitbuf, &log, sizeof(struct log));
    send(conn_fd, quitbuf, 1024, 0);
    exit(0);
}

int register_num()                                          //用户注册函数
{
    int         i;
    char        register_buf[1024] = "0";
    struct info user_temp;                                  //暂存当前申请的用户信息
    char        key[10], key1[10];
    struct log  apply;

    system("clear");
    memset(&apply, 0, sizeof(apply));
    printf("\n\t请输入用户名：");
    scanf("%s", apply.name);
    while(1)
    {
        printf("\n\t请输入密码：");
        inputkey(key);                                      //调用密文密码输入函数
        printf("\n\t请再次输入密码：");
        inputkey(key1);
        if(strcmp(key, key1) != 0)                          //对两次输入的密码进行比较
        {
            printf("两次输入密码不相同，按ENTER键返回重新输入！\n");
            getchar();
        }
        else 
        {
            strcpy(apply.pwd, key);
            break;
        }
    }

    apply.flag = 'a';
    memcpy(register_buf, &apply, sizeof(apply));
    send(conn_fd, register_buf, 1024, 0);

    memset(register_buf, 0, sizeof(register_buf));
    recv(conn_fd, register_buf, 1024, 0);
    memcpy(&apply, register_buf, 1024);
    if(apply.flag == 'n')
    {
        printf("\n 用户名已被使用!\n");
        getchar();
        return (0);
    }
    memcpy(&user_temp, register_buf, sizeof(register_buf));
    printf("\n申请成功用户名:%s\n", user_temp.username);
    strcpy(password, apply.pwd);
    user = user_temp;
    return 1;
}

int login()                                                 //登陆函数                           
{
    struct log log;
    char       login_buf[1024];
    int        ret;

    memset(&log, 0, sizeof(struct log));

    while(1)                                                //while循环确保输入的用户名不为空
    {
        system("clear");
        printf("\n\t请输入用户名：");
        scanf("%s", log.name);
        if(strcmp(log.name, "") != 0)
        {
            break;
        }
    }
    while(1)
    {
        printf("\n\t请输入密码：");
        inputkey(log.pwd);
        if(strcmp(log.pwd, "") != 0)
        {
            break;
        }
    }
    log.flag = 'l';
    memcpy(login_buf, &log, sizeof(struct log));
    ret = send(conn_fd, login_buf, 1024, 0);

    if(ret < 0)
    {
        perror("send error");
        exit(0);
    }
    memset(login_buf, 0, 1024);
    ret = recv(conn_fd, login_buf, 1024, 0);
    if(ret < 0)
    {
        perror("recv");
        exit(0);
    }
    if(strcmp(login_buf, log.name) == 0)
    {
        strcpy(user.username, log.name);
        printf("\n\t登陆成功！\n");
        return 1;
    }
    else
    {
        printf("\n\t登陆失败！\n");
        return 0;
    }

}

void print_login_menu()                                     //打印登陆成功之后的菜单                                     
{
    struct chat chat;
    while(1)
    {
        int             ret;
        struct chat     login_user;
        char            login_user_buf[1024];
        int             select;
        char            c[10];

        printf("\n");
        printf("\t---------------------\n");
        printf("\t       1.好友管理\n");
        printf("\t       2.消息管理\n");
        printf("\t       0.退出\n");
        printf("\t---------------------\n");
        printf("\t       请选择：");
        setbuf(stdin, NULL);
        scanf("%s", c);
        if((strcmp(c, "1") != 0) &&  (strcmp(c, "2") != 0) && (strcmp(c, "0") != 0)){
            continue;
        }
        select = atoi(c);
        switch (select)
        {
            case 1:
                friend_management();                    //跳转到好友管理菜单                 
                break;
            case 2:
                message_management();                   //跳转到消息管理菜单
                break;
            case 0:
                memset(&chat, 0, sizeof(struct chat));
                chat.flag = 'q';
                memcpy(login_user_buf, &chat, sizeof(struct chat));
                send(conn_fd, login_user_buf, 1024, 0);
                exit(0);
        }
    }
}

void friend_management()                                //好友管理菜单
{
    int             select;
    char            c[10];
    while(1)
    {
        //system("clear");
        printf("\t----------user: %s-----------------\n", user.username);
        printf("\t         1.添加好友\n");
        printf("\t         2.查看我的好友\n");
        printf("\t         3.查看在线好友\n");
        printf("\t         4.删除好友\n");
        printf("\t         0.返回上级菜单\n");
        printf("\t---------------------------------\n");
        printf("\t         请选择：");
        setbuf(stdin, NULL);
        scanf("%s", c);
        select = atoi(c);
        switch(select)
        {
            case 0:
                return ;
            case 1:
                add_friend();
                break;
            case 2:
                show_all_friend();
                break;
            case 3:
                show_online_friend();
                break;
            case 4:
                del_friend();
                break;
            default :
                break;
        }
    }
}

void add_friend()                                       //添加好友函数
{
    struct chat chat;
    char        add_buf[1024];
    int         ret;

    memset(&chat, 0, sizeof(struct chat));
    chat.flag = 'a';
    strcpy(chat.from, user.username);
    printf("\n\t请输入要添加的好友名：");
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    scanf("%s", chat.news);
    memcpy(add_buf, &chat, sizeof(struct chat));
    ret = send(conn_fd, add_buf, 1024, 0);
    if(ret != 1024)
    {
        perror("send error");
        exit(1);
    }
    return ;
}

void message_management()                                       //消息管理菜单
{
    FILE    *fp;
    struct chat     chat;
    char            c[10];
    int             select;

    while(1)
    {
        printf("\n\t-------------user:%s----------\n", user.username);
        printf("\t        1.发送私聊消息\n");
        printf("\t        2.发送群聊消息\n");
        printf("\t        3.查看聊天记录\n");
        printf("\t        0.返回上级菜单\n");
        printf("\t------------------------------\n");
        printf("\t        请选择：");
        scanf("%s", c);
        select = atoi(c);
        switch(select)
        {
            case 1:
                private_chat();
                break;
            case 2:
                public_chat();
                break;
            case 3:
                pthread_mutex_lock(&mutex);
                fp = fopen(user.username, "rt");
                if(fp == NULL)
                {
                    printf("\n\t无消息记录\n");
                    break;
                }
                printf("\n\t消息记录：\n");
                while(((fread(&chat, sizeof(struct chat), 1 ,fp))!= -1) && !feof(fp))
                {
                    printf("\nfrom:%s  time:%s  news:%s\n", chat.from, chat.time, chat.news);
                    memset(&chat, 0, sizeof(struct chat));
                }
                pthread_mutex_unlock(&mutex);
                break;
            case 0:
                return ;
            default :
                break;
        }
    }
}


void show_all_friend()                              //发送请求查看所有的好友
{
    char            buf[1024];
    struct chat     chat;
    memset(&chat, 0, sizeof(struct chat));
    chat.flag = 'l';
    strcpy(chat.from, user.username);
    memcpy(buf, &chat, sizeof(struct chat));
    send(conn_fd, buf, 1024, 0);
}

void show_online_friend()                           //发送查看在线好友的请求
{
    char            buf[1024];
    struct chat     chat;
    memset(&chat, 0, sizeof(struct chat));
    chat.flag = 'o';
    strcpy(chat.from, user.username);
    memcpy(buf, &chat, sizeof(struct chat));
    send(conn_fd, buf, 1024, 0);
}

void del_friend()                                   //删除好友
{   
    char buf[1024];
    struct chat chat;
    memset(&chat, 0, sizeof(struct chat));
    memset(buf, 0, 1024);
    chat.flag = 'd';
    strcpy(chat.from, user.username);
    printf("请输入要删除的好友：");
    scanf("%s", chat.news);
    memcpy(buf, &chat, sizeof(struct chat));
    send(conn_fd, buf, 1024, 0);
}

void private_chat()                                 //发送私聊消息
{
    struct chat chat;
    char        buf[1024];
    char        name[10];
    time_t      t;
    printf("-------------------------\n");
    printf("请输入接收方用户名：");
    scanf("%s", name);
    printf("\n\n----------输入exit退出---------\n");
    while(1)
    {
        memset(&chat, 0, sizeof(struct chat));
        chat.flag = 's';
        strcpy(chat.from, user.username);
        strcpy(chat.to, name);
        printf("请输入消息：");
        setbuf(stdin, NULL);
        scanf("%s", chat.news);
        time(&t);
        strcpy(chat.time, ctime(&t));
        if(strcmp(chat.news, "exit") == 0){
            return ;
        }
        memcpy(buf, &chat, sizeof(struct chat));
        send(conn_fd, buf, 1024, 0);
    }
}

void public_chat()                                          //发送群聊消息
{
    struct chat chat;
    char        buf[1024];
    char        name[10];
    time_t      t;
    printf("-------------------------\n");
    printf("-----输入exit取消发送----\n");
    while(1)
    {
        memset(&chat, 0, sizeof(struct chat));
        chat.flag = 'p';
        strcpy(chat.from, user.username);
        strcpy(chat.to, name);
        printf("请输入消息：");
        setbuf(stdin, NULL);
        scanf("%s", chat.news);
        time(&t);
        strcpy(chat.time, ctime(&t));
        if(strcmp(chat.news, "exit") == 0){
            return ;
        }
        memcpy(buf, &chat, sizeof(struct chat));
        send(conn_fd, buf, 1024, 0);
    }
}
