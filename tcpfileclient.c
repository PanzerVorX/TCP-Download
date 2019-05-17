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

#define PORT 3333
#define SERVERIP "192.168.1.200"
#define MAXSIZE 512

int main(){

    int sockfd;
    struct sockaddr_in serveraddr;
    int currentfd=-1;
    char currentname[255];
    char recvbuf[MAXSIZE],sendbuf[MAXSIZE];
    int recvLen;
    char * parameterargs[10];

    if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1){
        printf("errorrecvbuf:%s\n",recvbuf);
        perror("socket");
        exit(0);
    }

    serveraddr.sin_family=AF_INET;
	serveraddr.sin_port=htons(PORT);
	serveraddr.sin_addr.s_addr=inet_addr(SERVERIP);
	bzero(&serveraddr.sin_zero,8);

    if(connect(sockfd,(struct sockaddr *)&serveraddr,sizeof(struct sockaddr))==-1){
        perror("connect");
        exit(0);
    }

    sleep(3);

    while(1){//输出下载目标流程
        printf("input:");
        fgets(sendbuf,MAXSIZE,stdin);//读取输入的目标文件/位置
        sendbuf[strlen(sendbuf)-1]=0;//fgets()从标准输入流获取的数据包括换行符，作为数据发送时应考虑用替换清除
        if(sendbuf[0]!='/'){//下载目标必须为绝对路径且格式严格
            printf("\n must be an absolute path\n");
            continue;
        }
        send(sockfd,sendbuf,MAXSIZE,0);//发送下载目标
        break;
    }

    while(1){//下载目标流程
        if((recvLen=recv(sockfd,recvbuf,MAXSIZE-1,0)==-1)){
            send(sockfd,"936769482close ",strlen("936769482close "),0);
            perror("recv");
            close(sockfd);
            exit(0);
        }

        printf("recv size: %d\n",strlen(recvbuf));
        recvbuf[strlen(recvbuf)]=0;//数组转转字符串：将实际内容后元素设置为字符串结束符'/0'或0

        //获取子串首次出现的位置： char *strstr(const char *haystack, const char *needle) 参数：needle查找子串 返回值：存在返回地址，不存在ifanhuiNUL（关联头文件 string.h）
        if(strstr(recvbuf,"936769482filename")!=NULL){//创建文件
            parse(recvbuf,parameterargs);//解析存储文件的相对路径
            printf("file is %s\n",parameterargs[1]);
            memset(currentname,0,255);
            strcpy(currentname,parameterargs[1]);

            if(currentfd!=-1){
                close(currentfd);//关闭上个文件
            }

            if((currentfd=open(parameterargs[1],O_WRONLY|O_TRUNC|O_CREAT,644))==-1){//创建文件
                send(sockfd,"936769482close ",strlen("936769482close "),0);
                perror("open");
                close(sockfd);
                exit(0);
            }
            printf("file create\n");
            send(sockfd,"file continue",strlen("file continue"),0);//确认继续
        }
        else if(strstr(recvbuf,"936769482dirname")!=NULL){
            parse(recvbuf,parameterargs);//解析存储目录的相对路径
            printf("dir is %s\n",parameterargs[1]);
            memset(currentname,0,255);
            strcpy(currentname,parameterargs[1]);

            //判断文件是否存在：int access(const char *filename, int mode) 参数：mode判断类型（目录/文件为0）返回值：存在为0，不存在为-1（关联头文件：io.h）
            while(access(parameterargs[1],0)==-1){
                mkdir(parameterargs[1],644);//创建目录
            }
            printf("dir create\n");
            send(sockfd,"dir continue",strlen("dir continue"),0);//确认继续
        }
        else if(strstr(recvbuf,"936769482complete")!=NULL){//下载完毕
            printf("\n Download Complete \n");
            close(sockfd);
            break;
        }
        else if(strstr(recvbuf,"936769482noexist")!=NULL){//下载目标不存在
            send(sockfd,"936769482close ",strlen("936769482close "),0);
            printf("\nfile or dir no exist\n");
            close(sockfd);
            break;
        }
        else{//向文件写入数据
            if(write(currentfd,recvbuf,strlen(recvbuf))<0){
                send(sockfd,"936769482close ",strlen("936769482close "),0);
                printf("error write to : %s\n",currentname);
                perror("write");
                close(sockfd);
                exit(0);
            }
            send(sockfd,"data continue",strlen("data continue"),0);//确认继续
        }

        memset(parameterargs,0,10);//清空参数数组
        memset(recvbuf,0,MAXSIZE);//清空接收数组
    }
}

//分割参数形式的字符串：将字符串存入字符数组后通过移动地址，将获得的每个有效参数区首地址赋值给字符指针数组对应元素，并将其后的空白符替换为字符串结束符
int parse(char * buf,char **args){

    int num=0;

    while(*buf!='\0'){

        while((*buf==' ')||(*buf=='\t')||(*buf=='\n')){
            *buf='\0';
            buf++;
        }
        *args=buf;
        args++;
        num++;
        while((*buf!='\0')&&(*buf!=' ')&&(*buf!='\t')&&(*buf!='\n')){
            buf ++;
        }
    }
    *args='\0';
    return num;
}
