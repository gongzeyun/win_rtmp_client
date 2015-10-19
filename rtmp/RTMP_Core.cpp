#include "stdafx.h"


#include <string.h>
#include "common.h"
#include "rtmp.h"
#include "RTMP_Socket.h"
#include "RTMP_Packet.h"
#include "RTMP_Core.h"

extern RTMP_Context g_context;
extern char *TV_Channels[128][2];

int RTMP_Hand_shake()
{
	printf("HandShaking...\n");

	uint8_t C0 = 3;
	uint8_t S0 = 0;

	int send_ret = -1;
	int recv_ret = -1;

	//send C0 to server 
	send_ret = RTMP_Write((char*)&C0, sizeof(C0));
	
	if (send_ret < 0)
	{
		printf("client send C0 error, errorno %d\n", WSAGetLastError());
		return -1;
	}

	//send C1 to server
	uint8_t C1[1536] = {
							0, 0, 0, 0,		//time 4 byte
							0, 0, 0, 0,		//zero 4 byte
						};
	
	 srand(0xDEADC0DE);
	 for (int i = 8; i < 1536; i++)
	 {
		C1[i] = rand();
	 }

	 send_ret = RTMP_Write((char*)C1, 1536);
	 if (send_ret < 0)
	 {
		 printf("client send C1 error, errorno %d\n", WSAGetLastError());
	 }

	 Sleep(5000);

	 //recv S0
	 recv_ret = RTMP_Read((char*)&S0, sizeof(S0));
	 if (recv_ret < 0)
	 {
		 printf("client recv S0 error, errorno %d\n", WSAGetLastError());
	 }

	 //recv S1
	 uint8_t S1[1536] = {0};
	 recv_ret = RTMP_Read((char*)S1, 1536);
	 if (recv_ret < 0)
	 {
		 printf("client recv S1 error, errorno %d\n", WSAGetLastError());
	 }

	 //recv S2
	 uint8_t S2[1536] = {0};
	 recv_ret = RTMP_Read((char*)S2, 1536);
	 if (recv_ret < 0)
	 {
		 printf("client recv S2 error, errorno %d\n", WSAGetLastError());
	 }

	 //send C2
	 send_ret = RTMP_Write((char*)S1, 1536);
	 if (send_ret < 0)
	 {
		 printf("client send C2 error, errorno %d\n", WSAGetLastError());
	 }

	 if (!memcmp(C1, S2, 1536))
	 {
		 return 0;
	 }

	 return -1;
}


/*
	说明：从rtmp url中取得 协议、hostname、路径
	参数: IN url, OUT proto, IN proto_size
	返回值: OK 0, NG -1

	url 形式：rtmp://live.hkstv.hk.lxdns.com/live/hks
*/
int RTMP_ParseUrl(IN char *url, 
					OUT char *proto, IN int proto_size,
					OUT char *hostname, IN int hostname_size, 
					OUT char *app, IN int app_size, 
					OUT char *instance, IN int instance_size)
{
	char *p;
	char *phost;
	char *papp, *pinstance;


	if (NULL == url || NULL == proto)
	{
		return -1;

	}

	strcpy(g_context.url, url);

	/* 取得协议特征字符串 */
	p = strchr(url, ':');
	if (NULL != p)
	{
		memcpy(proto, url, RTMP_MIN(p - url, proto_size));
	}

	/* 取得hostname 字符串*/
	p++;		//skip ':'
	if (*p == '/')		//skip '/'
	{
		p++;
	}
	if (*p == '/')
	{
		p++;
	}

	phost = strchr(p, '/');
	if (NULL != phost)
	{
		memcpy(hostname, p, RTMP_MIN(phost - p, hostname_size));
	}

	/* 取得appname字符串 */
	phost++; //skip '/'
	
	papp = strchr(phost, '/');
	if (NULL != papp)
	{
		memcpy(app, phost, RTMP_MIN(papp - phost, app_size));
	}

	papp++;


	memcpy(instance, papp, instance_size);
	
	return 0;
}


int RTMP_Connect()
{
	RTMP_Packet pkt;
	uint8_t *p;

	int ret = -1;
	ret = RTMP_Create_packet(&pkt, 0, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE, 0, 0, RTMP_PACKET_MAX_LENGTH);
	if (0 != ret)
	{
		printf("RTMP_Connect, create pkt error\n");
		return -1;
	}

	p = pkt.data;

	RTMP_AMF_write_string(&p, "connect");

	g_context.tranction_id = 1;
	RTMP_AMF_write_number(&p, g_context.tranction_id++);	//always 1


	RTMP_AMF_write_object_start(&p);

	RTMP_AMF_write_field_name(&p, "app");
	RTMP_AMF_write_string(&p, g_context.app);
	//RTMP_AMF_write_string(&p, "lives");

	RTMP_AMF_write_field_name(&p, "flashVer");
	//RTMP_AMF_write_string(&p, "LNX 9,0,124,2");
	RTMP_AMF_write_string(&p, "FMSc/1.0");
	
	RTMP_AMF_write_field_name(&p, "tcUrl");
	//RTMP_AMF_write_string(&p, "rtmp://live.hkstv.hk.lxdns.com:1935/live");
	RTMP_AMF_write_string(&p, TV_Channels[g_context.cur_tv][1]);
#if 1
	RTMP_AMF_write_field_name(&p, "fpad");
	RTMP_AMF_write_bool(&p, 0);


	RTMP_AMF_write_field_name(&p, "audioCodecs");
    RTMP_AMF_write_number(&p, 4071.0);
    RTMP_AMF_write_field_name(&p, "videoCodecs");
    RTMP_AMF_write_number(&p, 252.0);
    RTMP_AMF_write_field_name(&p, "videoFunction");
    RTMP_AMF_write_number(&p, 1.0);
#endif
	RTMP_AMF_write_object_end(&p);

	pkt.data_size = p - pkt.data;


	if (0 != RTMP_Send_packet(&pkt))
	{
		printf("[RTMP_Connect] send packet error\n");
		RTMP_Destroy_packet(&pkt);
		return -1;
	}

	RTMP_Destroy_packet(&pkt);

	//根据connect时序图进行处理
	//recv packet from server[msg type is 5 server bandwidth]
	RTMP_Packet pkt_from_server;
	ret = RTMP_Create_packet(&pkt_from_server, 0, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE, 0, 0, RTMP_PACKET_MAX_LENGTH);

	if (0 != RTMP_Recv_packet(&pkt_from_server))
	{
		printf("[RTMP_Connect] recv server BW packet error, line %d\n", __LINE__);
		RTMP_Destroy_packet(&pkt_from_server);
		return -1;
	}
	printf("msg type %d\n", pkt_from_server.msg_type);

	g_context.server_band_width = (pkt_from_server.data[0] << 24) | (pkt_from_server.data[1] << 16) | (pkt_from_server.data[2] << 8) | (pkt_from_server.data[3]);
	if (pkt_from_server.data_size < 4 || g_context.server_band_width < 0)
	{
		printf("[RTMP_Connect] recv server BW packet error, line %d\n", __LINE__);
		RTMP_Destroy_packet(&pkt_from_server);
		return -1;
	}


	//recv packet from server[msg type is 6 Set Peer Bandwidth]
	if (0 != RTMP_Recv_packet(&pkt_from_server))
	{
		printf("[RTMP_Connect] recv server BW packet error, line %d\n", __LINE__);
		RTMP_Destroy_packet(&pkt_from_server);
		return -1;
	}
	printf("msg type %d\n", pkt_from_server.msg_type);

	g_context.client_band_width = g_context.server_band_width = (pkt_from_server.data[0] << 24) | (pkt_from_server.data[1] << 16) | (pkt_from_server.data[2] << 8) | (pkt_from_server.data[3]);
	if (pkt_from_server.data_size < 4 || g_context.server_band_width < 0)
	{
		printf("[RTMP_Connect] recv Set Peer Bandwidth error, line %d\n", __LINE__);
		RTMP_Destroy_packet(&pkt_from_server);
		return -1;
	}


#if 0
	//send packet to server[ msg type 5 server bandwidth]
	RTMP_Packet pkt_to_server_BW;
	ret = RTMP_Create_packet(&pkt_to_server_BW, 0, RTMP_NETWORK_CHANNEL, RTMP_PT_SERVER_BW, 0, 0, 4);

	p = pkt_to_server_BW.data;
	RTMP_write4byte_to_buffer_b(&p, g_context.server_band_width);

	pkt_to_server_BW.data_size = p - pkt_to_server_BW.data;
	if (0 != RTMP_Send_packet(&pkt_to_server_BW))
	{
		printf("[RTMP_Connect] send packet error\n");
		return -1;
	}

	RTMP_Destroy_packet(&pkt_to_server_BW);
#endif
	
	//recv packet from server[msg type 4, User control message]
	if (0 != RTMP_Recv_packet(&pkt_from_server))
	{
		printf("[RTMP_Connect] recv server Stream begin pkt error, line %d\n", __LINE__);
		RTMP_Destroy_packet(&pkt_from_server);
		return -1;
	}
	printf("msg type %d\n", pkt_from_server.msg_type);
	//recv packet from server[msg type 20, 17 Command message]
	if (0 != RTMP_Recv_packet(&pkt_from_server))
	{
		printf("[RTMP_Connect] recv server Stream begin pkt error, line %d\n", __LINE__);
		RTMP_Destroy_packet(&pkt_from_server);
		return -1;
	}
	printf("msg type %d\n", pkt_from_server.msg_type);
	//while (!RTMP_Recv_packet(&pkt_from_server));
	RTMP_Destroy_packet(&pkt_from_server);

#if 1
	
	//send packet to server[ msg type 5 server bandwidth]
	RTMP_Packet pkt_to_server_BW;
	ret = RTMP_Create_packet(&pkt_to_server_BW, 0, RTMP_NETWORK_CHANNEL, RTMP_PT_SERVER_BW, 0, 0, 4);

	p = pkt_to_server_BW.data;
	RTMP_write4byte_to_buffer_b(&p, g_context.server_band_width);

	pkt_to_server_BW.data_size = p - pkt_to_server_BW.data;
	if (0 != RTMP_Send_packet(&pkt_to_server_BW))
	{
		printf("[RTMP_Connect] send packet error\n");
		RTMP_Destroy_packet(&pkt_to_server_BW);
		return -1;
	}
#endif

	RTMP_Destroy_packet(&pkt_to_server_BW);
	return 0;
}


void RTMP_Response_ping(RTMP_Packet *pkt_recv)
{
	RTMP_Packet pkt_send;
	uint8_t *p;

	RTMP_Create_packet(&pkt_send, 0, pkt_recv->channel_id, pkt_recv->msg_type, 
							pkt_recv->msg_stream_id, pkt_recv->time_stamp, RTMP_PACKET_MAX_LENGTH);

	p = pkt_send.data;

	*p = 0x00;
	p += 1;

	*p = 0x07;
	p += 1;

   //RTMP_write4byte_to_buffer_b(&p, pkt_recv->time_stamp);
	memcpy(p, pkt_recv->data + 2, 4);
	p += 4;

   pkt_send.data_size = p - pkt_send.data;


   RTMP_Send_packet(&pkt_send);

   RTMP_Destroy_packet(&pkt_send);

}



void process_server_pkt()
{
	RTMP_Packet pkt;
	int ret = RTMP_Create_packet(&pkt, 0, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE, 0, 0, RTMP_PACKET_MAX_LENGTH);


	FILE *pFile = NULL;

	pFile = fopen("D:\\test.flv", "wb+");
	if (NULL == pFile)
	{
		printf("create file %s error\n", "D:\\test.flv");
		return ;
	}

	//先写入FLV文件头
	char* hdr_flv = "FLV\1\5\0\0\0\011\0\0\0\0";

	int write_ret = -1;
	write_ret = fwrite(hdr_flv, 1, 13, pFile);


	while (1)
	{
		if (0 != RTMP_Recv_packet(&pkt))
		{
			//printf("[process_server_pkt] recv server create stream response error\n");
			RTMP_Destroy_packet(&pkt);
			return;
		}
		
		//告诉server端Client已经接收到多少个字节
		RTMP_Packet packet_bytes_read;
		RTMP_Create_packet(&packet_bytes_read, 0, RTMP_NETWORK_CHANNEL, RTMP_PT_BYTES_READ, 
																g_context.stream_id, 0, RTMP_PACKET_MAX_LENGTH);

		uint8_t *p = packet_bytes_read.data;
		RTMP_write4byte_to_buffer_b(&p, g_context.bytes_read);
		packet_bytes_read.data_size = p - packet_bytes_read.data;

		RTMP_Send_packet(&packet_bytes_read);
		RTMP_Destroy_packet(&packet_bytes_read);


		if (4 == pkt.msg_type)
		{	
			uint8_t content = *(pkt.data + 1);
			//printf("break\n");
		}

		if (
				pkt.msg_type == RTMP_PT_AUDIO ||
				pkt.msg_type == RTMP_PT_VIDEO ||
				(pkt.msg_type == RTMP_PT_NOTIFY && !memcmp("\002\000\012onMetaData", pkt.data, 13))
			)
		{
			//FLV Tag头固定有15个字节
			//char flv_tag_hdr[15];
			//char *p = flv_tag_hdr;
			char *p = (char*)malloc(15 + pkt.data_size);
			char *temp = p;

			RTMP_write1byte_to_buffer((uint8_t **)&p, pkt.msg_type);
			
			RTMP_write3byte_to_buffer((uint8_t **)&p, pkt.data_size);	//data size
			RTMP_write3byte_to_buffer((uint8_t **)&p, pkt.time_stamp);	//time stamp

			RTMP_write1byte_to_buffer((uint8_t **)&p, 0);  //time stamp


			RTMP_write3byte_to_buffer((uint8_t **)&p, 0);	//stream id


			memcpy(p, pkt.data, pkt.data_size);  

			p += pkt.data_size;

			//RTMP_write4byte_to_buffer_b((uint8_t **)&p, 0);
			int pre_packet_size = 0;

			memcpy(p, &pre_packet_size, 4);

			int write_size = 0;
			if (write_size < 15 + pkt.data_size)
			{
				write_ret = fwrite(temp + write_size, 1, 15 + pkt.data_size - write_size, pFile);
				write_size+= write_ret;
			}

			//fclose(pFile);
			if (write_size < pkt.data_size)
			{
				break;
			}

			free(temp);
		}
			
		if (pkt.msg_type == RTMP_PT_PING)
		{
			if (0 == pkt.data[0] && 6 == pkt.data[1])
			{
				RTMP_Response_ping(&pkt);
			}
		}
		//printf("type %d, size %d, channel_id %d\n", pkt.msg_type, pkt.data_size, pkt.channel_id);
		if (RTMP_PT_AUDIO == pkt.msg_type || RTMP_PT_VIDEO == pkt.msg_type)
		//if (RTMP_PT_VIDEO == pkt.msg_type)
		{
			printf("type %d, timestamp %d\n", pkt.msg_type, pkt.time_stamp);
		}
		//printf("the bytes read is %d, bandwidth %d\n", g_context.bytes_read, g_context.client_band_width>>1);
	
		Sleep(15);
	}

	return;
}


int RTMP_Play()
{
	
	uint8_t *p;

	//generate createStream packet
	RTMP_Packet pkt;
	int ret = RTMP_Create_packet(&pkt, 0, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE, 0, 0, RTMP_PACKET_MAX_LENGTH);

	p = pkt.data;
	RTMP_AMF_write_string(&p, "createStream");
	RTMP_AMF_write_number(&p, g_context.tranction_id++);
	RTMP_AMF_write_null(&p);

	pkt.data_size = p - pkt.data;

	//send create stream  to server
	if (0 != RTMP_Send_packet(&pkt))
	{
		printf("[RTMP_Play] send create stream error\n");
		RTMP_Destroy_packet(&pkt);
		return -1;
	}

	//recv command message, (_result- createStream response)
	while (1)
	{
		if (0 != RTMP_Recv_packet(&pkt))
		{
			printf("[RTMP_Play] recv server create stream response error, line %d\n", __LINE__);
			RTMP_Destroy_packet(&pkt);
			return -1;
		}
		printf("msg type %d\n", pkt.msg_type);

		if (!memcmp(pkt.data, "\002\000\007_result", 10))
		{
			//get stream id
			//根据协议文档，最后pkt.data的最后8个字节为stream_id(即为payload的第21个字节开始，payload一共为29个字节)
			uint64_t temp_stream_id = RTMP_read8byte_from_buffer(pkt.data + 21);

			g_context.stream_id = RTMP_Int2double(temp_stream_id);
			break;
		}
		else if ((!memcmp(pkt.data, "\002\000\010onBWDone", 11)))
		{
#if 1
			RTMP_Packet pkt_check_bw;
			int ret = RTMP_Create_packet(&pkt_check_bw, 0, RTMP_SYSTEM_CHANNEL, RTMP_PT_INVOKE, 0, 0, RTMP_PACKET_MAX_LENGTH);

			p = pkt_check_bw.data;
			RTMP_AMF_write_string(&p, "_checkbw");
			RTMP_AMF_write_number(&p, g_context.tranction_id++);
			RTMP_AMF_write_null(&p);

			pkt_check_bw.data_size = p - pkt_check_bw.data;

			RTMP_Send_packet(&pkt_check_bw);
			RTMP_Destroy_packet(&pkt_check_bw);
#endif
		}
	}

	//now, we create stream success
	RTMP_Destroy_packet(&pkt);


	//generate play packet
	RTMP_Packet pkt_play;
	ret = RTMP_Create_packet(&pkt_play, 0, RTMP_VIDEO_CHANNEL, RTMP_PT_INVOKE, g_context.stream_id, 0, RTMP_PACKET_MAX_LENGTH);
	p = pkt_play.data;

	RTMP_AMF_write_string(&p, "play");
	RTMP_AMF_write_number(&p, 4);

	RTMP_AMF_write_null(&p);

	RTMP_AMF_write_string(&p, g_context.instance);
	//RTMP_AMF_write_string(&p, "hks");

	RTMP_AMF_write_number(&p, -2 * 1000);	//default is -2

	RTMP_AMF_write_number(&p, -1000);	//default is -2

	pkt_play.data_size = p - pkt_play.data;

	if (0 != RTMP_Send_packet(&pkt_play))
	{
		printf("[RTMP_Connect] send packet error\n");
		RTMP_Destroy_packet(&pkt_play);
		return -1;
	}
#if 1
	/**
		* Generate client buffer time and send it to the server.
	*/
	p = pkt_play.data;
	pkt_play.channel_id = RTMP_NETWORK_CHANNEL;
	pkt_play.fmt_type = 0;
	pkt_play.msg_type = RTMP_PT_PING;

	RTMP_write2byte_to_buffer_b(&p, 3);
	RTMP_write4byte_to_buffer_b(&p, g_context.stream_id);
	RTMP_write4byte_to_buffer_b(&p, 9000);

	pkt_play.data_size = p - pkt_play.data;
	if (0 != RTMP_Send_packet(&pkt_play))
	{
		printf("[RTMP_Connect] send packet [set bug time]error\n");
		RTMP_Destroy_packet(&pkt_play);
		return -1;
	}
#endif
	//根据协议说明文档，Server端应该回过来[set chunk size]消息
	//其余消息忽略
	while (1)
	{
		if (0 != RTMP_Recv_packet(&pkt_play))
		{
			printf("[RTMP_Play] recv server create stream response error, line %d\n", __LINE__);
			RTMP_Destroy_packet(&pkt_play);
			return -1;
		}
		if (RTMP_PT_CHUNK_SIZE == pkt_play.msg_type)		//msg is set chunk size
		{
			//get chunk size
			printf("msg type %d\n", pkt_play.msg_type);
			g_context.in_chunk_size = AV_RB32(pkt_play.data);
			break;
		}
		printf("msg type %d\n", pkt_play.msg_type);
	}

	//根据协议文档说明，Server端应该回过来[StreamBegin]消息
	//其余消息忽略
	while (1)
	{
		if (0 != RTMP_Recv_packet(&pkt_play))
		{
			printf("[RTMP_Play] recv server create stream response error, line %d\n", __LINE__);
			RTMP_Destroy_packet(&pkt_play);
			return -1;
		}
		if (RTMP_PT_PING == pkt_play.msg_type)		//usr control msg
		{
			printf("msg type %d\n", pkt_play.msg_type);
			break;
		}
		printf("msg type %d\n", pkt_play.msg_type);
	}

	//根据协议文档说明，Server端应该回过来[onStatus-play reset]消息
	//其余消息忽略
	while (1)
	{
		if (0 != RTMP_Recv_packet(&pkt_play))
		{
			printf("[RTMP_Play] recv server onStatus-play reset error, line %d\n", __LINE__);
			RTMP_Destroy_packet(&pkt_play);
			return -1;
		}
		if (RTMP_PT_INVOKE == pkt_play.msg_type)		//command msg
		{
			printf("msg type %d\n", pkt_play.msg_type);
			break;
		}
	}

	//根据协议文档说明，Server端应该回过来[onStatus-play start]消息
	//其余消息忽略
	while (1)
	{
		if (0 != RTMP_Recv_packet(&pkt_play))
		{
			printf("[RTMP_Play] recv server onStatus-play start error, line %d\n", __LINE__);
			RTMP_Destroy_packet(&pkt_play);
			return -1;
		}
		if (RTMP_PT_INVOKE == pkt_play.msg_type)		//command msg
		{
			printf("msg type %d\n", pkt_play.msg_type);
			break;
		}
		
	}

	RTMP_Destroy_packet(&pkt_play);


	//从此处开始，正常播放的时序已经完成
	//启动处理接受packet线程
	DWORD thread_id;
	HANDLE h_thread_process_pkt = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)process_server_pkt, NULL, 0, &thread_id);

	WaitForSingleObject(h_thread_process_pkt, INFINITE);
	return 0;
}

int RTMP_Open(char *url)
{
	char proto[16] = {0};
	char host_name[128] = {0};
	//char path[128] = {0};
	char app[128] = {0};
	char instance[128] = {0};

	int port = RTMP_DEFAULT_PORT;
	int ret = -1;

	if (0 != RTMP_ParseUrl(url, proto, 16, host_name, 128, app, 128, instance, 128))
	{
		printf("RTMP parse url error\n");
		return -1;
	}
	
	strcpy(g_context.proto, proto);
	strcpy(g_context.app, app);
	strcpy(g_context.instance, instance);


	if (strcmp("rtmp", proto))
	{
		/* 暂时不支持其他协议 */
		return -1;
	}

	//memcpy(g_context.path, path, strlen(path));

	ret = RTMP_Socket_open(host_name, port);
	if (ret < 0)
	{
		printf("open hostname %s, port %d socket error %d\n", host_name, port, WSAGetLastError());
		return -1;
	}

	//hand shake
	ret = RTMP_Hand_shake();
	if (ret != 0)
	{
		printf("client and server handshake error\n");
		return -1;
	}

	//hand shake done
	printf("Hand shake success\n");


	RTMP_Connect();
	RTMP_Play();

	return 0;
}





