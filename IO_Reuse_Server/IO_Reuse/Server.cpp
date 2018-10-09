// Multi_connect_Server.cpp : 定义控制台应用程序的入口点。
//

//#include "stdafx.h"
#define FD_SETSIZE 3072
#include <WINSOCK2.H>
#include <WS2tcpip.h>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <ctime>
#include <cassert>
#include<iterator>

#pragma comment(lib,"ws2_32.lib")
using namespace std;

#define  CLOCKS_PER_SEC ((clock_t)1000)

struct client
{
	int port;
	char* IP_addr;
	int sockfd;
};

int current_total = 0; //the total packet received

namespace bytes_helper {
	template <class T> struct type {};

	template <typename T, class iter>
	T read(iter it, type<T>) {
		T i = T();
		//[01][02][03][04]
		int T_len = sizeof(T);
		for (int idx = 0; idx < T_len; ++idx) {
			i |= *(it + idx) << (3 - idx) * 8;
		}
		return  i;
	}

	template <typename T, class iter>
	int write(T v, iter it, int size) {
		int i = 0;
		int T_len = sizeof(T);
		for (int idx = 0; idx < T_len; ++idx) {
			*(it + idx) = v >> (3 - idx) * 8;
			++i;
		}
		return i;
	}
}

enum packet_receive_state {
	S_READ_LEN,
	S_READ_CONTENT
};

int handle_error(int error_fd, int error_socket, string name_fd)
{
	if (error_fd == SOCKET_ERROR)
	{
		int rt = ::WSAGetLastError();
		cout << name_fd << " fail: " << rt << endl;//错误码
		closesocket(error_socket);
		return -1;
	}
	else if (error_fd == 0) return 0;
	else if (error_fd > 0) return 1;
}

int Create_Listeningsocket()
{
again_create:
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	int	ListeningSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	SOCKADDR_IN ServerAddr;
	memset(&ServerAddr, 0, sizeof(ServerAddr));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(8888);
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int bindrt = bind(ListeningSocket, (SOCKADDR *)&ServerAddr, sizeof(ServerAddr));
	if (handle_error(bindrt, ListeningSocket, "bind") == -1) goto again_create;

	int listenrt = listen(ListeningSocket, 3072);
	if (handle_error(listenrt, ListeningSocket, "listen") == -1) goto again_create;

	return ListeningSocket;
}

void handle_accept_client(int ListeningSocket_fd, vector<client> &client_info)
{
	//当当前连接的客户端小于限定的最大连接数量时，查询连接
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	int accept_fd = accept(ListeningSocket_fd, (SOCKADDR *)&clientAddr, &clientAddrLen);
	if (handle_error(accept_fd, accept_fd, "accept") == 1)
	{
		struct client current_client;
		//memset(&current_client, 0, sizeof(current_client));
		current_client.port = ntohs(clientAddr.sin_port);
		current_client.IP_addr = inet_ntoa(clientAddr.sin_addr);
		//inet_pton(AF_INET, "100.64.15.62", &clientAddr.sin_addr.s_addr);
		current_client.sockfd = accept_fd;
		client_info.push_back(current_client);
		cout << "current accept client IP: " << current_client.IP_addr << endl;
		cout << "current accept client port: " << current_client.port << endl << endl;
	}
}

void handle_client_massage(fd_set *read_set, vector<client> &client_info)
{
next_client:
	for (int i = 0; i < client_info.size(); i++)
	{
		//判断套接字描述符发生变化（有消息到来）的客户端
		int messagecoming = FD_ISSET(client_info[i].sockfd, read_set);
		if (messagecoming)
		{
			const int bufferLen = 1024;
			char readbuffer[bufferLen] = { 0 };
			int buffer_read_idx = 0;
			int buffer_write_idx = 0;
			int message_len;

			packet_receive_state s = S_READ_LEN;
		read_again:
			int recvrt = recv(client_info[i].sockfd, readbuffer + buffer_write_idx, bufferLen - buffer_write_idx, 0);

			string client_indicate_recv = client_info[i].IP_addr;
			client_indicate_recv += "/";
			client_indicate_recv += to_string(client_info[i].port);
			client_indicate_recv += " client recv: ";
			if (handle_error(recvrt, client_info[i].sockfd, client_indicate_recv) <= 0)
			{
				closesocket(client_info[i].sockfd);

				//消息接受完成，从集合read_set中删除该描述符，且从客户端套接字描述符数组中也删除
				FD_CLR(client_info[i].sockfd, read_set);

				cout << "current close client IP: " << client_info[i].IP_addr << endl;
				cout << "current close client port: " << client_info[i].port << endl << endl;
				client_info.erase(client_info.begin() + i);
				goto next_client;
			}

			buffer_write_idx += recvrt;

		read_content:
			switch (s)
			{
			case S_READ_LEN:
			{
				if (buffer_write_idx < sizeof(int) - 1)
				{
					goto read_again;
				}
				message_len = bytes_helper::read<int>(readbuffer, bytes_helper::type<int>());
				buffer_read_idx += sizeof(int);
				s = S_READ_CONTENT;
				goto read_content;
			}
			case S_READ_CONTENT:
			{
				int wrlen = buffer_write_idx - buffer_read_idx;
				if (wrlen >= message_len)
				{
					char message_content[bufferLen] = { 0 };
					memcpy(message_content, readbuffer + buffer_read_idx, message_len);
					current_total++;//处理消息次数
					buffer_read_idx += message_len;
					wrlen = buffer_write_idx - buffer_read_idx;

					memcpy(readbuffer, readbuffer + buffer_read_idx, wrlen);
					buffer_read_idx = 0;
					buffer_write_idx = wrlen;

					//send message (answer)
					char answerbuffer[bufferLen] = { 0 };
					int answerbuffer_idx = 0;
					int wlen = bytes_helper::write<int>(message_len, answerbuffer, bufferLen);
					answerbuffer_idx += wlen;
					memcpy(answerbuffer + answerbuffer_idx, message_content, message_len);
					answerbuffer_idx += message_len;
					int sendrt = send(client_info[i].sockfd, answerbuffer, answerbuffer_idx, 0);
					s = S_READ_LEN;

					string client_indicate_send = client_info[i].IP_addr;
					client_indicate_send += "/";
					client_indicate_send += to_string(client_info[i].port);
					client_indicate_send += " client send: ";
					if (handle_error(sendrt, client_info[i].sockfd, client_indicate_send) == -1)
					{
						closesocket(client_info[i].sockfd);

						FD_CLR(client_info[i].sockfd, read_set);
						cout << "current close client IP: " << client_info[i].IP_addr << endl;
						cout << "current close client port: " << client_info[i].port << endl << endl;
						client_info.erase(client_info.begin() + i);
						goto next_client;
					}
					break;
				}
				else
				{
					goto read_again;
				}
			}
			default:
			{
				assert(!"what");
			}
			}
		}
	}
}

void Handle_Client(int ListeningSocket_fd)
{
	struct timeval timeout;
	timeout.tv_sec = 0;  //s
	timeout.tv_usec = 0;  //ms

	fd_set read_set;
	vector<client> client_info;

	//Server QPS count
	const double time_span = 10;
	int QPScount_idx = 0;

	int before_total = 0;

	double current_time_duration = 0;
	clock_t starttime = clock();

	while (true)
	{
		//10s span and count
		current_time_duration = double(clock() - starttime) / CLOCKS_PER_SEC;
		while (current_time_duration > ((double)(QPScount_idx + 1))*time_span)
		{
			int QPScount = current_total - before_total;
			cout << QPScount_idx + 1 << "-10s      total: [" << QPScount;
			cout << "]      average(QPS): [" << (int)((double)(QPScount) / time_span) << "]" << endl << endl;
			QPScount_idx++;
			before_total = current_total;
		}

		//每次调用select前都要重新设置文件描述符和时间，因为事件发生后，文件描述符和时间都被内核修改
		FD_ZERO(&read_set);

		//添加服务端监听套接字到read_client_set集合
		FD_SET(ListeningSocket_fd, &read_set);

		//重新添加客户端套接字
		for (int i = 0; i < client_info.size(); i++)
		{
			FD_SET(client_info[i].sockfd, &read_set);
		}

		const int max_sockfd = 0;//Windows下该参数无所谓，可设任意值

								 //开始轮询处理服务端ListeningSocket_fd和客户端accept函数的client_sockfd套接字
		int selectrt = select(max_sockfd, &read_set, NULL, NULL, &timeout);//NULL);
		handle_error(selectrt, NULL, "select");

		const int Max_client_amount = 3072;

		if (FD_ISSET(ListeningSocket_fd, &read_set) && (client_info.size() < Max_client_amount))
		{
			//处理监听客户端请求
			handle_accept_client(ListeningSocket_fd, client_info);
		}
		else if (client_info.size() > 0)
		{
			//处理客户端消息(接受消息和应答：按消息原样返回)
			handle_client_massage(&read_set, client_info);
		}
	}
}

int main()
{
	int ListeningSocket = Create_Listeningsocket();
	Handle_Client(ListeningSocket);
	return 0;
}