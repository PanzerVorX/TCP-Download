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

    while(1){//�������Ŀ������
        printf("input:");
        fgets(sendbuf,MAXSIZE,stdin);//��ȡ�����Ŀ���ļ�/λ��
        sendbuf[strlen(sendbuf)-1]=0;//fgets()�ӱ�׼��������ȡ�����ݰ������з�����Ϊ���ݷ���ʱӦ�������滻���
        if(sendbuf[0]!='/'){//����Ŀ�����Ϊ����·���Ҹ�ʽ�ϸ�
            printf("\n must be an absolute path\n");
            continue;
        }
        send(sockfd,sendbuf,MAXSIZE,0);//��������Ŀ��
        break;
    }

    while(1){//����Ŀ������
        if((recvLen=recv(sockfd,recvbuf,MAXSIZE-1,0)==-1)){
            send(sockfd,"936769482close ",strlen("936769482close "),0);
            perror("recv");
            close(sockfd);
            exit(0);
        }

        printf("recv size: %d\n",strlen(recvbuf));
        recvbuf[strlen(recvbuf)]=0;//����תת�ַ�������ʵ�����ݺ�Ԫ������Ϊ�ַ���������'/0'��0

        //��ȡ�Ӵ��״γ��ֵ�λ�ã� char *strstr(const char *haystack, const char *needle) ������needle�����Ӵ� ����ֵ�����ڷ��ص�ַ��������ifanhuiNUL������ͷ�ļ� string.h��
        if(strstr(recvbuf,"936769482filename")!=NULL){//�����ļ�
            parse(recvbuf,parameterargs);//�����洢�ļ������·��
            printf("file is %s\n",parameterargs[1]);
            memset(currentname,0,255);
            strcpy(currentname,parameterargs[1]);

            if(currentfd!=-1){
                close(currentfd);//�ر��ϸ��ļ�
            }

            if((currentfd=open(parameterargs[1],O_WRONLY|O_TRUNC|O_CREAT,644))==-1){//�����ļ�
                send(sockfd,"936769482close ",strlen("936769482close "),0);
                perror("open");
                close(sockfd);
                exit(0);
            }
            printf("file create\n");
            send(sockfd,"file continue",strlen("file continue"),0);//ȷ�ϼ���
        }
        else if(strstr(recvbuf,"936769482dirname")!=NULL){
            parse(recvbuf,parameterargs);//�����洢Ŀ¼�����·��
            printf("dir is %s\n",parameterargs[1]);
            memset(currentname,0,255);
            strcpy(currentname,parameterargs[1]);

            //�ж��ļ��Ƿ���ڣ�int access(const char *filename, int mode) ������mode�ж����ͣ�Ŀ¼/�ļ�Ϊ0������ֵ������Ϊ0��������Ϊ-1������ͷ�ļ���io.h��
            while(access(parameterargs[1],0)==-1){
                mkdir(parameterargs[1],644);//����Ŀ¼
            }
            printf("dir create\n");
            send(sockfd,"dir continue",strlen("dir continue"),0);//ȷ�ϼ���
        }
        else if(strstr(recvbuf,"936769482complete")!=NULL){//�������
            printf("\n Download Complete \n");
            close(sockfd);
            break;
        }
        else if(strstr(recvbuf,"936769482noexist")!=NULL){//����Ŀ�겻����
            send(sockfd,"936769482close ",strlen("936769482close "),0);
            printf("\nfile or dir no exist\n");
            close(sockfd);
            break;
        }
        else{//���ļ�д������
            if(write(currentfd,recvbuf,strlen(recvbuf))<0){
                send(sockfd,"936769482close ",strlen("936769482close "),0);
                printf("error write to : %s\n",currentname);
                perror("write");
                close(sockfd);
                exit(0);
            }
            send(sockfd,"data continue",strlen("data continue"),0);//ȷ�ϼ���
        }

        memset(parameterargs,0,10);//��ղ�������
        memset(recvbuf,0,MAXSIZE);//��ս�������
    }
}

//�ָ������ʽ���ַ��������ַ��������ַ������ͨ���ƶ���ַ������õ�ÿ����Ч�������׵�ַ��ֵ���ַ�ָ�������ӦԪ�أ��������Ŀհ׷��滻Ϊ�ַ���������
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
