#pragma comment(lib,"Ws2_32.lib")

#include<WinSock2.h>
#include<WS2tcpip.h>
#include<stdio.h>
#include<Windows.h>

#include"FileHelper.h"

int main()
{

	WORD wVersionRequested;
	WSADATA wsaData;
	char sendData[BUFSIZ] = "Send\n";
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
	while (true)
	{
		SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
		FILE* f = fh.selectfile();
		sendto(socketClient, beginData, BUFSIZ, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
		strcpy(Filename, fh.getFileName());
		sendto(socketClient, Filename, BUFSIZ, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
		int count = 0;
		int sum = 0;
		while ((count = fread(sendData, 1, BUFSIZ, f)) > 0)
		{
			Sleep(1);
			printf("%d\n", sum += count);
			sendto(socketClient, sendData, BUFSIZ, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
		}
		sendto(socketClient, overData, BUFSIZ, 0, (SOCKADDR*)&addrServ, sizeof(SOCKADDR));
		closesocket(socketClient);
	}
	WSACleanup();
	return 0;

}
