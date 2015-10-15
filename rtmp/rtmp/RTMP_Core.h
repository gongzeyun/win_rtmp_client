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

	double tranction_id;
	double stream_id;	//create stream 成功后，server端返回的结果

	int32_t server_band_width;
	int32_t client_band_width;

	uint8_t prev_msg_type;
	int32_t prev_msg_length;
	int32_t prev_msg_stream_id;

}RTMP_Context;

#define __RTMP_CORE_H
#endif