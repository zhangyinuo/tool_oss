#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <assert.h>


#define MAXBUF 64
#define MAXEPOLLSIZE 100


/*
   setnonblocking �C ���þ��Ϊ��������ʽ
   */
int setnonblocking(int sockfd)
{
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1)
	{
		return -1;
	}
	return 0;
}




/*
   pthread_handle_message �C �̴߳��� socket �ϵ���Ϣ�շ�
   */
void* pthread_handle_message(int* sock_fd)
{
	char recvbuf[MAXBUF + 1];
	char sendbuf[MAXBUF+1];
	int  ret;
	int  new_fd;
	struct sockaddr_in client_addr;
	socklen_t cli_len=sizeof(client_addr);


	new_fd=*sock_fd; 


	/* ��ʼ����ÿ���������ϵ������շ� */
	bzero(recvbuf, MAXBUF + 1);
	bzero(sendbuf, MAXBUF + 1);


	/* ���տͻ��˵���Ϣ */
	ret = recvfrom(new_fd, recvbuf, MAXBUF, 0, (struct sockaddr *)&client_addr, &cli_len);
	if (ret > 0){
		printf("socket %d recv from : %s : %d message: %s ��%d bytes\n",
				new_fd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), recvbuf, ret);
		sendto(new_fd, recvbuf, strlen(recvbuf), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
	}
	else
	{
		fprintf(stderr, "received failed! error code %d��message : %s \n",
				errno, strerror(errno));    
	}
	/* ����ÿ���������ϵ������շ����� */ 
	fprintf(stderr, "pthread exit!\n");
	fflush(stdout); 
	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	int listener, kdpfd, nfds, n, curfds;
	socklen_t len;
	struct sockaddr_in my_addr, their_addr;
	unsigned int myport;
	struct epoll_event ev;
	struct epoll_event events[MAXEPOLLSIZE];
	struct rlimit rt;


	myport = 1234; 


	pthread_t thread;
	pthread_attr_t attr;


	/* ����ÿ����������򿪵�����ļ��� */
	rt.rlim_max = rt.rlim_cur = MAXEPOLLSIZE;
	if (setrlimit(RLIMIT_NOFILE, &rt) == -1) 
	{
		perror("setrlimit");
		exit(1);
	}
	else 
	{
		fprintf(stdout, "setting success \n");
	}


	/* ���� socket ���� */
	if ((listener = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror("socket create failed ��");
		exit(1);
	}
	else
	{
		fprintf(stdout, "socket create  success \n");
	}


	/*����socket���ԣ��˿ڿ�������*/
	int opt=SO_REUSEADDR;
	setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));


	setnonblocking(listener);
	bzero(&my_addr, sizeof(my_addr));
	my_addr.sin_family = PF_INET;
	my_addr.sin_port = htons(myport);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(listener, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) 
	{
		perror("bind");
		exit(1);
	} 
	else
	{
		fprintf(stdout, "IP and port bind success \n");
	}

	/* ���� epoll ������Ѽ��� socket ���뵽 epoll ������ */
	kdpfd = epoll_create(MAXEPOLLSIZE);
	len = sizeof(struct sockaddr_in);
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = listener;
	if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, listener, &ev) < 0) 
	{
		fprintf(stderr, "epoll set insertion error: fd=%d\n", listener);
		return -1;
	}
	else
	{
		fprintf(stdout, "listen socket added in  epoll success \n");
	}


	while (1) 
	{
		/* �ȴ����¼����� */
		nfds = epoll_wait(kdpfd, events, 100, 1000);
		if (nfds == -1)
		{
			perror("epoll_wait");
			break;
		}

		/* ���������¼� */
		for (n = 0; n < nfds; ++n)
		{
			if (events[n].data.fd == listener) 
			{
				/*��ʼ������ֵ������ΪĬ��ֵ*/
				pthread_attr_init(&attr);
				pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);


				/*  �����߳�Ϊ��������*/ 
				pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);


				if(pthread_create(&thread,&attr,(void*)pthread_handle_message,(void*)&(events[n].data.fd)))
				{
					perror("pthread_creat error!");
					exit(-1);
				} 
			} 
		}
	}
	close(listener);
	return 0;
}

