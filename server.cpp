#pragma comment(lib,"Ws2_32.lib")
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<stdio.h>
#include<string>
#include<Windows.h>
#include "FileHelper.h"
#include<iostream>
using namespace std;

char* extract(char pkt[520]);

//数据包一共1500个字节
//0-4表示序列号，5-9表示确认序列号
//10-14是标志位，分别保留位、A（ack）、R（reset）、S（syn）、F（fin）
//15-19是校验和，20-1499是数据


//套接字初始化



int check(char* ch)
{
	int sum = 0;
	for (int i = 0; i < strlen(ch); i++)
	{
		sum += abs((int)ch[i]) % 2;
	}
	return sum;
}



int main()
{
	char sendData[BUFSIZ]="你好！\n";
	char beginData[BUFSIZ]="Begin\n";
	char overData[BUFSIZ]="Over\n";
	char ok[BUFSIZ]="ok";

	WORD wVersionRequested;
	WSADATA wsaData;
 
	FileHelper fh;
	int err;
	wVersionRequested=MAKEWORD(2,2);
	err=WSAStartup(wVersionRequested,&wsaData);
	if (err!=0)
	{
		printf("WSAStartup failed with error:%d\n",err);
		return -1;
	}
	if (LOBYTE(wsaData.wVersion)!=2||HIBYTE(wsaData.wVersion)!=2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		return -1;
	}
	SOCKET socketServer=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	SOCKADDR_IN addrServ;
	addrServ.sin_addr.S_un.S_addr=htonl(INADDR_ANY);//指定0.0.0.0地址，表示任意地址
	addrServ.sin_family=AF_INET;//表示IPv4的套接字类型
	addrServ.sin_port=htons(4999);
	bind(socketServer,(SOCKADDR*)&addrServ,sizeof(SOCKADDR));
	SOCKADDR_IN addrClient;
	int length=sizeof(SOCKADDR);
	char recvBuf[512]={};
	char pkt[5008]={};
	char data[5000]={};
	char sendheader[8] = { '0', '0', '0', '0', '0', '0', '0', '\0' };
	char recvheader[8] = { '0', '0', '0', '0', '0', '0', '0', '\0' }; 
	int rev=0;
	int time=1;
	int nNetTimeout=1000;




	cout<<"等待连接......"<<endl;
	recvfrom(socketServer,recvBuf,BUFSIZ,0,(SOCKADDR*)&addrClient,&length);

	cout<<"收到信息，等待确认......"<<endl;
	sendto(socketServer,beginData,BUFSIZ,0,(SOCKADDR*)&addrClient, sizeof(SOCKADDR));

	recvfrom(socketServer,recvBuf,BUFSIZ,0,(SOCKADDR*)&addrClient,&length);
	if (strcmp(recvBuf,ok)==0)
		cout<<"收到确认消息，连接已建立！"<<endl;
	
	while (true)
	{
		DWORD TIME_OUT=10;
		
		char Filename[BUFSIZ]={};
		char ClientAddr[BUFSIZ]={};
		char FromName[BUFSIZ]={};
		FILE *f=NULL;
		
		if(err=setsockopt(socketServer,SOL_SOCKET,SO_SNDTIMEO,(char *)&TIME_OUT,sizeof(TIME_OUT)))
		{
			printf("失败！\n");
		};
		printf("%d\n",err);
		
		recvfrom(socketServer,recvBuf,BUFSIZ,0,(SOCKADDR*)&addrClient,&length);
		cout<<"第"<<time<<"次传输文件"<<endl;
		time++;
		/*
		if (setsockopt(socketServer, SOL_SOCKET, SO_RCVTIMEO, (char *)&nNetTimeout,sizeof(int)) < 0) 
		{
			printf("socket option SO_RCVTIMEO not support\n");
			return 0;
		}*/
		if (strcmp(recvBuf,ok)==0)
		{
			recvfrom(socketServer,recvBuf,BUFSIZ,0,(SOCKADDR*)&addrClient,&length);
			strcpy(ClientAddr,inet_ntoa(addrClient.sin_addr));
			strcpy(FromName,recvBuf);
			fh.createDir(ClientAddr);
			strcpy(Filename,ClientAddr);
			strcat(Filename,"\\");
			strcat(Filename,recvBuf);
			f=fh.createFile(Filename);
 
		}
		int sum=0;
		while((rev=recvfrom(socketServer,pkt,5008,0,(SOCKADDR*)&addrClient,&length))>=0)
		{
			/*
			if(rev=recvfrom(socketServer,pkt,520,0,(SOCKADDR*)&addrClient,&length)<0)
			{
					if (rev == EWOULDBLOCK || rev == EAGAIN)
					{
						printf("recvfrom timeout\n");
						sendto(socketServer,pkt, 520, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
						cout<<"重新发送数据包"<<endl;
					}
					else
					{
						printf("recvfrom err:%d\n", rev);
						sendto(socketServer,pkt, 520, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
						cout<<"重新发送数据包"<<endl;
					}
			}
			*/
			
			//cout<<pkt<<"    "<<sizeof(pkt)<<endl;
			
			if (strcmp(overData,pkt)==0)
			{
				printf("文件%s传输成功!\n",FromName);
				fclose(f);
				break;
			}
			
			for(int i=8;i<5008;i++)
				data[i-8]=pkt[i];
			
			/**/
			int che=check(data);
			if (che == 0) {
				sendheader[2] = '0';
			}
			else
			{
				sendheader[2] = '1';
			}

			cout<<"che:"<<sendheader[2]<<"   "<<pkt[2]<<endl;
			cout<<"seq:"<<sendheader[1]<<"   "<<pkt[1]<<endl;
			
			
			if(sendheader[2]!=pkt[2]  )//校验和不同，或者序号不同，发送重发的请求
			{
				sendheader[0]='0';
				sendto(socketServer,sendheader,8,0,(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
			}
			else if(sendheader[2]!=pkt[2]){


			}
			else{
				sendheader[0]='1';
				cout<<"接受无误，发生确认消息"<<endl;
				sendto(socketServer,sendheader,8,0,(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
			
				if(sendheader[1]=='0')
					sendheader[1]='1';
				else
					sendheader[1]='0';
			
			fwrite(data,1,rev-8,f);
			printf("%db\n",sum+=rev-8);
		}
		}
 
		if (rev<0||strcmp(overData,recvBuf)!=0)
		{
			printf("IP:%s发来的%s传输过程中失去连接\n",addrClient,FromName);
			fclose(f);
			remove(Filename);
		}
 
	}
 
	closesocket(socketServer);
	WSACleanup();
	return 0;
 
}
char* extract(char pkt[520]){
	char data[512];
	for(int i=8;i<520;i++)
		data[i-8]=pkt[i];
	data[511]='\0';
	return data;
}
