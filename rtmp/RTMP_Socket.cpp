#include "stdafx.h"

#include <WinSock2.h>
#include <ws2tcpip.h>

#include "common.h"
#include "RTMP_Socket.h"
#include "RTMP_Core.h"

#pragma comment (lib, "Ws2_32.lib")

extern RTMP_Context g_context;

int RTMP_Socket_open(IN char *host_name, IN int port)
{
	int iResult;
	DWORD dwRetval;
	char str_port[16] = {0};


	struct addrinfo *result = NULL;
    struct addrinfo hints;
	struct sockaddr_in *temp = NULL;

	WSADATA wsaData;

	/* 根据 host_name,取得对应的地址 */
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

	itoa(port, str_port, 10);

	dwRetval = getaddrinfo(host_name, str_port, &hints, &result);
    if ( dwRetval != 0 ) 
	{
        printf("getaddrinfo failed with error: %d\n", dwRetval);
        WSACleanup();
        return -1;
    }

	temp = (sockaddr_in*)result->ai_addr;

	/* connect to server */
	SOCKET fd_socket = socket(AF_INET, SOCK_STREAM, 0);
	
	int ret_conn = -1;
	ret_conn = connect(fd_socket, (sockaddr*)temp, sizeof(sockaddr));

	if (ret_conn < 0)
	{
		printf("ip %s\n", inet_ntoa(temp->sin_addr));
		printf("connect to server error, error no %d\n", WSAGetLastError());

		return -1;
	}
	g_context.fd_socket = fd_socket;

	return fd_socket;
}


int RTMP_Write(char *buf_write, uint32_t size_write)
{
	return send(g_context.fd_socket, buf_write, size_write, 0);
}


int RTMP_Read(char *buf_read, uint32_t size_read)
{
	return recv(g_context.fd_socket, buf_read, size_read, 0);
}