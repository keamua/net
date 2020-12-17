/*
 * udpserver.c - A UDP echo server
 * usage: udpserver <port_for_server>
 */
#pragma comment(lib,"Ws2_32.lib")
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<stdio.h>
#include<string>
#include<Windows.h>

#include<iostream>
using namespace std;


#include <stdlib.h>
#include <string.h>

#include <sys/types.h>


#include <fcntl.h>

#define BUFSIZE 1016
#define bzero(a, b)   memset(a, 0, b)
#define bcopy(a, b, n)   memcpy(a, b, n)


FILE* file;
char path_buffer[_MAX_PATH];
HANDLE handle;


typedef struct packet{
  int seq;
  int size;
  char data[BUFSIZE];
}packet;

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

packet dp_pkt(char *package)
{
	char pktseq[8];
	char pktsize[8];
	packet pkt;
	for (int i = 0; i < 8; i++)
		pktseq[i]=package[i] ;
	for (int i = 0; i < 8; i++)
		pktsize[i]=package[i+8];
	pkt.seq=atoi(pktseq);
	pkt.size=atoi(pktsize);

	for (int i = 0; i <pkt.size; i++)
		pkt.data[i]=package[i+16] ;
	
	return pkt;
}

int main(int argc, char **argv) {

	int portno; /* port to listen on 端口号*/
	
	char buf[BUFSIZE]; /* message buf 信息缓存*/
	

	int n,m; /* message byte size 信息数据*/
	int drp_prob; //丢包率
	/*
	* check command line arguments
	
	if (argc != 3) {
		fprintf(stderr, "usage: %s <port_for_server> <drop probability>\n", argv[0]);
		exit(1);
	}
	portno = atoi(argv[1]); 
	drp_prob = atoi(argv[2]); //转成int型
	*/

	drp_prob = 0; //转成int型

	WORD wVersionRequested;
	WSADATA wsaData;
 
	int err;
	wVersionRequested = MAKEWORD(2,2);
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
	SOCKET ServerSocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	SOCKADDR_IN addrServ;
	addrServ.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//指定0.0.0.0地址，表示任意地址
	addrServ.sin_family = AF_INET;//表示IPv4的套接字类型
	addrServ.sin_port = htons(9002);
	bind(ServerSocket,(SOCKADDR*)&addrServ,sizeof(SOCKADDR));
	SOCKADDR_IN addrClient;
	int length=sizeof(SOCKADDR);
	//clientlen = sizeof(clientaddr);
	//while (1)
	//{

	/*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);//初始化，缓存

	cout<<"等待连接......"<<endl;
	n = recvfrom(ServerSocket, buf, BUFSIZE, 0,(SOCKADDR*)&addrClient, &length);//接收名字信息
    //n = recvfrom(sockfd, buf, BUFSIZE, 0,(struct sockaddr *) &clientaddr, &clientlen); 
    if (n < 0) error("ERROR in recvfrom");
    printf("Reached here\n");
    printf("Contents of Buf : %s\n", buf);
   


    int i = 0;
    int first = 0, second, third;
    //char * strncpy ( char * destination, const char * source, size_t num );信息的三部分分割一下
    while(1)
    {
		if(buf[i] == '|' && first == 0) first = i;
		if(buf[i] == '|' && first !=0) second = i;
		if(buf[i] == '\0') {third = i;break;}
		i++;
    }
    
    //printf("%s\n",buf);
    printf("First : %d, Second : %d, third : %d\n",first,second,third);
    char *filename = new char[first+1];
    char *size = new char[second - first];
    char *chunks = new char [third - second];


    strncpy( filename, buf, first);
    filename[first] = '\0';
    printf("filename : %s\n",filename);

    strncpy ( size, buf+first+1, second-first-1);
    size[second-first] = '\0';
    printf("SIZE : %s\n",size);

    strncpy ( chunks, buf+second+1, third-second-1);
    chunks[third-second] = '\0';
    printf("CHUNKS : %s\n",chunks);

    int chunks_count = atoi(chunks);
    printf("chunks : %d\n",chunks_count); //打印信息。

    char fname[20]="new_";
    strcat(fname, filename);
    printf("Creating the copied output file : %s\n",fname);//给创建新文件的名字加上new_


    //printf("%s\n",fname);
    int fd, count = 0;//文件的指针，和文件的大小
	remove(fname);
	if (file = fopen(fname, "ab"))
	{
		printf("文件创建成功\n");
	}
	else
	{
		printf("文件创建失败\n");
		return 0;
	}
	
  	int seq_received, seq_last_ack; //接受到的序列号，和最后一个确认的ack。
  	//int m;

	packet buff;

	char recvbuf[1016];
	int sz = sizeof(packet);

	int chunks_counter = 0;
	printf("outside while \n");


	while(chunks_counter < chunks_count) //当没有接受到所有的包的时候
	{

		bzero(recvbuf,1016);
		n = recvfrom(ServerSocket,recvbuf,1016,0,(SOCKADDR*)&addrClient, &length);
		buff = dp_pkt(recvbuf);


		printf("Received %d\n",chunks_counter);
		seq_received = buff.seq; //收到的包的序号
		printf("sequence received is : %d\n",seq_received);
		if(chunks_counter > 0)//当不是第一个包的时候
		{
        	if (seq_received == seq_last_ack + 1) //接送到的序号和期待的那个包一样
        	{
				if((m = fwrite(buff.data,1,buff.size, file)) == -1)
				{
					perror("write fail");
					exit (6);
            }
            count = count + m; //文件大小加上
            bzero(buf,BUFSIZE);

            sprintf(buf,"%04d",seq_received);
			n = sendto(ServerSocket,buf,strlen(buf),0,(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
          	if(n < 0) perror("ERROR in sendto");
            else seq_last_ack = seq_received; //将最后一个确认收到的包更新
            chunks_counter++; //已经收到的包的个数增加
        	}

       		else
       		{
            //不是期待的那个包
       			bzero(buf,BUFSIZE);
       			sprintf(buf,"%04d",seq_last_ack);
				n = sendto(ServerSocket,buf,strlen(buf),0,(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
			 	if (n < 0) error("ERROR in sendto");
       		}
		}

		else//当是第一个包的时候
		{
			if((m = fwrite(buff.data,1,buff.size, file)) == -1)
		//  if((m = write(fd,buff->data,buff->size)) == -1)
            {
               perror("write fail");
               exit (6);
            }

            printf("write once done\n");
            count = count + m;
            bzero(buf,BUFSIZE);

            sprintf(buf,"%04d",seq_received);
			n = sendto(ServerSocket,buf,strlen(buf),0,(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
            if(n < 0) perror("ERROR in sendto");
            else seq_last_ack = seq_received;
            chunks_counter++;
      }

      printf("outside write once done\n");

   }

   
}
