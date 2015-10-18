#ifndef __RTMP_CORE_H
#include "common.h"
int RTMP_ParseUrl(IN char *url, 
					OUT char *proto, IN int proto_size,
					OUT char *hostname, IN int hostname_size, 
					OUT char *path, IN int path_size);

int RTMP_Open(char *url);


typedef struct RTMP_Context
{
	SOCKET fd_socket;
	char url[128];
	char proto[128];
	char app[128];
	char instance[128];
	int in_chunk_size;     //chunk size from server
	int out_chunk_size;   //chunk size to server

	int32_t server_band_width;
	int32_t client_band_width;

}RTMP_Context;

#define __RTMP_CORE_H
#endif