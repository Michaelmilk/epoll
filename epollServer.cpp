#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

using namespace std;

#define MAX_EVENTS  500
#define BUFFER_LEN  10240
#define SRV_IP      "172.18.136.218"
#define SRV_PORT    9100
#define LISTEN_QUEUE 100    /*监听SOCKET的等待队列*/

//相关全局变量：EPOLL描述符，服务器端监听SOCKET，数据缓冲区以及事件数组
int         epollfd;
int         srvfd;
char        buffer[BUFFER_LEN];
struct      epoll_event eventList[MAX_EVENTS];


//处理函数
void AcceptConn(int fd);
void RecvData(int fd);
void SendData(int fd);



/**************************************************
函数名：setnonblocking
功能：将通信SOCKET设置为非阻塞
***************************************************/
int setnonblocking(int sockfd)
{
    if ( fcntl( sockfd, F_SETFL, fcntl( sockfd, F_GETFD, 0 ) | O_NONBLOCK ) == -1 )
     {
         return -1;
     }
     return 0;
}


/******************************************************
函数名：initSrvSocket
功能：初始化服务器端监听SOCKET
参数：port 监听端口
*******************************************************/
int initSrvSocket(int port)
{
     struct sockaddr_in srvAddr;
     bzero( &srvAddr, sizeof( srvAddr ) );
     srvAddr.sin_family = AF_INET;
     srvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
     srvAddr.sin_port = htons (port);

     srvfd = socket( AF_INET, SOCK_STREAM, 0 );
     if ( srvfd == -1 )
     {
         printf( "can't create socket file" );
         return -1;
     }

     int opt = 1;
     setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

     if (setnonblocking( srvfd ) < 0 )
     {
         printf( "setnonblock error" );
         return -1;
     }

     if (bind( srvfd, ( struct sockaddr * ) &srvAddr, sizeof( struct sockaddr ) ) == -1 )
     {
         printf( "bind error" );
         return -1;
     }

     if (listen(srvfd, LISTEN_QUEUE) == -1)
     {
         printf( "listen error");
         return -1;
     }
}




/**************************************************
函数名：AcceptConn
功能：接受客户端的链接
参数：srvfd：监听SOCKET
***************************************************/
void AcceptConn(int srvfd)
{
    struct sockaddr_in sin;
    socklen_t len = sizeof(struct sockaddr_in);
    bzero(&sin, len);

    //接受来自客户端的连接
    int confd = accept(srvfd, (struct sockaddr*)&sin, &len);

    if (confd < 0) {
       printf("%s: bad accept\n");
       return;
    } else {
        printf("Accept Connection: %d", confd);
    }

    setnonblocking(confd);

    //将新建立的连接添加到EPOLL的监听中
    struct epoll_event event;
    event.data.fd = confd;
    event.events =  EPOLLIN|EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, confd, &event);
}




//读取数据
void RecvData(int fd)
{
    memset(buffer, 0, BUFFER_LEN);
    printf("RecvData function\n");
    int len;
    len = recv(fd, buffer, BUFFER_LEN, 0);

    if(len > 0)
    {
        printf("SOCKET HANDLE: %d: CONTENT: %s\n", fd, buffer);
    }
    else if(len == 0)
    {
        close(fd);
        printf("SOCKET HANDLE: %d closed gracefully.\n", fd);
    }
    else if (len < 0)
    {
        close(fd);
        printf("fd=%d error[%d]:%s\n", fd, errno, strerror(errno));
    }

    printf("content is %s\n", buffer);
}


//发送数据
void SendData(int fd)
{
    printf("SendData function\n");
    int len;
    len = send(fd, buffer, BUFFER_LEN, 0);

    if (len > 0) {
        printf("SendData: %s\n", buffer);
    } else if (len <= 0) {
        close(fd);
        printf("SendData:[fd=%d] error[%d]\n", (int)fd);
    }
}




void handleEvent(struct epoll_event* pEvent)
{
    printf("handleEvent function, HANDLE: %d, EVENT is %d\n",
           pEvent->data.fd,
           pEvent->events);

     if ( (pEvent->events & EPOLLERR) ||
          (pEvent->events & EPOLLHUP) ||
         !(pEvent->events & EPOLLIN)
         )
     {
          printf ( "epoll error\n");
          close (pEvent->data.fd);
          return;
     }


    if (pEvent->data.fd == srvfd) {//如果发生事件的fd是监听的srvfd，那么接受请求。
        AcceptConn(srvfd);
        printf ( "Accept one connection\n");
    } else {
        RecvData(pEvent->data.fd);
        SendData(pEvent->data.fd);
     //   epoll_ctl(epollfd, EPOLL_CTL_DEL, pEvent->data.fd, pEvent);
    }

}


int EPollServer()
{

    int  srvPort = 6888;
    initSrvSocket(srvPort);

    /* 创建 epoll 句柄，把监听 socket 加入到 epoll 集合里 */
    epollfd = epoll_create(MAX_EVENTS);
    struct epoll_event event;
    /*
    EPOLLIN：      表示对应的文件描述符可以读；
    EPOLLOUT：     表示对应的文件描述符可以写；
    EPOLLPRI：     表示对应的文件描述符有紧急的数据可读；
    EPOLLERR：     表示对应的文件描述符发生错误；
    EPOLLHUP：     表示对应的文件描述符被挂断；
    EPOLLET：      表示对应的文件描述符有事件发生；
    */
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = srvfd;
    if ( epoll_ctl(epollfd, EPOLL_CTL_ADD, srvfd, &event) < 0 )
    {
        printf("epoll Add Failed: fd=%d\n", srvfd);
        return -1;
    }

    printf( "epollEngine startup：port %d\n", srvPort);

    while(1)
    {
        /*等待事件发生*/
        int nfds = epoll_wait(epollfd, eventList, MAX_EVENTS, -1);
        if ( nfds == -1 )
        {
            printf( "epoll_wait" );
            continue;
        }

        /* 处理所有事件 */
        int n = 0;
        for (; n < nfds; n++)
            handleEvent(eventList + n);
    }

    close(epollfd);
    close(srvfd);
};

/*
void NormalServer()
{
   //init listen socket
   int srvfd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in srvAddr;
    bzero(&srvAddr, sizeof(srvAddr));
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = inet_addr(SRV_IP);
    srvAddr.sin_port = htons(SRV_PORT);
    bind(srvfd, (const sockaddr*)&srvAddr, sizeof(srvAddr));
    listen(srvfd, 20);

    while(1)
    {
        printf("AcceptConn function \n");
        struct sockaddr_in sin;
        socklen_t len = sizeof(struct sockaddr_in);
        int nfd;

        // accept
        if((nfd = accept(srvfd, (struct sockaddr*)&sin, &len)) == -1)
        {
            if(errno != EAGAIN && errno != EINTR)
                printf("%s: bad accept\n", __func__);
            return;
        }

        for (int j = 0; j < 10; j++)
        {
            recv(nfd, buffer, BUFFER_LEN, 0);
            printf("%s\n", buffer);
        }
    }
}*/

int main(int argc, char **argv)
{
    EPollServer();
    return 0;
}
