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

struct threadparameter{//��װ�̺߳����Ĳ���
	char * clientip_pstr;//�ͻ���ip�ĵ��ʮ��ʽ
	int connfd;//���Ӧ�ͻ���ͨ�ŵ������׽���
};
//ȷ���̺߳����ѽ��ղ����Է�ֹ��ǰ�ͷŴ洢�����Ŀռ䣺ͨ��ȫ���жϱ������̼߳�ͨ��ȫ�ֱ���ͨ�ţ�
int isrecvparameter=0;

void * communication(void *);
int checkfiletype(char *);
void downloadfile(int,char *,char *);
void downloaddir(int,char *,char *);
long countdirsize(char *);

int main(){

	int sockfd,connfd,sin_size;
	int val = 1;
	pthread_t communicationthread;//�洢�߳�id
	struct threadparameter thread_parameter;//�̺߳��������ṹ��
	struct sockaddr_in serveraddr,clientaddr;//������Ϣ�ṹ��

	if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1){
		perror("socket");
		exit(0);
	}

	serveraddr.sin_family=AF_INET;
	serveraddr.sin_port=htons(PORT);
	serveraddr.sin_addr.s_addr=inet_addr(SERVERIP);
	bzero(&serveraddr.sin_zero,8);

	bind(sockfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr));

	setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(char *)&val,sizeof(val));//�˿ڿ�����
	listen(sockfd,10);//���ü����׽���

	while(1){//�����ͻ��˵����󲢷����̴߳���

		sin_size=sizeof(clientaddr);
		if((connfd=accept(sockfd,(struct sockaddr *)&clientaddr,&sin_size))<0){
			perror("accept");
			exit(0);
		}

		thread_parameter.clientip_pstr=inet_ntoa(clientaddr.sin_addr);//����ת��ʮ��
		thread_parameter.connfd=connfd;//�����׽���������
		pthread_create(&communicationthread,NULL,communication,(void *)&thread_parameter);//���������߳�

		while(isrecvparameter==0);//�ȴ��̺߳������ղ���
		isrecvparameter=0;
		sleep(3);//����3s
	}

	return 0;
}

void * communication(void * pthread_parameter){//�����߳�

    //�Ӻ���������ȡ�ͻ���IP�������׽���������
	char * clientip_pstr=((struct threadparameter * )pthread_parameter)->clientip_pstr;
	int connfd=((struct threadparameter*)pthread_parameter)->connfd;
	isrecvparameter=1;//ȷ���ѽ��ղ���

	printf("client [%s] connect\n",clientip_pstr);//������ӿͻ���
	char recvbuf[MAXSIZE],sendbuf[MAXSIZE];//����/����������
	int recvLen;//����/����ʵ�ʳ���
	int len;
	char * temp;
	char dest[255];//�ͻ��������ļ������·��

	if((recvLen=recv(connfd,recvbuf,MAXSIZE-1,0))==-1){//��ȡ�������ص�Ŀ��
		perror("recv");
		exit(0);
	}
	recvbuf[recvLen]=0;//ת�ַ���
	printf("%s request download %s\n",clientip_pstr,recvbuf);//������յ�����Ŀ��

	if((checkfiletype(recvbuf)!=2)&&(checkfiletype(recvbuf)!=0)){//������Ŀ��Ϊ�ļ�

        //��ȡ�ͻ��˴洢�ļ������·��
        temp=strrchr(recvbuf,'/');
        int n=temp-recvbuf;
        strncpy(dest,temp+1,strlen(recvbuf)-n-1);

		downloadfile(connfd,recvbuf,dest);//�����ļ�
		printf("\n %s Download %s Complete \n",clientip_pstr,recvbuf);
        strcpy(sendbuf,"936769482complete ");
		send(connfd,sendbuf,MAXSIZE,0);//����������ɱ�ʶ
	}
	else if(checkfiletype(recvbuf)==2){//������Ŀ��ΪĿ¼

		//��ȡ�ͻ��˴洢Ŀ¼�����·��
		len=strlen(recvbuf);
		if(recvbuf[len-1]=='/'){//��ĩβ����/
                    recvbuf[len-1]==0;//��ȡ�ַ������е�n�γ��ֵ�ָ���ַ�λ�ã�ͨ����ν��ַ��״�/ĩ�γ��ֵ�λ�������滻Ϊ�����ַ����а���λ�����ַ�����������
            		temp=strrchr(recvbuf,'/');
            		int n=temp-recvbuf;
            		strncpy(dest,temp+1,strlen(recvbuf)-n-1);
		}
		else{//��ĩβ������/
            		temp=strrchr(recvbuf,'/');
            		int n=temp-recvbuf;
            		strncpy(dest,temp+1,strlen(recvbuf)-n-1);
		}

		downloaddir(connfd,recvbuf,dest);//����Ŀ¼
		printf("\n %s Download %s Complete \n",clientip_pstr,recvbuf);
		strcpy(sendbuf,"936769482complete ");
        send(connfd,sendbuf,MAXSIZE,0);//����������ɱ�ʶ
	}
	else{//������Ŀ�겻����
		strcpy(sendbuf,"936769482noexist");
        send(connfd,sendbuf,MAXSIZE,0);//�����ļ������ڱ�ʶ
	}
	close(connfd);
}

int checkfiletype(char * name){//�ж��ļ�����

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

void downloadfile(int connfd,char * from ,char * to){//�����ļ�
	char recvbuf[MAXSIZE],sendbuf[MAXSIZE];
	int fromfd;
	int num;
	int val;

    //�ͻ��������˿�ͨ���ڷ��͵���Ϣ�м��������ʶ�öԷ�ʶ���Կ��ƶԷ�����
    //��ʽ������д���ַ����������ַ����飩��int sprintf(char *str, char * format [, argument, ...]) ��������������ת�ַ���������ֵ���ɹ�Ϊ�ַ������ȣ�ʧ��Ϊ-1������ͷ�ļ���stdio.h��
	sprintf(sendbuf,"936769482filename %s ",to);
    send(connfd,sendbuf,strlen(sendbuf),0);
	memset(sendbuf,0,MAXSIZE);//ÿ�η��ͺ󽫷����������

	//TCPճ�������Ͷ�Ϊ�˽�������ݰ�����Ч�ķ����Է���ʹ�����Ż�������Nagle�㷨������μ��С/������С�����ݰ��ϲ���һ��������ݰ����з���
	//�ͻ��������˱����շ�ͬ������ֹTCPճ���������Ͷ˵ȴ����յ����ն�ÿ��������Ϻ���ȷ�ϼ�������Ϣ
    recv(connfd,recvbuf,MAXSIZE-1,0);
    //�ͻ����쳣�ر�ǰӦ�����˷��ͶϿ����ӵ���Ϣ�Է�ֹ����˼���ʹ��ʧЧ�׽��ֵ��³���
    if(strstr(recvbuf,"936769482close")!=NULL){
        close(connfd);
        pthread_exit((void *)&val);
    }
    memset(recvbuf,0,MAXSIZE);

	if((fromfd=open(from,O_RDONLY))==-1){//��Ŀ���ļ�
		perror("opem from");
		return;
	}

	while((num=read(fromfd,sendbuf,MAXSIZE-1))>0){//��ȡ

		printf("%s\n",sendbuf);
		printf("read size:%d\n",strlen(sendbuf));
		if(send(connfd,sendbuf,num,0)<0){//��������
			printf("write %s error\n",to);
			perror("write");
		}
		memset(sendbuf,0,MAXSIZE);//���ÿռ䣨�����飩ÿ��ʹ����Ӧ���
        recv(connfd,recvbuf,MAXSIZE-1,0);
        if(strstr(recvbuf,"936769482close")!=NULL){
            close(connfd);
            pthread_exit((void *)&val);
        }
        memset(recvbuf,0,MAXSIZE);
	}
	close(fromfd);//�ر��ļ�
}

void downloaddir(int connfd,char *from,char * to){//����Ŀ¼

    DIR * d1;
    struct dirent * dent1;
    char fromtotalname[255];//Դ�ļ�·��
    char tototalname[255];//�ͻ��˴洢�����ļ������·��
    char frombuf[255],tobuf[255];//�洢Ŀ¼���·���ĸ�������
    char sendbuf[1024],recvbuf[1024];
    int len;
    int val;

    d1=opendir(from);//��Ŀ¼
    if(d1==NULL){
        perror("opendir");
        return;
    }

    sprintf(sendbuf,"936769482dirname %s ",to);
	send(connfd,sendbuf,strlen(sendbuf),0);//���ʹ���Ŀ¼����Ϣ
	memset(sendbuf,0,MAXSIZE);
    recv(connfd,recvbuf,MAXSIZE-1,0);
    if(strstr(recvbuf,"936769482close")!=NULL){
        close(connfd);
        pthread_exit((void *)&val);
    }
    memset(recvbuf,0,MAXSIZE);

    //����Ŀ¼ǰӦ��������Ŀ¼·��β����/���ţ����ں������ƴ��Ŀ¼����
    strcpy(tototalname,to);//����ָ������ĵݹ麯�����ڶ�ָ���Ӧ�ռ���޸Ĳ������Ե���Ӧ���Ǵ����¿ռ�ָ���Է�ֹ���������ı���������δ�������ԭ����
    len=strlen(to);
    if(to[len-1]!='/'){
        strcat(tototalname,"/");
    }

    strcpy(fromtotalname,from);
    len=strlen(from);
    if(from[len-1]!='/'){
        strcat(fromtotalname,"/");
    }

    while((dent1=readdir(d1))!=NULL){//��ȡĿ¼
		if((dent1->d_name[0])!='.'){

			//ƴ��Ŀ¼����·��
			strcpy(frombuf,fromtotalname);
			strcat(frombuf,dent1->d_name);

			strcpy(tobuf,tototalname);
			strcat(tobuf,dent1->d_name);

			//�ж�Ŀ¼����ļ�����
			if(checkfiletype(frombuf)!=2){
				downloadfile(connfd,frombuf,tobuf);
				printf("a file complete\n");
			}
			else{
				 downloaddir(connfd,frombuf,tobuf);//�ݹ�
			}
		}
	}
    closedir(d1);//�ر�Ŀ¼
}
