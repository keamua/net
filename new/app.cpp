/*`
 * udpapp.c - A simple UDP app
 * usage: udpapp c <mode : 'c'> <port> <filename> if in "client_mode"
 * usage: udpapp s <mode : 's'> if in "server_mode"
 */
#pragma comment(lib,"Ws2_32.lib")
#include <WinSock2.h>
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <sys/wait.h>
#include <signal.h>
//#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
//#include <pthread.h>

#define SLEEP_VAL 2
#define BUFSIZE 1000
#define WIN_SZ 3
#define MSS 1024
#define MCS 1000
#define buffrrrsize 10
#define bzero(a, b)   memset(a, 0, b)
#define bcopy(a, b, n)   memcpy(a, b, n)


 //packet datastructure that the sender sends
typedef struct packet {
	int seq;
	int size;
	char data[MCS];
}packet;

char* mk_pkt(packet *pkt) //给数据打包，头文件标明，前8（0-7）位是序号，（8-11）4位是大小，后面是数据；
{
	char pktseq[8];
	sprintf(pktseq, "%d", pkt->seq);

	char pktsize[8];
	sprintf(pktsize, "%d", pkt->size);

	char* package = new char[16 + pkt->size];
	for (int i = 0; i < 8; i++)
		package[i] = pktseq[i];
	for (int i = 0; i < 8; i++)
		package[i + 8] = pktsize[i];
	for (int i = 0; i < pkt->size; i++)
		package[i + 16] = pkt->data[i];
	return package;
}

packet dp_pkt(char* package)
{
	char pktsize[8];
	char pktseq[8];
	packet pkt;
	for (int i = 0; i < 8; i++)
		pktseq[i] = package[i];
	for (int i = 0; i < 8; i++)
		pktsize[i] = package[i + 8];
	pkt.seq = atoi(pktseq);
	pkt.size = atoi(pktsize);
	for (int i = 0; i < pkt.size; i++)
		pkt.data[i] = package[i+16];
	return pkt;
}

//The ack send by the receiver is in this datastructure
typedef struct ack_packet {
	int size;
	int seq_number;
}ack_packet;


char* mk_ackpkt(ack_packet* pkt) //给数据打包，头文件标明，前8（0-7）位是序号，（8-11）4位是大小，后面是数据；
{
	char pktseq[8];
	sprintf(pktseq, "%d", pkt->seq_number);

	char pktsize[8];
	sprintf(pktsize, "%d", pkt->size);

	char* package = new char[16];
	for (int i = 0; i < 8; i++)
		package[i] = pktsize[i];
	for (int i = 0; i < 8; i++)
		package[i + 8] = pktseq[i];

	return package;
}
ack_packet dp_ackpkt(char* package)
{
	char pktsize[8];
	char pktseq[8];
	
	ack_packet pkt;
	for (int i = 0; i < 8; i++)
		pktsize[i] = package[i];
	for (int i = 0; i < 8; i++)
		pktseq[i] = package[i + 8];
	pkt.size = atoi(pktsize);
	pkt.seq_number = atoi(pktseq);

	return pkt;
}

//The datastucture for the receiver buffer
typedef struct recv_buff {
	char buff[MCS];
	int flag;//flag denoting whether the item in the array is read or not
}recv_buff;



typedef struct send_buff {
	char buff[MCS];
	int flag;//flag denoting whether the item in the array is read or not
}send_buff;


//The first packet sent by the sender
typedef struct inform_packet {
	int file_size;
	int num_chunks;
	char file_name[100];
}inform_packet;

char* mk_infopkt(inform_packet *pkt) //给数据打包，头文件标明，前8（0-7）位是序号，（8-11）4位是大小，后面是数据；
{
	char pktseq[8];
	sprintf(pktseq, "%d", pkt->file_size);

	char pktsize[8];
	sprintf(pktsize, "%d", pkt->num_chunks);

	char* package = new char[116];
	for (int i = 0; i < 8; i++)
		package[i] = pktseq[i];
	for (int i = 0; i < 8; i++)
		package[i + 8] = pktsize[i];
	for (int i = 0; i < 100; i++)
		package[i + 16] = pkt->file_name[i];
	return package;
}

inform_packet dp_infopkt(char* package)
{
	char pktsize[8];
	char pktseq[8];
	inform_packet pkt;
	for (int i = 0; i < 8; i++)
		pktsize[i] = package[i];
	for (int i = 0; i < 8; i++)
		pktseq[i] = package[i + 8];
	pkt.file_size = atoi(pktsize);
	pkt.num_chunks = atoi(pktseq);
	for (int i = 16; i < 116; i++)
		pkt.file_name[i - 16] = package[i];
	return pkt;
}

send_buff  sender_buff[buffrrrsize];//sender buffer
recv_buff receiver_buff[buffrrrsize];//receiver buffer
int send_wrt_ptr = 0, u = 0, go_ackmanager = 0; // tells the send_helper function where to write in the sender buffer
int send_rd_ptr = 0; // tells the send_helper function where to read from the sender buffer
int chunks, lastpack_sz_sender, lastpack_sz_recvr;//chunks : total chunks to be send by the sender. lastpack_sz_sender : size of last packet sent by sender.
int recv_rd_ptr = -1, sender_win, serverlen, clientlen;//The read ptr location in the receiver buffer. sender_win : size of sender win.
int recv_file_sz, chunks_toberecvd, count = 0;//recv_file_sz : file size variable for the receiver. chunks_toberecvd : no. of chunks variable at the receiver.
char filename[100], file_tbt[100];//filename : name of file received by the receiver. file_tbt : name of the file sent by the sender.
SOCKET sender_sock, recv_sock;
SOCKADDR_IN serveraddr;
SOCKADDR_IN srvraddr, clientaddr;
struct hostent* server;
char* hostname;
float cwnd;
int recvr_win;
int ack_ptr = -1, bs_ptr = -1, bs_ptr_help;
int go_appreceive = 0;
inform_packet* fp;
HANDLE apprecv_thread;
HANDLE receiver_thread;
int recvbuff_currsize = buffrrrsize;

int flag = 0, x = 0;



// time_t start_time;

static int alarm_fired = 0;


DWORD WINAPI  appsend(void* param);
void  send_handler(char* data);
DWORD WINAPI  tranmssn_ctrl(void* param);
packet* packet_maker(int packet_count);
DWORD WINAPI  ack_manager(void* param);
//int min(int a, int b);
DWORD WINAPI  apprecv(void* param);
DWORD WINAPI  receiver(void* param);

void error(char* msg) {
	perror(msg);
	exit(0);
}

/*
void mysig(int sig)
{
	pid_t pid;
	printf("PARENT : Received signal %d \n\n", sig);
	if (sig == SIGALRM)
	{
		printf("signal llll\n\n");
		alarm_fired = 1;
		flag = 0;
	}
}
*/


int main(int argc, char** argv)
{


	int  portno,  i;
	serverlen = sizeof(serveraddr);
	clientlen = sizeof(clientaddr);

	if (argc != 4 && argc != 2)
	{
		fprintf(stderr, "usage: %s <hostname> <port> <filename>\n\n", argv[0]);
		fprintf(stderr, "usage: %s <port>\n\n", argv[0]);
		exit(0);
	}

	if (argc == 4)
	{
		WORD wVersionRequested;
		WSADATA wsaData;

		int err;
		wVersionRequested = MAKEWORD(2, 2);
		err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0)
		{
			printf("WSAStartup failed with error:%d\n", err);
			return -1;
		}
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
		{
			printf("Could not find a usable version of Winsock.dll\n");
			WSACleanup();
			return -1;
		}
		hostname = argv[1];
		portno = atoi(argv[2]);
		strcpy(file_tbt, argv[3]);

		for (i = 0; i < buffrrrsize; i++) sender_buff[i].flag = 0;

		sender_sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sender_sock < 0)
			printf("ERROR opening socket\n");

		server = gethostbyname(hostname);
		if (server == NULL)
		{
			printf("ERROR, no such host as %s\n\n", hostname);
			exit(0);
		}

		printf("sending file to %s\n\n", server->h_name);

		bzero((char*)&serveraddr, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		bcopy((char*)server->h_addr, (char*)&serveraddr.sin_addr.s_addr, server->h_length);
		serveraddr.sin_port = htons(portno);

		HANDLE sender;
		if (((sender = CreateThread(NULL, NULL, appsend, NULL, NULL, NULL))) == NULL)
		{
			printf("error in thread creation\n");
			int x=GetLastError();
			printf("%d\n",x);
			return EXIT_FAILURE;
		}

		HANDLE window_control;
		if (((window_control = CreateThread(NULL, NULL, tranmssn_ctrl, NULL , NULL, NULL))) == NULL)
		{
			printf("error in thread creation\n");
			return EXIT_FAILURE;
		}


		HANDLE ack_thread;
		if (((ack_thread = CreateThread(NULL, NULL, ack_manager, NULL,NULL,NULL))) == NULL)
		{
			printf("error in thread creation\n");
			return EXIT_FAILURE;
		}

		printf("All threads created succesfully\n\n");

		while (1);

	}

	else
	{
		portno = atoi(argv[1]);
		WORD wVersionRequested;
		WSADATA wsaData;

		int err;
		wVersionRequested = MAKEWORD(2, 2);
		err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0)
		{
			printf("WSAStartup failed with error:%d\n", err);
			return -1;
		}
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
		{
			printf("Could not find a usable version of Winsock.dll\n");
			WSACleanup();
			return -1;
		}

		recv_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (recv_sock < 0)
			printf("ERROR opening socket\n");

		//bzero((char*)&srvraddr, sizeof(srvraddr));
		srvraddr.sin_family = AF_INET;
		srvraddr.sin_addr.s_addr = htonl(INADDR_ANY);
		srvraddr.sin_port = htons(portno);
		
		if (bind(recv_sock, (SOCKADDR*)&srvraddr, sizeof(SOCKADDR)) != 0)
			printf("ERROR on binding\n");

		int i;
		for (i = 0; i < buffrrrsize; i++) receiver_buff[i].flag = 0;



		if (((receiver_thread = CreateThread(NULL, NULL, receiver, NULL,NULL,NULL))) == NULL)
		{
			printf("error in thread creation\n");
			return EXIT_FAILURE;
		}



		if (((apprecv_thread = CreateThread(NULL, NULL, apprecv, NULL,NULL,NULL))) == NULL)
		{
			printf("error in thread creation\n");
			return EXIT_FAILURE;
		}

		printf("Threads created succesfully\n");

		while (1);



	}


	return 0;
}


DWORD WINAPI appsend(void* param)
{

	printf("<<appsend>>\tInside app send !!\n\n");

	int bytes_read, z = 0, n;
	int bytes_sent;
	//struct stat st;
	int size;
	char temp_buff[MCS];
	FILE* fd;

	lastpack_sz_sender = 0;

	if (!(fd = fopen(file_tbt, "rb")))
	{
		printf("open fail");
		exit(0);
	}

	struct _stat st;
	int result;
	result = _stat(file_tbt, &st);
	if (result != 0)
	{
		perror("Problem getting information");
		return 0;
	}

	size = st.st_size;
	printf("<<appsend>>\tSize = %d\n\n", size);
	chunks = size / (MCS);
	lastpack_sz_sender = size % MCS;
	if (lastpack_sz_sender > 0) chunks++;
	else lastpack_sz_sender = MCS;

	fp = (inform_packet*)malloc(sizeof(inform_packet));
	fp->file_size = size;
	fp->num_chunks = chunks;
	strcpy(fp->file_name, file_tbt);
	u = 1;

	while (1)
	{
		bzero(temp_buff, MCS);
		if (z != chunks - 1)
		{
			n = fread(temp_buff, 1, MCS, fd);
			//n = read(fd, temp_buff, MCS);
			send_handler(temp_buff);
		}
		else
		{
			n = fread(temp_buff, 1, lastpack_sz_sender, fd);
			//n = read(fd, temp_buff, lastpack_sz_sender);
			send_handler(temp_buff);
			break;
		}
		z++;
	}
	return 0;
}

void send_handler(char* data)
{
	while (sender_buff[(send_wrt_ptr) % buffrrrsize].flag == 1);
	bzero(sender_buff[send_wrt_ptr].buff, MCS);
	memcpy(sender_buff[send_wrt_ptr].buff, data, strlen(data));
	sender_buff[send_wrt_ptr].flag = 1;

	send_wrt_ptr = (send_wrt_ptr + 1) % buffrrrsize;
	printf("<<send_handler>>\t send_wrt_ptr = %d\n\n", send_wrt_ptr);

}


DWORD WINAPI tranmssn_ctrl(void* param)
{
	while (u == 0);
	printf("<<tranmssn_ctrl>>\tinside tranmission ctrl\n\n");
	int sz = sizeof(packet);
	cwnd = 3;
	recvr_win = buffrrrsize;
	sender_win = min((int)cwnd, recvr_win);
	int packet_count = 0;
	int bytes_sent, i;
	bs_ptr_help = 0;

	bytes_sent = sendto(sender_sock, mk_infopkt(fp), 116, 0, (struct sockaddr*)&serveraddr, serverlen);
	if (bytes_sent < 0) printf("ERROR in sendto");

	while (ack_ptr < chunks - 1)
	{
		// if(recvr_win == 0) sender_win = 3;
		while (bs_ptr - ack_ptr < sender_win && bs_ptr < chunks - 1 && sender_buff[bs_ptr_help].flag == 1)
		{

			//	while( bs_ptr_help == send_wrt_ptr );
			packet* pk = packet_maker(packet_count);
			// printf("%s\n\n\n\n",pk->data);
			bytes_sent = sendto(sender_sock, mk_pkt(pk), 1016, 0, (struct sockaddr*)&serveraddr, serverlen);
			if (bytes_sent < 0) perror("ERROR in sendto");


			//printf("\n\n\n\n%s\n\n\n",pk->data);
			//printf("\n%d\n\n\n", pk->size);



			go_ackmanager = 1;
			bs_ptr++;
			bs_ptr_help = (bs_ptr_help + 1) % buffrrrsize;
			packet_count++;
			printf("<<tranmssn_ctrl>>\tbase_ptr = %d, ack_ptr = %d\n", bs_ptr, ack_ptr);
			printf("<<tranmssn_ctrl>>\tcwnd = %f\n", cwnd);
			printf("<<tranmssn_ctrl>>\trecvr_win = %d\n", recvr_win);
			if ((int)cwnd < recvr_win) sender_win = (int)cwnd;
			else sender_win = recvr_win;
			printf("<<tranmssn_ctrl>>\tsender win = %d\n\n", sender_win);
		}
	}
	return 0;
	// signal(SIGUSR1,sig_handler);

}

packet* packet_maker(int packet_count)
{
	int sz;
	packet* new_packet = (packet*)malloc(sizeof(packet));
	if (packet_count < chunks - 1) sz = MCS;
	else sz = lastpack_sz_sender;

	new_packet->seq = packet_count;
	new_packet->size = sz;

	memcpy(new_packet->data, sender_buff[bs_ptr_help].buff, sz);
	//printf("\n\n\n\n%s\n\n\n",new_packet->data);
	//printf("\n%d\n\n\n", new_packet->size);
	printf("<<packet>>\t send_rd_ptr = %d, bs_ptr_help = %d\n\n", send_rd_ptr, bs_ptr_help);
	return new_packet;
}



DWORD WINAPI ack_manager(void* param)
{
	int recv_flag = 1, temp;
	int bytes_recv, dupack_count = 0, seq_recvd;
	int sz_rcvpk = sizeof(ack_packet);
	ack_packet* recv_pk = (ack_packet*)malloc(sizeof(ack_packet));
	char recv_pk_buff[16];
	int prev_seq = -1;
	int ssthresh = INT_MAX;

	while (go_ackmanager == 0);

	while (ack_ptr < chunks - 1)
	{

		//alarm(SLEEP_VAL);
		//(void)signal(SIGALRM, mysig);
		do
		{
			bzero(recv_pk, sz_rcvpk);
			bzero(recv_pk_buff, 16);
			if (recv_flag) bytes_recv = recvfrom(sender_sock, recv_pk_buff, 16, 0, (struct sockaddr*)&serveraddr, &serverlen);
			*recv_pk = dp_ackpkt(recv_pk_buff);
			if (bytes_recv > 0)
			{
				if (recv_flag)
				{
					seq_recvd = recv_pk->seq_number;
					recvr_win = recv_pk->size;
					printf("<<ack_manager>>  \tseq_recvd through ack = %d\n", seq_recvd);
				}

				else recv_flag = 1;
				if (seq_recvd >= prev_seq + 1)
				{
					// printf("<<ack_manager>>\tseq_recvd>=prev_seq+1\n");
					ack_ptr = ack_ptr + (seq_recvd - prev_seq);
					temp = (send_rd_ptr + (seq_recvd - prev_seq)) % buffrrrsize;
					while (send_rd_ptr != temp)
					{
						sender_buff[send_rd_ptr].flag = 0;
						send_rd_ptr = (send_rd_ptr + 1) % buffrrrsize;
					}

					printf("<<ack_manager>> \tack_ptr = %d, send_rd_ptr = %d\n", ack_ptr, send_rd_ptr);
					if (sender_win <= ssthresh)
					{
						//printf("<<ack_manager>>\tsender_win <= ssthresh\n\n");
						cwnd += (seq_recvd - prev_seq);
						printf("<<ack_manager>> \tcwnd = %f\n\n", cwnd);
					}

					else
					{
						float increase = (float)(seq_recvd - prev_seq) / (float)((int)cwnd);
						cwnd += increase;
					}

					prev_seq = seq_recvd;
				}

				else
				{
					dupack_count++;
					if (dupack_count == 3)
					{
						bs_ptr = ack_ptr;

						ssthresh = sender_win / 2;
						if (ssthresh < 3) ssthresh = 3;
						sender_win = 3;
						flag = 1;
						alarm_fired = 1;
					}
				}
			}

		} while (!alarm_fired);



		if (flag == 0)
		{
			bs_ptr = ack_ptr;
			if (sender_win > 3) sender_win /= 2;
		}


		// if(flag==1)
		// {
		while (seq_recvd != prev_seq)
		{
			printf("<<ack_manager>>\tInside lower while\n");
			printf("<<ack_manager>>\tcwnd : %f, sender_win : %d, recv_win : %d\n\n", cwnd, sender_win, recvr_win);

			prev_seq = seq_recvd;
			bzero(recv_pk, sz_rcvpk);
			bzero(recv_pk_buff, 16);
			bytes_recv = recvfrom(sender_sock, recv_pk_buff, 16, 0, (struct sockaddr*)&serveraddr, &serverlen);
			*recv_pk = dp_ackpkt(recv_pk_buff);
			seq_recvd = recv_pk->seq_number;
			recv_flag = 0;
			recvr_win = recv_pk->size;
		}
		// }
	}
	return 0;
}


DWORD WINAPI  apprecv(void* param)
{
	while (go_appreceive == 0);
	//printf("Inside apprecv got a go from receiver\n\n");
	int  m, sz;
	FILE* fd;
	char fname[20];
	sprintf(fname, "new_");
	printf("<<<apprecv>> \tfile name = %s\n\n", filename);
	strcat(fname, filename);

	int chunks_count = 0;

	if (fd = fopen(fname, "ab"))
	{
		printf("open success!");
		//exit(3);
	}

	printf("<<<apprecv>> \tCreated %s succesfully\n\n", fname);

	while (chunks_count < chunks_toberecvd)
	{
		// printf("<<<apprecv>> before busy while\n\n");
		while (x == 0);
		// printf("<<<apprecv>> after busy while\n\n");
		count = 0;
		// printf("<<<apprecv>> \tCount inside apprecv before update= %d\n\n",count);


		if (chunks_count < chunks_toberecvd - 1) sz = MCS;
		else sz = lastpack_sz_recvr;
		count = 0;
		printf("<<<apprecv>> \treceive read pointer : %d\n", recv_rd_ptr);


		// pthread_mutex_lock(&lock);
		// pthread_mutex_unlock(&lock);

		// printf("<<<apprecv>> \treceiver_buff[recv_rd_ptr+1].flag = %d\n", receiver_buff[recv_rd_ptr+1].flag);
		while (receiver_buff[(recv_rd_ptr + 1) % buffrrrsize].flag == 1)
		{
			if (chunks_count < chunks_toberecvd - 1) sz = MCS;
			else sz = lastpack_sz_recvr;
			recv_rd_ptr = (recv_rd_ptr + 1) % buffrrrsize;
			count++;
			// printf("<<<apprecv>> \treceiver_buff[recv_rd_ptr+1].flag = %d\n", receiver_buff[recv_rd_ptr+1].flag);
			// printf("count : %d\n", count);
			// printf("%s\n\n\n\n", receiver_buff[recv_rd_ptr].buff);
			// printf("%d\n\n", buf->size);m = write(fd, receiver_buff[recv_rd_ptr].buff, sz
			
			if ((m = fwrite(receiver_buff[recv_rd_ptr].buff, 1, sz, fd)) == -1)
			{
				perror("write fail");
				exit(6);
			}
			recvbuff_currsize++;
			if (chunks_count == chunks_toberecvd) printf("%d", sz);
			receiver_buff[recv_rd_ptr].flag = 0;
			chunks_count++;

		}
		// printf("<<<apprecv>> \tCount inside apprecv after update= %d\n\n",count);
		x = 0;
	}

	//close(fd);
	return 0;
	exit(0);
}



DWORD WINAPI  receiver(void* param)
{
	printf("<<<receiver>> \tinside receiver\n\n");

	int last_ack = -1, seq_recvd;
	int pos;

	inform_packet* buff = (inform_packet*)malloc(sizeof(inform_packet));
	char recv_info_buff[116];
	packet* buf = (packet*)malloc(sizeof(packet));
	char pkt_buf[1016];
	ack_packet* ack = (ack_packet*)malloc(sizeof(ack_packet));
	int sz = sizeof(ack_packet);
	bzero(recv_info_buff, 116);
	bzero(buff, sizeof(inform_packet));
	int m, n;
	n = recvfrom(recv_sock, recv_info_buff, 116, 0, (SOCKADDR*)&clientaddr, &clientlen);
	*buff = dp_infopkt(recv_info_buff);
	printf("n = %d\n", n);
	strcpy(filename, buff->file_name);
	recv_file_sz = buff->file_size;
	chunks_toberecvd = buff->num_chunks;
	lastpack_sz_recvr = recv_file_sz % MCS;
	if (lastpack_sz_recvr == 0) lastpack_sz_recvr = MCS;

	printf("<<<receiver>> \treceiving %s of size %d comprising of %d chunks\n\n", filename, recv_file_sz, chunks_toberecvd);

	go_appreceive = 1;

	int chunks_count = 0;
	while (chunks_count < chunks_toberecvd)
	{
		// printf("<<<receiver>> inside while of receiver\n\n");
		bzero(pkt_buf, 1016);
		bzero(buf, sizeof(packet));
		n = recvfrom(recv_sock, pkt_buf, 1016, 0, (struct sockaddr*)&clientaddr, &(clientlen));
		*buf = dp_pkt(pkt_buf);

		if (n < 0) perror("ERROR in recvfrom");


		//printf("\n\n\n\n\n-------------%s\n\n\n\n",buf->data);
		// printf("%d\n\n", buf->size);
		seq_recvd = buf->seq;
		printf("<<<receiver>> \tseq_received is : %d\n", seq_recvd);
		printf("<<<receiver>> \tlast ack = %d\n\n", last_ack);
		if (seq_recvd > last_ack)
		{
			//printf("sequence received is greater than prev ack\n\n");
			int prev_read_ptr = recv_rd_ptr;
			if (seq_recvd - last_ack <= recvbuff_currsize)
			{

				pos = (recv_rd_ptr + (seq_recvd - last_ack)) % buffrrrsize;
				bzero(receiver_buff[pos].buff, MCS);
				memcpy(receiver_buff[pos].buff, buf->data, buf->size);
				//printf("%s\n\n\n\n",receiver_buff[pos].buff);
				printf("\n\nchunks_count = %d\n\n", chunks_count);
				//printf("%s\n\n", receiver_buff[pos].buff);
				chunks_count++;
				x = 1;
				receiver_buff[pos].flag = 1;
				recvbuff_currsize--;
				// printf("<<<receiver>> before busy while\n\n");
				while (x == 1);
				// printf("<<<receiver>> after busy while\n\n");
				last_ack += count;
				// printf("<<<receiver>> \tCount  = %d\n",count);
				bzero(ack, sizeof(ack_packet));
				ack->size = recvbuff_currsize;
				ack->seq_number = last_ack;
				m = sendto(recv_sock, mk_ackpkt(ack), 16, 0, (struct sockaddr*)&clientaddr, (clientlen));
				// printf("<<<receiver>> \tseq_recvd >= last ack\n");
				printf("<<<receiver>> \tAck sent  = %d\n", last_ack);
			}




		}

		else
		{
			bzero(ack, sizeof(ack_packet));
			ack->size = recvbuff_currsize;
			ack->seq_number = last_ack;
			m = sendto(recv_sock, mk_ackpkt(ack), 16, 0, (struct sockaddr*)&clientaddr, (clientlen));
			// printf("<<<receiver>> \tseq_recvd < last ack\n");
			printf("<<<receiver>> \tAck sent  = %d\n", last_ack);
		}

	}
	return 0;
}
