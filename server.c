/*************************************************************************
	> File Name: server.c
	> Author: zhuxinquan
	> Mail: zhuxinquan61@gmail.com
	> Created Time: 2015年08月08日 星期六 09时34分39秒
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<pthread.h>

#define SERV_PORT       4507                        //服务器端的端口
#define LISTENQ         20                          //连接请求队列的最大长度
#define BUFSIZE         1024                        //缓冲区大小       

struct conn                                         //每个客户端有一个连接套接字
{
    int     fd;                                     //连接套接字
    char    name[10];                               //连接的客户端用户名
}conn[20];                                          //直接定义最大请求连接队列数个结构体，保证每个客户端使用不同的连接套接字

struct info                                         //个人信息结构体
{
    char username[10];                              //用户名
};


struct users 
{
    char        password[10];                       //用户密码
    int         friends_num;                        //好友数
    struct info user;                               //用户个人信息
    struct info friend[10];                         //好友信息
    struct users * next;
};

struct chat
{
    int         flag;                               //消息标志
    char        from[10];                           //消息来自
    char        to[10];                             //消息发往
    char        time[30];                           //时间
    char        news[500];                          //聊天信息
};

struct log
{
    char        flag;                               //登陆和注册的标志
    char        name[10];
    char        pwd[10];
};

void my_err(const char *, int);                     //错误处理函数
void quit(void *);                                  //服务器退出函数
void * client (void *);                             //单独的处理客户端处理线程
void save();                                        //用户数据保存
struct users * readuser();                          //用户数据读取
struct users * apply(struct log, int);              //申请账号
struct users * login(struct log, int);              //用户登陆


pthread_mutex_t         mutex;                      //锁
struct users *          head = NULL;                //用户数据链表的头指针
struct users *          current_user;               //指向当前用户的指针
FILE                    *fp1;
pthread_mutex_t         mutex_fp;

int main(void)
{
    int                 i;
    int                 sock_fd, conn_fd;           //TCP套接字， 连接套接字
    int                 optval;                     //保存待设定的套接字选项的值
    socklen_t           cli_len;                    //保存struct sockaddr_in 的字节长度
    struct sockaddr_in  cli_addr, serv_addr;        //客户端和服务器端的地址结构
    pthread_t           thid, quit_thid;            //线程ID，quit_thid用来服务器退出

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);      //创建一个TCP套接字
    if(sock_fd < 0)
    {
        my_err("socket", __LINE__);
        exit(1);
    }

    optval = 1;                                     //设置该套接字使之可以重新绑定端口，optval中存有待设定的值
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(int)) < 0)
    {
        my_err("setsockopt", __LINE__);
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));      //初始化服务器端口地址
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port =htons(SERV_PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if(bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)) < 0)    //将套接字绑定到本地端口
    {
        my_err("bind", __LINE__);
        exit(1);
    }

    if(listen(sock_fd, LISTENQ) < 0)                //将套接字转化为监听字
    {
        my_err("listen", __LINE__);
        exit(1);
    }
    
    for(i = 0; i < LISTENQ; i++ )                   //初始化连接套接字
    {
        conn[i].fd = -1;                            //将所有的客户端的套接字初始化为-1,表示未连接
        strcpy(conn[i].name, " ");
    }

    pthread_create(&quit_thid, NULL, (void *)quit, NULL);       //创建一个线程，用来服务器退出
    cli_len = sizeof(struct sockaddr_in);                       //客户端套接字长度
    while(1)
    {
        for(i = 0; i < LISTENQ; i++)                            //得到未使用的一个连接套接字
        {
            if(conn[i].fd == -1){                               //当前连接套接字未使用，找到未使用的则退出循环
                break;
            }
        }
        conn_fd = accept(sock_fd, (struct sockaddr *)&cli_addr, &cli_len);            //通过accept接受客户端连接请求，返回连接套接字
        if(conn_fd < 0)                                                             //出错处理
        {
            my_err("accept", __LINE__); 
            exit(1);
        }
        
        conn[i].fd = conn_fd;                                   //若该连接套接字可用，将其使用赋值给连接套接字队列中未被使用的一个
        printf("accept a new client, IP :%s\n", inet_ntoa(cli_addr.sin_addr));      //新客户端连接，服务器显示客户端连接ip
        fp1 = fopen("sys_log", "at+");
        fprintf(fp1, "accept a new client, IP :%s\n", inet_ntoa(cli_addr.sin_addr));
        fclose(fp1);
        int g = i;
        pthread_create(&thid, NULL, client, (void *)&g);                     //创建一个新的线程处理客户端请求
    }
    return 0;
}

void * client (void * arg)                                      //客户端的线程函数，处理客户端发送的所有请求
{
    int             f = *(int *)arg;
    int             i = f;
    char            flag[1024] ;
    char            recv_buf[1024];
    struct users*   p_user, *p;
    int             ret;
    char 		    filename[20];
    FILE *fp1 = NULL;
    
    while(1)
    {
        struct log  log;
        memset(flag, 0, sizeof(flag));
        
        if((ret = recv(conn[i].fd, flag, 1024, 0)) < 0)
        {
            my_err("apply", __LINE__);
            pthread_exit(0);
        }
        
        pthread_mutex_lock(&mutex);                             //加锁
        memcpy(&log, flag, 1024);
        
        head = readuser();
        if(log.flag == 'a')                                     //注册请求
        {
            p_user = apply(log, i);
            pthread_mutex_unlock(&mutex);                       //解锁
            if(p_user != NULL)
            {
                strcpy(conn[i].name, p_user->user.username);
                break;
            }
        }

        if(log.flag == 'l')                                     //登陆请求
        {
            p_user = login(log, i);
            pthread_mutex_unlock(&mutex);
            if(p_user != NULL)
            {
                break;
            }
        }
        
        if(log.flag == 'q')
        {
            pthread_mutex_unlock(&mutex);
            memset(&log, 0, sizeof(struct log));
            log.flag = 'y';
            memcpy(flag, &log, sizeof(struct log));
            ret = send(conn[i].fd, flag, 1024, 0);
            if(ret < 0)
            {
                perror("send failed");
                pthread_exit(0);
            }
            close(conn[i].fd);
            conn[i].fd = -1;
            pthread_exit(0);
        }
    }
    /*
     * 当登陆或者注册成功之后，先打开用户名.db的文件
     * 若打开成功说明有离线消息存在，然后将消息读取后
     * 发送，当文件读取并发送完毕关闭并删除该文件
     */
    struct chat chat;
    memset(filename, 0, 20);
    strcat(filename, p_user->user.username);
    strcat(filename, ".db");
    fp1 = fopen(filename, "rt");
    if(fp1 != NULL)
    {
        while(((fread(&chat, sizeof(struct chat), 1, fp1)) != -1) && !feof(fp1))
        {
            int i;
            memset(recv_buf, 0, 1024);
            chat.flag = 'x';
            memcpy(recv_buf, &chat, sizeof(struct chat));
            for(i = 0; i < 20; i++)
            {
                if(strcmp(conn[i].name, chat.to) == 0)
                    send(conn[i].fd, recv_buf, 1024, 0);
            }
        }
        fclose(fp1);
        unlink(filename);
    }
    while(1)                                            //登陆或注册成功之后处理客户端的请求
    {
        int flag2 = 0;
        struct chat         chat;
        memset(&chat, 0, sizeof(chat));
        memset(recv_buf, 0, sizeof(recv_buf));
        ret = recv(conn[i].fd, recv_buf, 1024, 0);
        memcpy(&chat, recv_buf, sizeof(recv_buf));
        switch(chat.flag)
        {
            case 'a':                                   //客户端请求添加好友
                pthread_mutex_lock(&mutex);
                head = readuser();
                pthread_mutex_unlock(&mutex);
                for(p = head->next; p != NULL; p = p->next)
                {
                    if(strcmp(p->user.username, chat.news) == 0)
                        break;
                }
                if(p == NULL)
                {
                    memset(&chat, 0, sizeof(struct chat));
                    memset(recv_buf, 0, 1024);
                    chat.flag = 'a';
                    strcpy(chat.news, "n");
                    memcpy(recv_buf, &chat, sizeof(struct chat));
                    send(conn[i].fd, recv_buf, 1024, 0);
                    break;
                }
                for(p = head->next; p != NULL; p = p->next)
                {
                    if(strcmp(chat.from, p->user.username) == 0)                //找到该用户
                    {
                        int i;
                        for(i = 0; i < p->friends_num; i++)
                        {
                            if(strcmp(p->friend[i].username, chat.news) == 0)
                            {
                                flag2 = 1;
                                break;
                            }   
                        }
                        if(flag2 == 1){
                            break;
                        }
                        strcpy((p->friend[p->friends_num]).username, chat.news);
                        p->friends_num++;
                        break;
                    }
                }
                memset(&chat, 0, 1024);
                chat.flag = 'a';
                if(flag2 == 1)
                    strcpy(chat.news, "n");
                else{
                    strcpy(chat.news, "y");
                }
                memset(recv_buf, 0, 1024);
                memcpy(recv_buf, &chat, sizeof(struct chat));
                send(conn[i].fd, recv_buf, 1024, 0);
                pthread_mutex_lock(&mutex);
                save();
                head = readuser();
                pthread_mutex_unlock(&mutex);
                break;
            case 'q':                                                       //客户端退出请求
                memset(&chat, 0, sizeof(struct chat));
                chat.flag = 't';
                memcpy(recv_buf, &chat, sizeof(struct chat));
                send(conn[i].fd, recv_buf, 1024, 0);
                conn[i].fd = -1;
                strcpy(conn[i].name, " ");
                printf("用户%s退出成功\n", (p_user->user).username);
                fp1 = fopen("sys_log", "at+");
                fprintf(fp1, "用户%s退出成功\n", (p_user->user).username);
                fclose(fp1);
                pthread_exit(0);
                break;
            case 'l':                                                       //客户端请求查看所有好友
                pthread_mutex_lock(&mutex);
                head = readuser();
                pthread_mutex_unlock(&mutex);
                p = head->next;
                memset(recv_buf, 0, 1024);
                for(p; p != NULL; p = p->next)
                {
                    if(strcmp(chat.from, (p->user).username) == 0)
                    {
                        int i;
                        for(i = 0; i < p->friends_num; i++)
                        {
                            strcat(chat.news, (p->friend[i].username));
                            strcat(chat.news, ",");
                        }
                        break;
                    }
                }
                memcpy(recv_buf, &chat, sizeof(struct chat));
                send(conn[i].fd, recv_buf, 1024, 0);
                break;
	    case 'o':                                                           //客户端请求查看在线好友
                pthread_mutex_lock(&mutex);
                head = readuser();
                pthread_mutex_unlock(&mutex);
                p = head->next;
                memset(recv_buf, 0, 1024);
                for(p; p != NULL; p = p->next)
                {
                    if(strcmp(chat.from, (p->user).username) == 0)
                    {
                        int i, j;
                        for(i = 0; i < p->friends_num; i++)
                        {
                            for(j = 0; j < 20; j++)                         //判断是否在线
                            {
                                if(strcmp(p->friend[i].username, conn[j].name) == 0)
                                {
                                    printf("conn[i].fd%s\n", conn[i].name);
                                    strcat(chat.news, (p->friend[i].username));
                                    strcat(chat.news, ",");
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
                memcpy(recv_buf, &chat, sizeof(struct chat));
                send(conn[i].fd, recv_buf, 1024, 0);
                break;
            case 'd':                                                       //删除好友请求
                pthread_mutex_lock(&mutex);
                head = readuser();
                pthread_mutex_unlock(&mutex);
                p = head->next;
                for(p; p != NULL; p = p->next)
                {
                    if(strcmp(p->user.username, chat.from) == 0)
                    {
                        int i, j;
                        for(i = 0; i < p->friends_num; i++)
                        {
                            if(strcmp(p->friend[i].username, chat.news) == 0)
                            {
                                for(j = i; j < p->friends_num - 1; j++)
                                {
                                    strcpy(p->friend[j].username, p->friend[j+1].username);
                                }
                                strcpy(p->friend[j].username, "");
                                pthread_mutex_lock(&mutex);
                                p->friends_num--;
                                save();
                                head = readuser();
                                pthread_mutex_unlock(&mutex);
                                break;
                            }
                        }
                        break;
                    }
                }
                break;
            case 's':                                                       //私聊消息                                     
                pthread_mutex_lock(&mutex);
                head = readuser();
                pthread_mutex_unlock(&mutex);
                p = head->next;             
                int j;
                for(j = 0; j < LISTENQ; j++)
                {
                    if(strcmp(conn[j].name, chat.to) == 0)
                    {
                        memset(flag, 0, 1024);
                        memcpy(flag, &chat, sizeof(struct chat));
                        send(conn[j].fd, flag, 1024, 0);
                        break;
                    }
                }
                /*
                 * 先将离线消息保存到文件当中
                 */
		        memset(filename, 0, 20);                                    //若发送失败，将消息保存至文件，等待该用户登陆之后发送
                strcat(filename, chat.to);
                strcat(filename, ".db");
                fp1 = fopen(filename, "at+");
                fwrite(&chat, sizeof(struct chat), 1, fp1);
                fclose(fp1);
                memset(&chat, 0, sizeof(struct chat));
                chat.flag = '0';
                memcpy(flag, &chat, sizeof(struct chat));
                send(conn[i].fd, flag, 1024, 0);
                break;
            case 'p':                                                       //群聊消息
                pthread_mutex_lock(&mutex);
                head = readuser();
                pthread_mutex_unlock(&mutex);
                p = head->next;
                int k;
                for(k = 0; k < LISTENQ; k++)
                {
                    if(k == i)
                        continue;
                    if(conn[k].fd != -1)
                    {
                        memset(flag, 0, 1024);
                        memcpy(flag, &chat, sizeof(struct chat));
                        send(conn[k].fd, flag, 1024, 0);
                    }
                }
                break;
            case '\0':
                printf("用户%s强制退出\n", (p_user->user).username);
                conn[i].fd = -1;
                strcpy(conn[i].name, " ");
                fp1 = fopen("sys_log", "at+");
                fprintf(fp1, "用户%s强制退出\n", (p_user->user).username);
                fclose(fp1);
                pthread_exit(0);
                break;
        }
    }
}

void quit(void *arg)                        //服务器退出函数
{
    struct chat     chat;
    char buf[1024];
    char shutdown[10];                          //接受服务器退出时输入
    int  i;                                 //关闭所有的连接套接字
    while(1)
    {
        printf("----------输入exit退出！！----------\n");
        scanf("%s", shutdown);
        if(strcmp(shutdown, "exit") == 0)
        {
            memset(&chat, 0, sizeof(struct chat));
            chat.flag = 'e';
            memcpy(buf, &chat, sizeof(struct chat));
            for(i = 0; i < LISTENQ; i++)
            {
                if(conn[i].fd != -1) 
                {
                    send(conn[i].fd, buf, 1024, 0);
                    close(conn[i].fd);
                    conn[i].fd = -1;
                    strcpy(conn[i].name, " ");
                }
            }
            exit(1);
        }
    }
}


void save()                                             //用户数据保存
{
    FILE *          fp;
    struct users*   p = head->next;
    if((fp = fopen("usersdata.db", "wb")) == NULL)
    {
        printf("Cann't open usersdata.db\n");
        fp1 = fopen("err_log", "at+");
        fprintf(fp1, "cann't open userdata.db\n");
        fclose(fp1);
        exit(0);
    }
    while(p != NULL)
    {
        fwrite(p, sizeof(struct users), 1, fp);
        p = p->next;
    }
    fclose(fp);
}
 
struct users * readuser()                                   //用户数据读取函数
{
    FILE *          fp;
    struct users    *p1, *p2;
    
    if((fopen("usersdata.db", "rb")) == NULL)               //先判断文件是否存在，不存在则新建
    {
        fp = fopen("usersdata.db", "wb");                   //不存在，新建
        fclose(fp);
    }
    
    if((fp = fopen("usersdata.db", "rb")) == NULL)
    {
        printf("Cann't open usersdata.db\n");
        fp1 = fopen("err_log", "at+");
        fprintf(fp1, "conn't open userdata.db\n");
        fclose(fp1);
        exit(0);
    }
    head = p1 = (struct users*)malloc(sizeof(struct users));
    p2 = (struct users *)malloc(sizeof(struct users));
    fread(p2, sizeof(struct users), 1, fp);
    while(!feof(fp))
    {
        p1->next = p2;
        p1 = p2;
        p2 = (struct users *)malloc(sizeof(struct users));
        fread(p2, sizeof(struct users), 1, fp);
    }
    p1->next = NULL;
    free(p2);
    fclose(fp);
    return head;
}

void my_err(const char * err_string, int line)      //自定义出错处理函数
{
    fprintf(stderr, "line: %d", line);
    fp1 = fopen("err_log", "at+");
    fprintf(fp1, "%s:%s\n", err_string, strerror(errno));
    fclose(fp1);
    perror(err_string);
}

struct users * apply(struct log log, int i)         //注册函数                         
{
    char            enroll_buf[1024];
    struct log      log1;
    struct users    *p = head, *p1;
    int             ret;

    log1 = log;
    while(p->next != NULL)
    {
        if(strcmp((p->next->user).username, log1.name)  == 0)
        {
            memset(&log1, 0, sizeof(struct log));
            log1.flag = 'n';
            memcpy(enroll_buf, &log1, sizeof(struct log));
            ret = send(conn[i].fd, enroll_buf, 1024, 0);
            if(ret != 1024)
            {
                printf("send error\n");
                fp1 = fopen("err_log", "at+");
                fprintf(fp1, "send error\n");
                fclose(fp1);
                pthread_exit((void *)1);
                
            }
            return NULL;
        }
        p = p->next;
    }
    
    p1 = (struct users*)malloc(sizeof(struct user*));
    strcpy((p1->user).username, log1.name);
    strcpy(p1->password, log1.pwd);
    p1->friends_num = 0;
    p->next = p1;                                           //只将单独的信息追加到链表头指针,然后将这一单独的信息保存进文件
    p1->next = NULL;
    save();
    memset(enroll_buf, 0, sizeof(enroll_buf));
    memcpy(enroll_buf, &(p1->user), sizeof(p1->user));
    ret = send(conn[i].fd, enroll_buf, 1024, 0);
    if(ret != 1024)
    {
        printf("发送失败！\n");
        pthread_exit((void *)1);
    }
    printf("用户%s注册成功\n", (p1->user).username);
    fp1 = fopen("sys_log", "at+");
    fprintf(fp1, "用户%s注册成功\n", (p1->user).username);
    fclose(fp1);
    return p1;
}

struct users * login(struct log log, int i)                 //登陆函数
{
    char            login_buf[1024];
    struct users    *p = head->next;
    struct users    user_temp;
    int             ret;
    while(p != NULL)
    {
        if(strcmp((p->user).username, log.name) == 0)
        {
            if(strcmp(p->password, log.pwd) == 0)
            {
                user_temp.user = p->user;
                memset(login_buf, 0, sizeof(login_buf));
                memcpy(login_buf, &user_temp.user, sizeof(user_temp.user));
                ret = send(conn[i].fd, login_buf, 1024, 0);
                printf("用户%s登陆成功！\n", log.name);
                fp1 = fopen("sys_log", "at+");
                fprintf(fp1, "用户%s登陆成功！\n", log.name);
                fclose(fp1);
                if(ret != 1024)
                {
                    printf("发送失败！\n");
                    pthread_exit((void *)1);
                }
                strcpy(conn[i].name, log.name);
                return p;
            }
        }
        p = p->next;
    }
    fp1 = fopen("err_log", "at+");
    fprintf(fp1, "用户%s登陆失败\n", log.name);
    fclose(fp1);
    ret = send(conn[i].fd, "n", 1024, 0);
    if(ret != 1024)
    {
        printf("发送失败！\n");
        pthread_exit((void *)1);
    }
    return NULL;
}
