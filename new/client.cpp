/*
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#pragma comment(lib,"Ws2_32.lib")
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include <sys/stat.h>
#include <WinSock2.h>
#include <Windows.h>


#include <time.h>


#define SLEEP_VAL 5
#define BUFSIZE 1000
#define WIN_SZ 3
#define bzero(a, b)   memset(a, 0, b)
#define bcopy(a, b, n)   memcpy(a, b, n)

typedef struct packet{
	int seq;
	int size;
	char data[BUFSIZE];
}packet; //包结构体，包括序号，大小和数据


static int alarm_fired = 0;//全局静态变量检测超时
bool sig = false; //检测超时


FILE* file;
char path_buffer[_MAX_PATH];
HANDLE handle;


DWORD WINAPI timer(LPVOID lparam)
{
	while(1){
		Sleep(5000);
		if(sig)
			alarm_fired = 1;
	}
}
/*
 * error - wrapper for perror 错误的包
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

char * mk_pkt(packet pkt) //给数据打包，头文件标明，前8（0-7）位是序号，（8-11）4位是大小，后面是数据；
{
	char pktseq[8];
	sprintf(pktseq,"%d",pkt.seq);

	char pktsize[8];
	sprintf(pktsize,"%d",pkt.size);

	char* package =new char [16 + pkt.size];
	for (int i = 0; i < 8; i++)
		package[i]=pktseq[i] ;
	for (int i = 0; i < 8; i++)
		package[i+8]=pktsize[i];
	for (int i = 0; i < pkt.size; i++)
		package[i+16]=pkt.data[i] ;
	return package;
}


int main(int argc, char **argv)
{
    int sockfd, portno, n;  //socket，端口 ，或者用来储存返回值
    int serverlen;  //服务器的长度
	int bytes_recv;  //接受到信息多长
	
    char buf1[BUFSIZE], buf_ack[BUFSIZE]; //两个缓存，一个缓存数据，另一个缓存接受到的ack。
	

	WSADATA wsaData = { 0 };//存放套接字信息
	SOCKET ClientSocket = INVALID_SOCKET;//客户端套接字
	SOCKADDR_IN ServerAddr ;//服务端地址
	USHORT uPort ;//服务端端口
	int length = sizeof(SOCKADDR);


    int sz = sizeof(packet); //包结构的大小
	/*
    if (argc != 4)
    {
       fprintf(stderr,"usage: %s <hostname> <port> <filename>\n", argv[0]);
       exit(0);
    }//输入 ，服务器ip，端口号，传送文件的名字，来发送
    //hostname = argv[1]; 
    portno = atoi(argv[2]);
	*/

	//初始化套接字
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		printf("WSAStartup failed with error code: %d\n", WSAGetLastError());
		return -1;
	}
	//判断套接字版本
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("wVersion was not 2.2\n");
		return -1;
	}
	//创建套接字
	ClientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ClientSocket == INVALID_SOCKET)
	{
		printf("socket failed with error code: %d\n", WSAGetLastError());
		return -1;
	}

	
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(9002);//服务器端口
	ServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//服务器地址

    
    printf("Done till here\n");//起码完成了建立连接
    bzero(buf1, BUFSIZE);
	char filename[]="helloworld.txt";

    int fd;
	if (file = fopen(filename, "rb"))
	{
			printf("文件打开成功\n");
	}
	else
	{
			printf("文件不存在，请重新输入\n");
			return EXIT_FAILURE;
	}

	struct _stat buf;
    int result;
    char timebuf[26];
	result = _stat(filename, &buf);
	if (result != 0)
    {
        perror("Problem getting information");
		return 0;
	}

    int lastpack_sz = 0, bytes_sent; //最后一个包的大小，和发送的字节
    //struct stat st; // st stat结构体是用来储存文件的属性的
    //fstat(fd, &st); 
    int size = buf.st_size; //文件大小
    printf("Size = %d\n",size);
    int chunks = size/1000; //要发送多少个包，向下取整
    if(size > chunks*1000) //最后一个包的大小
    {
        lastpack_sz = size-(chunks*1000);
        chunks += 1;
    }
    printf("Chunks = %d\n",chunks);

    char chunks_arr[10], size_arr[10];
    sprintf(size_arr,"%d",size); //把大小转化成char储存

    printf("%s\n",size_arr);

    sprintf(chunks_arr,"%d",chunks);//把包的数目转化成字符串
    printf("%s\n",chunks_arr);

    strcpy(buf1, filename);
    buf1[strlen(filename)] = '|';
    strcat(buf1, size_arr);
    printf("buff after size_concat : %s\n",buf1);

    buf1[strlen(filename)+strlen(size_arr)+1] ='|';
    strcat(buf1,chunks_arr);
    printf("buff after chunks_concat : %s\n",buf1);

    buf1[strlen(filename)+strlen(size_arr)+strlen(chunks_arr)+2] = '\0';
    serverlen = sizeof(ServerAddr);
    printf("Sending %s to server\n", buf1); //应该是输出文件信息，名字+大小+要发送的包的数目

	bytes_sent = sendto(ClientSocket,buf1, strlen(buf1), 0, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
    //bytes_sent = sendto(sockfd, buf1, strlen(buf1), 0, (struct sockaddr *)&serveraddr, serverlen); //将信息发送个服务器端
    bzero(buf1,BUFSIZE); //再将缓冲数组清零

    int sequence_number =0, chunks_transferred = 0; //序号，和已经发送的包的大小
	int temp_sz = 1000; //目前的大小
	int bs_ptr=-1; 
	int ack_ptr = -1;
	int seq_received ;
	int nNetTimeout = 1000;
	int f;
	int new_winsz, win_size=3;
    packet *buff= new packet[chunks]; //包的数组
	//char *buffc= new char[chunks];
    int z=0;
    for(z=0;z<chunks;z++)
    {
    	//buff[z] = (packet*)malloc(1*sizeof(packet));
    	//bzero(buff[z], sz);
		buff[z].seq = z;
		if(z != chunks-1)
        {
            buff[z].size = 1000;
			n = fread(buff[z].data, 1, 1000, file);
            //n=read(fd,buff[z].data,1000);
        }
        else
        {
            buff[z].size = lastpack_sz;
			n = fread(buff[z].data, 1, lastpack_sz, file);
            //n= read(fd,buff[z].data,lastpack_sz);
        }
    }//把所有数据初始化好

	/*
    for(z=0;z<chunks;z++)
    {
    	printf("seq[%d] = %d\n",z,buff[z].seq);
    }
	

	if(setsockopt(ClientSocket, SOL_SOCKET, SO_RCVTIMEO,(char*)&nNetTimeout, sizeof(int)) < 0)
	{
		perror("fail to setsockopt");
	}
*/
    //把每个包的序号打印出来
    while(ack_ptr != chunks-1) //当包还没有发送完的时候
    {
		int i=0;
		while(i < win_size)  //连续发送3个包
		{
			bytes_sent = sendto(ClientSocket,mk_pkt(buff[ack_ptr+1+i]), 1016, 0, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
			//bytes_sent = sendto(sockfd, buff[ack_ptr+1+i], sz, 0, (struct sockaddr *)&serveraddr, serverlen);
			printf("sending chunk %d\n",ack_ptr+1+i);
			printf("bytes_sent = %d\n",bytes_sent);
			if (bytes_sent < 0) error("ERROR in sendto");
			else bs_ptr++; //用来标记已经判断发送的包
			i++;
		}


		while(bs_ptr-ack_ptr == win_size) //当已发送的包 - 已经确认收到的包的为 窗口大小
		{
			f = 0;
			sig =true;
			handle = CreateThread(NULL, NULL, timer, 0, 0, 0);

			//alarm(SLEEP_VAL);
			//(void) signal(SIGALRM, mysig); //定时器
			do
			{	bzero(buf_ack,BUFSIZE);
				bytes_recv = recvfrom(ClientSocket, buf_ack, BUFSIZE, 0, (SOCKADDR*)&ServerAddr, &length);
				//bytes_recv = recvfrom(sockfd, buf_ack, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen); //接受确认序号
				printf("Received  ack\n");
				int initial_ack = ack_ptr; //当确认到的序号
				if(bytes_recv > 0 )
				{
					seq_received = atoi(buf_ack); //接受到的ack包里面的序号，用来核对
					int i = 0;
					while(i < win_size)
					{
						if(seq_received == initial_ack+1+i); //当收到的ack包和未确认的包的第一个相同时
						{
							printf("Ack matched with %d\n",initial_ack+1+i); //这个包匹配上了
							ack_ptr = initial_ack + i+1; //确认的包的个数增加
							printf("Ack_ptr = %d\n",ack_ptr);
							f = 1;
							sig = false;
							alarm_fired = 1;
							new_winsz = win_size + i + 1; //增加窗口大小进行拥塞控制
						}
						i++;
					}	
				}

			}while(!alarm_fired);  //没有超时的时候

			if(f == 0) //此时说明没有收到任何一个正确的ack
			{
				int i=0;
				win_size = win_size/2; //说明发生了拥塞，减少一半窗口发送的包的个数，来控制
				bs_ptr = ack_ptr; //返回到原来的
				while(i<win_size)
				{
					bytes_sent = sendto(ClientSocket,mk_pkt(buff[i]), sz, 0, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
					//bytes_sent = sendto(sockfd, buff[i], sz, 0, (struct sockaddr *)&serveraddr, serverlen); //在新的窗口下重新发送当前没有发送成功的包
					bs_ptr++;
					i++;
				}
			}
		}	
    }
    return 0;
}
