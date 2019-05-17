#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>

#define PORT 3333
#define SERVERIP "192.168.1.200"
#define MAXSIZE 512

struct threadparameter{//封装线程函数的参数
	char * clientip_pstr;//客户端ip的点分十形式
	int connfd;//与对应客户端通信的连接套接字
};
//确保线程函数已接收参数以防止提前释放存储参数的空间：通过全局判断变量（线程间通过全局变量通信）
int isrecvparameter=0;

void * communication(void *);
int checkfiletype(char *);
void downloadfile(int,char *,char *);
void downloaddir(int,char *,char *);
long countdirsize(char *);

int main(){

	int sockfd,connfd,sin_size;
	int val = 1;
	pthread_t communicationthread;//存储线程id
	struct threadparameter thread_parameter;//线程函数参数结构体
	struct sockaddr_in serveraddr,clientaddr;//网络信息结构体

	if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1){
		perror("socket");
		exit(0);
	}

	serveraddr.sin_family=AF_INET;
	serveraddr.sin_port=htons(PORT);
	serveraddr.sin_addr.s_addr=inet_addr(SERVERIP);
	bzero(&serveraddr.sin_zero,8);

	bind(sockfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr));

	setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(char *)&val,sizeof(val));//端口可重用
	listen(sockfd,10);//设置监听套接字

	while(1){//监听客户端的请求并分配线程处理

		sin_size=sizeof(clientaddr);
		if((connfd=accept(sockfd,(struct sockaddr *)&clientaddr,&sin_size))<0){
			perror("accept");
			exit(0);
		}

		thread_parameter.clientip_pstr=inet_ntoa(clientaddr.sin_addr);//整型转点十分
		thread_parameter.connfd=connfd;//连接套接字描述符
		pthread_create(&communicationthread,NULL,communication,(void *)&thread_parameter);//创建传输线程

		while(isrecvparameter==0);//等待线程函数接收参数
		isrecvparameter=0;
		sleep(3);//休眠3s
	}

	return 0;
}

void * communication(void * pthread_parameter){//传输线程

    //从函数参数获取客户端IP与连接套接字描述符
	char * clientip_pstr=((struct threadparameter * )pthread_parameter)->clientip_pstr;
	int connfd=((struct threadparameter*)pthread_parameter)->connfd;
	isrecvparameter=1;//确认已接收参数

	printf("client [%s] connect\n",clientip_pstr);//输出连接客户端
	char recvbuf[MAXSIZE],sendbuf[MAXSIZE];//接收/发送数据区
	int recvLen;//接收/发送实际长度
	int len;
	char * temp;
	char dest[255];//客户端下载文件的相对路径

	if((recvLen=recv(connfd,recvbuf,MAXSIZE-1,0))==-1){//获取请求下载的目标
		perror("recv");
		exit(0);
	}
	recvbuf[recvLen]=0;//转字符串
	printf("%s request download %s\n",clientip_pstr,recvbuf);//输出接收的下载目标

	if((checkfiletype(recvbuf)!=2)&&(checkfiletype(recvbuf)!=0)){//当下载目标为文件

        //获取客户端存储文件的相对路径
        temp=strrchr(recvbuf,'/');
        int n=temp-recvbuf;
        strncpy(dest,temp+1,strlen(recvbuf)-n-1);

		downloadfile(connfd,recvbuf,dest);//下载文件
		printf("\n %s Download %s Complete \n",clientip_pstr,recvbuf);
        strcpy(sendbuf,"936769482complete ");
		send(connfd,sendbuf,MAXSIZE,0);//发送下载完成标识
	}
	else if(checkfiletype(recvbuf)==2){//当下载目标为目录

		//获取客户端存储目录的相对路径
		len=strlen(recvbuf);
		if(recvbuf[len-1]=='/'){//若末尾存在/
                    recvbuf[len-1]==0;//获取字符数组中第n次出现的指定字符位置：通过多次将字符首次/末次出现的位置数据替换为其他字符进行按序定位（如字符串结束符）
            		temp=strrchr(recvbuf,'/');
            		int n=temp-recvbuf;
            		strncpy(dest,temp+1,strlen(recvbuf)-n-1);
		}
		else{//若末尾不存在/
            		temp=strrchr(recvbuf,'/');
            		int n=temp-recvbuf;
            		strncpy(dest,temp+1,strlen(recvbuf)-n-1);
		}

		downloaddir(connfd,recvbuf,dest);//下载目录
		printf("\n %s Download %s Complete \n",clientip_pstr,recvbuf);
		strcpy(sendbuf,"936769482complete ");
        send(connfd,sendbuf,MAXSIZE,0);//发送下载完成标识
	}
	else{//当下载目标不存在
		strcpy(sendbuf,"936769482noexist");
        send(connfd,sendbuf,MAXSIZE,0);//发送文件不存在标识
	}
	close(connfd);
}

int checkfiletype(char * name){//判断文件属性

	struct stat buf;
	if(lstat(name,&buf)==-1){
		return 0;
	}
	switch(buf.st_mode&S_IFMT){
		case S_IFREG:
			return 1;
		case S_IFDIR:
			return 2;
		case S_IFLNK:
			return 3;
		case S_IFCHR:
			return 4;
		case S_IFBLK:
			return 5;
		case S_IFSOCK:
			return 6;
		case S_IFIFO:
			return 7;
	}
}

void downloadfile(int connfd,char * from ,char * to){//下载文件
	char recvbuf[MAXSIZE],sendbuf[MAXSIZE];
	int fromfd;
	int num;
	int val;

    //客户端与服务端可通过在发送的信息中加入操作标识让对方识别以控制对方操作
    //格式化数据写入字符串变量（字符数组）：int sprintf(char *str, char * format [, argument, ...]) 可用于其它类型转字符串，返回值：成功为字符串长度，失败为-1（关联头文件：stdio.h）
	sprintf(sendbuf,"936769482filename %s ",to);
    send(connfd,sendbuf,strlen(sendbuf),0);
	memset(sendbuf,0,MAXSIZE);//每次发送后将发送数组清空

	//TCP粘包：发送端为了将多个数据包更有效的发到对方，使用了优化方法（Nagle算法）将多次间隔小/数据量小的数据包合并成一个大的数据包进行发送
	//客户端与服务端保持收发同步（防止TCP粘包）：发送端等待接收到接收端每步操作完毕后发送确认继续的信息
    recv(connfd,recvbuf,MAXSIZE-1,0);
    //客户端异常关闭前应向服务端发送断开连接的信息以防止服务端继续使用失效套接字导致出错
    if(strstr(recvbuf,"936769482close")!=NULL){
        close(connfd);
        pthread_exit((void *)&val);
    }
    memset(recvbuf,0,MAXSIZE);

	if((fromfd=open(from,O_RDONLY))==-1){//打开目标文件
		perror("opem from");
		return;
	}

	while((num=read(fromfd,sendbuf,MAXSIZE-1))>0){//读取

		printf("%s\n",sendbuf);
		printf("read size:%d\n",strlen(sendbuf));
		if(send(connfd,sendbuf,num,0)<0){//发送数据
			printf("write %s error\n",to);
			perror("write");
		}
		memset(sendbuf,0,MAXSIZE);//复用空间（如数组）每次使用完应清空
        recv(connfd,recvbuf,MAXSIZE-1,0);
        if(strstr(recvbuf,"936769482close")!=NULL){
            close(connfd);
            pthread_exit((void *)&val);
        }
        memset(recvbuf,0,MAXSIZE);
	}
	close(fromfd);//关闭文件
}

void downloaddir(int connfd,char *from,char * to){//下载目录

    DIR * d1;
    struct dirent * dent1;
    char fromtotalname[255];//源文件路径
    char tototalname[255];//客户端存储下载文件的相对路径
    char frombuf[255],tobuf[255];//存储目录项的路径的复用数组
    char sendbuf[1024],recvbuf[1024];
    int len;
    int val;

    d1=opendir(from);//打开目录
    if(d1==NULL){
        perror("opendir");
        return;
    }

    sprintf(sendbuf,"936769482dirname %s ",to);
	send(connfd,sendbuf,strlen(sendbuf),0);//发送创建目录的信息
	memset(sendbuf,0,MAXSIZE);
    recv(connfd,recvbuf,MAXSIZE-1,0);
    if(strstr(recvbuf,"936769482close")!=NULL){
        close(connfd);
        pthread_exit((void *)&val);
    }
    memset(recvbuf,0,MAXSIZE);

    //遍历目录前应考虑完善目录路径尾部的/符号，便于后面进行拼接目录子项
    strcpy(tototalname,to);//若带指针参数的递归函数存在对指针对应空间的修改操作则自调处应考虑传递新空间指针以防止被调函数改变主调函数未操作完的原参数
    len=strlen(to);
    if(to[len-1]!='/'){
        strcat(tototalname,"/");
    }

    strcpy(fromtotalname,from);
    len=strlen(from);
    if(from[len-1]!='/'){
        strcat(fromtotalname,"/");
    }

    while((dent1=readdir(d1))!=NULL){//读取目录
		if((dent1->d_name[0])!='.'){

			//拼接目录子项路径
			strcpy(frombuf,fromtotalname);
			strcat(frombuf,dent1->d_name);

			strcpy(tobuf,tototalname);
			strcat(tobuf,dent1->d_name);

			//判断目录项的文件类型
			if(checkfiletype(frombuf)!=2){
				downloadfile(connfd,frombuf,tobuf);
				printf("a file complete\n");
			}
			else{
				 downloaddir(connfd,frombuf,tobuf);//递归
			}
		}
	}
    closedir(d1);//关闭目录
}
