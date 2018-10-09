#define FD_SETSIZE 3072
#include <winsock2.h>
#include <iostream>
#include <WS2tcpip.h>
#include <windows.h>
#include <time.h>
#include <queue>
#include <string>
#include <assert.h>
#include "func.h"

#define MAX_SOCKET 3072
#pragma comment(lib,"ws2_32.lib")
#define CLOCKS_PER_SEC ((clock_t)1000)  //时钟刻度（毫秒）
#define BUFLEN 1024    //缓冲区长度

using namespace std;

namespace bytes_helper
{
	template <class T> struct type {};
	template <typename T, class iter>
	T read(iter it) {
		T i = T();
		int T_len = sizeof(T);
		for (int idx = 0; idx < T_len; ++idx) {
			i |= *(it + idx) << (3 - idx) * 8;
		}
		return  i;              //返回的i是转换成功的值
	}

	template <typename T, class iter>
	void write(T v, iter it) {
		int T_len = sizeof(T);
		for (int idx = 0; idx < T_len; ++idx)
		{
			*(it + idx) = v >> (T_len - 1 - idx) * 8;//读入：小端转大端(it是转换成功的值，缓冲区首地址)
		}
	}
}

#pragma pack(push,1) 
struct Message
{
	int len;
	char content[1020] = "Hello,Server!";                 //string类型占40个字节
};
#pragma pack(pop)

int main(int argc, char *argv[])    //命令参数设置
{
	WSADATA wsadata;
	SOCKADDR_IN serverAddr;
	int count = 0;   //记录QPS
	int last_count = 0;//记录上一个阶段完成的count
	int msg_head = 0;

	char buf_send[BUFLEN] = { 0 };       //存储发送缓冲区的数据
	char client_recv_buffer[BUFLEN] = { 0 };             //存储接收缓冲区的数据
	char max_size_client[BUFLEN] = { 0 };             //临时记录接收缓冲区数据
	double excTime = 0.0;
	double conDuration = 0.0;     //记录实际经过时间
	int sendState;
	int times = 0;            //记录当前时间段

	Message sen2;
	sen2.len = strlen(sen2.content);

	memcpy(buf_send, &sen2, sen2.len + sizeof(int));   //把待发送的结构体拷贝到数组中
	bytes_helper::write<int>(sen2.len, buf_send);      //buf_send小端转大端

	cout << "Please input the excute time(seconds):";
	cin >> excTime;

	WSAStartup(MAKEWORD(2, 2), &wsadata);

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(atoi(argv[1]));   //Host to Net:小端转大端
	inet_pton(AF_INET, argv[2], &serverAddr.sin_addr.s_addr);     //IP地址
	
connect_again:
	int max_connect;
	printf("Please input the count of connect:");
	cin >> max_connect;
	SOCKET s[MAX_SOCKET] = {0};
	int connectState[MAX_SOCKET] = {0};
	memset(connectState,-1,MAX_SOCKET*sizeof(int));
	int i = 0;               //标志当前连接数
	while (i< max_connect) 
	{
		s[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		connectState[i] = connect(s[i], (SOCKADDR*)&serverAddr, sizeof(serverAddr));
		i++;
	}

	vector <int>socket_num;
	i = 0;   
	while (connectState[i] == 0)
	{
		socket_num.push_back(i);
		i++;
	}

	clock_t timeBegin = clock();   //记录开始时间
	clock_t stageBegin = timeBegin;     //记录每个阶段开始时间

		for (int k = 0; k < socket_num.size(); k++)     //先发一遍
		{
			bytes_helper::write<int>(sen2.len, buf_send);      //buf_send小端转大端
			sendState = send(s[socket_num[k]], buf_send, sen2.len + 4, 0);
			if (sendState == SOCKET_ERROR)
			{
				conDuration = (double)(clock() - timeBegin) / CLOCKS_PER_SEC;
				SendErrorDeal(conDuration, count);
				closesocket(s[socket_num[k]]);
				WSACleanup();
				continue;
			}	
		}

		int fd = 0;
		int can_read = 0;
		fd_set sets;
		fd_set temps;  //存储sets的值
		FD_ZERO(&sets);
		for (fd = 0; fd < socket_num.size(); fd++)
			FD_SET(s[socket_num[fd]], &sets);
		temps = sets;  
		struct timeval tv;
		while (1)
		{
			if ((double)(clock() - stageBegin) / CLOCKS_PER_SEC >= 5.0)   //每隔5秒，输出一次QPS
			{
				last_count = count - last_count;
				cout << times << "--" << times + 5 << "s ," << "the QPS value of server:" << last_count / 5.0 << endl;
				last_count = count;
				times += 5;
				stageBegin = clock();
			}

			if ((double)(clock() - timeBegin) / CLOCKS_PER_SEC >= excTime)      //超时，则跳出，并关闭连接。
			{
				for (int m = 0; m < socket_num.size(); m++)
					closesocket(s[socket_num[m]]);	
				goto outside;
			}

			sets = temps;   //通过复制的方式，重置sets集合（用户态到内核态的拷贝，每select（）一次，就重新拷贝一次）
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			can_read = select(socket_num.size()+ 1, &sets, NULL, NULL, &tv);       //非阻塞
		/*	if (can_read <= 0)
				continue;*/
			for (fd = 0; fd < socket_num.size(); fd++)
			{
				if (FD_ISSET(s[socket_num[fd]], &sets))
				{
					count++;    //总的收发次数
					int have_recv_idx = 0;
					int idx = 0;
					while (have_recv_idx < BUFLEN)                             //先检查是否读完len
					{
					read_again:
						idx = recv(s[socket_num[fd]], client_recv_buffer + have_recv_idx, BUFLEN - have_recv_idx, 0);
						msg_head = bytes_helper::read<int>(client_recv_buffer); //读出：大端转小端
						if (idx == SOCKET_ERROR)                                 //=0.断开；<0,错误。
						{
							conDuration = (double)(clock() - timeBegin) / CLOCKS_PER_SEC;
							RecErrorDeal(conDuration, count);
							memset(client_recv_buffer, 0, have_recv_idx);            //清空client_recv_buffer数组
							closesocket(s[socket_num[fd]]);
							goto connect_again;
						}
						else if (idx == 0)
						{
							cout << "Normally close the connection,the QPS value of client:" << (double)(count / excTime) << endl << endl;
							memset(client_recv_buffer, 0, have_recv_idx);            //清空client_recv_buffer数组
							closesocket(s[socket_num[fd]]);
							goto connect_again;
						}
						have_recv_idx += idx;
						if (have_recv_idx >= msg_head + sizeof(int))
						{
							memset(client_recv_buffer, 0, have_recv_idx);
							have_recv_idx = 0;
							msg_head = 0;
							break;
						}
						else
							goto read_again;
					}
					bytes_helper::write<int>(sen2.len, buf_send);      //buf_send小端转大端
					sendState = send(s[socket_num[fd]], buf_send, sen2.len + 4, 0);
					if (sendState == SOCKET_ERROR)
					{
						conDuration = (double)(clock() - timeBegin) / CLOCKS_PER_SEC;
						SendErrorDeal(conDuration, count);
						closesocket(s[socket_num[fd]]);
						goto connect_again;
					}
					FD_CLR(s[socket_num[fd]], &sets);
					if (--can_read == 0)
						break;
				}
			}	
		}

	outside:
		WSACleanup();
		system("pause");
		return 1;
}