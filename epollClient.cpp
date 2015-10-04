#include <unistd.h>
#include <sys/types.h>       /* basic system data types */
#include <sys/epoll.h>
#include <sys/socket.h>      /* basic socket definitions */
#include <netinet/in.h>      /* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>       /* inet(3) functions */
#include <netdb.h> 	      /*gethostbyname function */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <queue>
#include <iostream>

using namespace std;




// #include <fcntl.h>
// #include <time.h>

#define MAXLINE		10240
#define MAX_EVENTS	500


//相关全局变量：EPOLL描述符，服务器端监听SOCKET，数据缓冲区以及事件数组
int         epollfd;
int connfd;
struct epoll_event eventList[MAX_EVENTS];


void handle(int sockfd)
{
	char sendline[MAXLINE], recvline[MAXLINE];
	int n;

	// printf("dfsdsd\n");
	// if (fgets(sendline, MAXLINE, stdin) == NULL) {
	// 	break;//read eof
	// }
	/*
	//也可以不用标准库的缓冲流,直接使用系统函数无缓存操作
	if (read(STDIN_FILENO, sendline, MAXLINE) == 0) {
		break;//read eof
	}
	*/

	//n = write(sockfd, sendline, strlen(sendline));
	n = read(sockfd, recvline, MAXLINE);
	if (n == 0) {
		printf("echoclient: server terminated prematurely\n");
	}
	printf("Recv from srv, content : %s\n", recvline);
	//write(STDOUT_FILENO, recvline, n);
	//如果用标准库的缓存流输出有时会出现问题
	//fputs(recvline, stdout);
	//printf("exit handle\n");
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


    if (pEvent->data.fd == connfd) {//如果发生事件的fd是监听的srvfd，那么接受请求。
        handle(connfd);
    }
    //printf("exit handleEvent\n");
}


int EpollHandle()
{
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
	event.data.fd = connfd;
	if ( epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &event) < 0 )
	{
		printf("epoll Add Failed: fd=%d\n", connfd);
		return -1;
	}

	printf( "epollEngine startup\n");

	while(1)
	{
		//printf("loop\n");
		char sendline[MAXLINE];
		printf("Input : ");
		scanf("%s", sendline);
		int len = write(connfd, sendline, strlen(sendline));
		if (len > 0) {
			printf("SendData: %s\n", sendline);
		} else if (len <= 0) {
			close(connfd);
			printf("SendData:[fd=%d] error[%d]\n", (int)connfd);
		}

		/*等待事件发生*/
		int nfds = epoll_wait(epollfd, eventList, MAX_EVENTS, -1);
		if ( nfds == -1 )
		{
			printf( "epoll_wait" );
			continue;
		}
		//printf("nfds : %d\n", nfds);
		/* 处理所有事件 */
		int n = 0;
		for (; n < nfds; n++)
			handleEvent(eventList + n);
		//printf("again loop\n");
	}

	close(epollfd);
	close(connfd);
};

int ConnectToSrv(char* servInetAddr, int servPort)
{
	struct sockaddr_in servaddr;
	connfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(servPort);
	inet_pton(AF_INET, servInetAddr, &servaddr.sin_addr);

	if (connect(connfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		perror("connect error");
		return -1;
	}
}


int main(int argc, char **argv)
{
	char * servInetAddr = "127.0.0.1";
	int servPort = 6888;
	//char buf[MAXLINE];
	//int connfd;
	//struct sockaddr_in servaddr;

	//可以在执行的时候带参数，第一个参数为srv的IP，第二个为srv的端口
	if (argc == 2) {
		servInetAddr = argv[1];
	}
	if (argc == 3) {
		servInetAddr = argv[1];
		servPort = atoi(argv[2]);
	}
	if (argc > 3) {
		printf("usage: echoclient <IPaddress> <Port>\n");
		return -1;
	}

	// connfd = socket(AF_INET, SOCK_STREAM, 0);

	// bzero(&servaddr, sizeof(servaddr));
	// servaddr.sin_family = AF_INET;
	// servaddr.sin_port = htons(servPort);
	// inet_pton(AF_INET, servInetAddr, &servaddr.sin_addr);

	// if (connect(connfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
	// 	perror("connect error");
	// 	return -1;
	// }
	printf("welcome to echoclient\n");
	//handle(connfd);     /* do it all */
	ConnectToSrv(servInetAddr, servPort);
	EpollHandle();
	close(connfd);
	printf("exit\n");
	exit(0);
}