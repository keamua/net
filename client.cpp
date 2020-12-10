#pragma comment(lib,"Ws2_32.lib")

#include<WinSock2.h>
#include<WS2tcpip.h>
#include<stdio.h>
#include<Windows.h>
#include<time.h>


#include"FileHelper.h"

using namespace std;



char* make_pkt(char header[8], char data[BUFSIZ]);

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

	WORD wVersionRequested;
	WSADATA wsaData;
	char sendData[BUFSIZ] = "";
	char ok[BUFSIZ] = "ok";
	char beginData[BUFSIZ] = "Begin\n";
	char overData[BUFSIZ] = "Over\n";
	char Filename[BUFSIZ] = {};

	



	FileHelper fh;
	int err;
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		/* Tell the user that we could not find a usable */
		/* Winsock DLL.                                  */
		printf("WSAStartup failed with error: %d\n", err);
		return 1;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		return 1;
	}

	SOCKADDR_IN addrServ;
	addrServ.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	addrServ.sin_family = AF_INET;
	addrServ.sin_port = htons(4999);

	int length = sizeof(SOCKADDR);
	char recvBuf[BUFSIZ] = {};
	char sendheader[8] = { '0', '0', '0', '0', '0', '0', '0', '\0' };
	char recvheader[8] = { '0', '0', '0', '0', '0', '0', '0', '\0' };
	int rev = 0;

	SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);

	cout << "发送连接请求......" << endl;
	while (strcmp(recvBuf, beginData) != 0) {
		sendto(socketClient, beginData, BUFSIZ, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
		recvfrom(socketClient, recvBuf, BUFSIZ, 0, (SOCKADDR*)&addrServ, &length);
		Sleep(1000);
	}
	cout << "成功收到回应！发送消息确认......" << endl;
	sendto(socketClient, ok, BUFSIZ, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));  //发送确认消息


	while (true)
	{
		FILE* f = fh.selectfile();
		sendto(socketClient, ok, BUFSIZ, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
		
		strcpy(Filename, fh.getFileName());
		sendto(socketClient, Filename, BUFSIZ, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
		int count = 0;
		int sum = 0;
		while ((count = fread(sendData, 1, 512, f)) > 0)
		{
			if (sendheader[1] == '0' ) {
				printf("%d\n", sum += count);
				int che=check(sendData);
				if (che == 0) {
					sendheader[2] = '0';
				}
				else
				{
					sendheader[2] = '1';
				}
				//cout << sendData << endl;
				char* pkt = make_pkt(sendheader, sendData);
				//cout << pkt << endl;
				
				sendto(socketClient,pkt, 520, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
				clock_t now = clock();
				cout << "状态1，等待收到ACK" << endl;

				recvfrom(socketClient, recvheader, 8, 0, (SOCKADDR*)&addrServ, &length);
				
				while (recvheader[0] == '0' || recvheader[1] == '1') {
					sendto(socketClient, pkt, 520, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
					recvfrom(socketClient, recvheader, 8, 0, (SOCKADDR*)&addrServ, &length);
				}
				cout << "状态2，接受到ACK且序号为0的消息"<<endl;
				sendheader[1] = '1';
			}
			else
			{

				printf("%d\n", sum += count);
				int che = check(sendData);
				if (che == 0) {
					sendheader[2] = '0';
				}
				else
				{
					sendheader[2] = '1';
				}
				//cout << sendData << endl;
				char* pkt = make_pkt(sendheader, sendData);
				//cout << pkt << endl;
				sendto(socketClient, pkt, 520, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
				cout << "状态3，等待收到ACK" << endl;
				recvfrom(socketClient, recvheader, 8, 0, (SOCKADDR*)&addrServ, &length);
				
				while (recvheader[0] == '0' || recvheader[1] == '0') {
					sendto(socketClient, pkt, 520, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
					recvfrom(socketClient, recvheader, 8, 0, (SOCKADDR*)&addrServ, &length);
				}
				
				cout << "状态4，接受到ACK且序号为1的数据包"<<endl;
				sendheader[1] = '0';
			}
		}
		sendto(socketClient, overData, BUFSIZ, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
		
	}
	closesocket(socketClient);
	WSACleanup();
	return 0;

}


char* make_pkt(char header[8], char data[BUFSIZ])
{
	char *pkt=strcat(header,data);
	/*
		for (int i = 0; i < 8; i++)
		pkt[i] = header[i];
	for (int i = 8; i < 520; i++)
		pkt[i] = data[i-8];
	*/
	return pkt;
};
bool isACK(char* ch)
{
	if (ch[0] == '1' && ch[1] == '1')
		return true;
	else
		return false;

}


