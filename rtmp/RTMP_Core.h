#ifndef __RTMP_CORE_H
#include "common.h"
#include "RTMP_Packet.h"






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

	RTMP_Packet prev_packets[10];
#if 0
	uint8_t prev_msg_type[10];
	int32_t prev_msg_length[10];
	int32_t prev_msg_stream_id[10];
	int32_t prev_msg_read_data[10];
#endif

	uint32_t bytes_read;
	uint32_t last_bytes_read;

	uint32_t cur_tv;

}RTMP_Context;


int RTMP_ParseUrl(IN char *url, 
					OUT char *proto, IN int proto_size,
					OUT char *hostname, IN int hostname_size, 
					OUT char *path, IN int path_size);

int RTMP_Open(char *url);



#define __RTMP_CORE_H
#endif