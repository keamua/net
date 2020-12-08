#pragma comment(lib,"Ws2_32.lib")
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<stdio.h>
#include<Windows.h>
#include "FileHelper.h"
 
int main()
{
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
	char recvBuf[BUFSIZ]={};
	int rev=0;
	while (true)
	{
		DWORD TIME_OUT=10;
		char sendData[BUFSIZ]="你好！\n";
		char beginData[BUFSIZ]="Begin\n";
		char overData[BUFSIZ]="Over\n";
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
		if (strcmp(recvBuf,beginData)==0)
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
		while((rev=recvfrom(socketServer,recvBuf,BUFSIZ,0,(SOCKADDR*)&addrClient,&length))>0)
		{
			if (strcmp(overData,recvBuf)==0)
			{
				printf("文件%s传输成功!\n",FromName);
				fclose(f);
				break;
			}
		//	printf(recvBuf);
			fwrite(recvBuf,1,rev,f);
			printf("%db\n",sum+=rev);
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
