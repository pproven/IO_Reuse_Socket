#include <iostream>
#include <time.h> 
#include <winsock2.h>

using namespace std;

void RecErrorDeal(double conDuration, int count)    //Recv的错误处理
{
	cout << "Receive failed!" << endl;
	cout << "The error dode:" << WSAGetLastError() << endl;
	cout << "Connection duration：" << conDuration << "s" << endl;
	cout << "The QPS value of the client:" << count / conDuration << endl << endl;
}


void SendErrorDeal(double conDuration, int count)    //Send的错误处理
{
	cout << "Send failed!" << endl;
	cout << "The error dode:" << WSAGetLastError() << endl;
	cout << "Connection duration：" << conDuration << "s" << endl;
	cout << "The QPS value of the server:" << count / conDuration << endl << endl;
}

