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
#define CLOCKS_PER_SEC ((clock_t)1000)  //ʱ�ӿ̶ȣ����룩
#define BUFLEN 1024    //����������

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
		return  i;              //���ص�i��ת���ɹ���ֵ
	}

	template <typename T, class iter>
	void write(T v, iter it) {
		int T_len = sizeof(T);
		for (int idx = 0; idx < T_len; ++idx)
		{
			*(it + idx) = v >> (T_len - 1 - idx) * 8;//���룺С��ת���(it��ת���ɹ���ֵ���������׵�ַ)
		}
	}
}

#pragma pack(push,1) 
struct Message
{
	int len;
	char content[1020] = "Hello,Server!";                 //string����ռ40���ֽ�
};
#pragma pack(pop)

int main(int argc, char *argv[])    //�����������
{
	WSADATA wsadata;
	SOCKADDR_IN serverAddr;
	int count = 0;   //��¼QPS
	int last_count = 0;//��¼��һ���׶���ɵ�count
	int msg_head = 0;

	char buf_send[BUFLEN] = { 0 };       //�洢���ͻ�����������
	char client_recv_buffer[BUFLEN] = { 0 };             //�洢���ջ�����������
	char max_size_client[BUFLEN] = { 0 };             //��ʱ��¼���ջ���������
	double excTime = 0.0;
	double conDuration = 0.0;     //��¼ʵ�ʾ���ʱ��
	int sendState;
	int times = 0;            //��¼��ǰʱ���

	Message sen2;
	sen2.len = strlen(sen2.content);

	memcpy(buf_send, &sen2, sen2.len + sizeof(int));   //�Ѵ����͵Ľṹ�忽����������
	bytes_helper::write<int>(sen2.len, buf_send);      //buf_sendС��ת���

	cout << "Please input the excute time(seconds):";
	cin >> excTime;

	WSAStartup(MAKEWORD(2, 2), &wsadata);

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(atoi(argv[1]));   //Host to Net:С��ת���
	inet_pton(AF_INET, argv[2], &serverAddr.sin_addr.s_addr);     //IP��ַ
	
connect_again:
	int max_connect;
	printf("Please input the count of connect:");
	cin >> max_connect;
	SOCKET s[MAX_SOCKET] = {0};
	int connectState[MAX_SOCKET] = {0};
	memset(connectState,-1,MAX_SOCKET*sizeof(int));
	int i = 0;               //��־��ǰ������
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

	clock_t timeBegin = clock();   //��¼��ʼʱ��
	clock_t stageBegin = timeBegin;     //��¼ÿ���׶ο�ʼʱ��

		for (int k = 0; k < socket_num.size(); k++)     //�ȷ�һ��
		{
			bytes_helper::write<int>(sen2.len, buf_send);      //buf_sendС��ת���
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
		fd_set temps;  //�洢sets��ֵ
		FD_ZERO(&sets);
		for (fd = 0; fd < socket_num.size(); fd++)
			FD_SET(s[socket_num[fd]], &sets);
		temps = sets;  
		struct timeval tv;
		while (1)
		{
			if ((double)(clock() - stageBegin) / CLOCKS_PER_SEC >= 5.0)   //ÿ��5�룬���һ��QPS
			{
				last_count = count - last_count;
				cout << times << "--" << times + 5 << "s ," << "the QPS value of server:" << last_count / 5.0 << endl;
				last_count = count;
				times += 5;
				stageBegin = clock();
			}

			if ((double)(clock() - timeBegin) / CLOCKS_PER_SEC >= excTime)      //��ʱ�������������ر����ӡ�
			{
				for (int m = 0; m < socket_num.size(); m++)
					closesocket(s[socket_num[m]]);	
				goto outside;
			}

			sets = temps;   //ͨ�����Ƶķ�ʽ������sets���ϣ��û�̬���ں�̬�Ŀ�����ÿselect����һ�Σ������¿���һ�Σ�
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			can_read = select(socket_num.size()+ 1, &sets, NULL, NULL, &tv);       //������
		/*	if (can_read <= 0)
				continue;*/
			for (fd = 0; fd < socket_num.size(); fd++)
			{
				if (FD_ISSET(s[socket_num[fd]], &sets))
				{
					count++;    //�ܵ��շ�����
					int have_recv_idx = 0;
					int idx = 0;
					while (have_recv_idx < BUFLEN)                             //�ȼ���Ƿ����len
					{
					read_again:
						idx = recv(s[socket_num[fd]], client_recv_buffer + have_recv_idx, BUFLEN - have_recv_idx, 0);
						msg_head = bytes_helper::read<int>(client_recv_buffer); //���������תС��
						if (idx == SOCKET_ERROR)                                 //=0.�Ͽ���<0,����
						{
							conDuration = (double)(clock() - timeBegin) / CLOCKS_PER_SEC;
							RecErrorDeal(conDuration, count);
							memset(client_recv_buffer, 0, have_recv_idx);            //���client_recv_buffer����
							closesocket(s[socket_num[fd]]);
							goto connect_again;
						}
						else if (idx == 0)
						{
							cout << "Normally close the connection,the QPS value of client:" << (double)(count / excTime) << endl << endl;
							memset(client_recv_buffer, 0, have_recv_idx);            //���client_recv_buffer����
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
					bytes_helper::write<int>(sen2.len, buf_send);      //buf_sendС��ת���
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