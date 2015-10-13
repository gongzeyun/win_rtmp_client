#include "stdafx.h"


#include <string.h>
#include "common.h"
#include "rtmp.h"
#include "RTMP_Socket.h"
#include "RTMP_Packet.h"
#include "RTMP_Core.h"

extern RTMP_Context g_context;

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

	 //send C2
	 send_ret = RTMP_Write((char*)S1, 1536);
	 if (send_ret < 0)
	 {
		 printf("client send C2 error, errorno %d\n", WSAGetLastError());
	 }

	 //recv S2
	 uint8_t S2[1536] = {0};
	 recv_ret = RTMP_Read((char*)S2, 1536);
	 if (recv_ret < 0)
	 {
		 printf("client recv S2 error, errorno %d\n", WSAGetLastError());
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
	RTMP_AMF_write_number(&p, 1);	//always 1

	RTMP_AMF_write_object_start(&p);

	RTMP_AMF_write_field_name(&p, "app");
	RTMP_AMF_write_string(&p, g_context.app);

	RTMP_AMF_write_field_name(&p, "flashVer");
	RTMP_AMF_write_string(&p, "LNX 9,0,124,2");
	
	RTMP_AMF_write_field_name(&p, "tcUrl");
	RTMP_AMF_write_string(&p, g_context.url);

	RTMP_AMF_write_field_name(&p, "fpad");
	RTMP_AMF_write_bool(&p, 0);


	RTMP_AMF_write_field_name(&p, "audioCodecs");
    RTMP_AMF_write_number(&p, 4071.0);
    RTMP_AMF_write_field_name(&p, "videoCodecs");
    RTMP_AMF_write_number(&p, 252.0);
    RTMP_AMF_write_field_name(&p, "videoFunction");
    RTMP_AMF_write_number(&p, 1.0);

	RTMP_AMF_write_object_end(&p);

	pkt.data_size = p - pkt.data;

#if 0
	uint8_t temp[210] = {
							  0x02 ,0x00 ,0x07 ,0x63 ,0x6f ,0x6e ,0x6e ,0x65
							 ,0x63 ,0x74 ,0x00 ,0x3f ,0xf0 ,0x00 ,0x00 ,0x00
							 ,0x00 ,0x00 ,0x00 ,0x03 ,0x00 ,0x03 ,0x61 ,0x70
							 ,0x70 ,0x02 ,0x00 ,0x04 ,0x6c ,0x69 ,0x76 ,0x65
							 ,0x00 ,0x08 ,0x66 ,0x6c ,0x61 ,0x73 ,0x68 ,0x56
							 ,0x65 ,0x72 ,0x02 ,0x00 ,0x0d ,0x4c ,0x4e ,0x58
							 ,0x20 ,0x39 ,0x2c ,0x30 ,0x2c ,0x31 ,0x32 ,0x34
							 ,0x2c ,0x32 ,0x00 ,0x05 ,0x74 ,0x63 ,0x55 ,0x72
							 ,0x6c ,0x02 ,0x00 ,0x28 ,0x72 ,0x74 ,0x6d ,0x70
							 ,0x3a ,0x2f ,0x2f ,0x6c ,0x69 ,0x76 ,0x65 ,0x2e
							 ,0x68 ,0x6b ,0x73 ,0x74 ,0x76 ,0x2e ,0x68 ,0x6b
							 ,0x2e ,0x6c ,0x78 ,0x64 ,0x6e ,0x73 ,0x2e ,0x63
							 ,0x6f ,0x6d ,0x3a ,0x31 ,0x39 ,0x33 ,0x35 ,0x2f
							 ,0x6c ,0x69 ,0x76 ,0x65 ,0x00 ,0x04 ,0x66 ,0x70
							 ,0x61 ,0x64 ,0x01 ,0x00 ,0x00 ,0x0c ,0x63 ,0x61
							 ,0x70 ,0x61 ,0x62 ,0x69 ,0x6c ,0x69 ,0x74 ,0x69
							 ,0x65 ,0x73 ,0x00 ,0x40 ,0x2e ,0x00 ,0x00 ,0x00
							 ,0x00 ,0x00 ,0x00 ,0x00 ,0x0b ,0x61 ,0x75 ,0x64
							 ,0x69 ,0x6f ,0x43 ,0x6f ,0x64 ,0x65 ,0x63 ,0x73
							 ,0x00 ,0x40 ,0xaf ,0xce ,0x00 ,0x00 ,0x00 ,0x00
							 ,0x00 ,0x00 ,0x0b ,0x76 ,0x69 ,0x64 ,0x65 ,0x6f
							 ,0x43 ,0x6f ,0x64 ,0x65 ,0x63 ,0x73 ,0x00 ,0x40
							 ,0x6f ,0x80 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
							 ,0x0d ,0x76 ,0x69 ,0x64 ,0x65 ,0x6f ,0x46 ,0x75
							 ,0x6e ,0x63 ,0x74 ,0x69 ,0x6f ,0x6e ,0x00 ,0x3f
							 ,0xf0 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
							 ,0x00 ,0x09
						};

	memcpy(pkt.data, temp, 210);
	pkt.data_size = 210;
#endif
	if (0 != RTMP_Send_packet(&pkt))
	{
		printf("[RTMP_Connect] send packet error\n");
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
		return -1;
	}

	g_context.server_band_width = (pkt_from_server.data[0] << 24) | (pkt_from_server.data[1] << 16) | (pkt_from_server.data[2] << 8) | (pkt_from_server.data[3]);
	if (pkt_from_server.data_size < 4 || g_context.server_band_width < 0)
	{
		printf("[RTMP_Connect] recv server BW packet error, line %d\n", __LINE__);
		return -1;
	}


	//recv packet from server[msg type is 6 Set Peer Bandwidth]
	if (0 != RTMP_Recv_packet(&pkt_from_server))
	{
		printf("[RTMP_Connect] recv server BW packet error, line %d\n", __LINE__);
		return -1;
	}
	g_context.client_band_width = g_context.server_band_width = (pkt_from_server.data[0] << 24) | (pkt_from_server.data[1] << 16) | (pkt_from_server.data[2] << 8) | (pkt_from_server.data[3]);
	if (pkt_from_server.data_size < 4 || g_context.server_band_width < 0)
	{
		printf("[RTMP_Connect] recv Set Peer Bandwidth error, line %d\n", __LINE__);
		return -1;
	}


#if 1
	//send packet to server[ msg type 5 server bandwidth]
	RTMP_Packet pkt_to_server_BW;
	ret = RTMP_Create_packet(&pkt_to_server_BW, 0, RTMP_NETWORK_CHANNEL, RTMP_PT_SERVER_BW, 0, 0, 4);

	p = pkt_to_server_BW.data;
	RTMP_write4byte_to_buffer(&p, g_context.server_band_width);

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
		return -1;
	}

	//recv packet from server[msg type 20, 17 Command message]
	if (0 != RTMP_Recv_packet(&pkt_from_server))
	{
		printf("[RTMP_Connect] recv server Stream begin pkt error, line %d\n", __LINE__);
		return -1;
	}

	RTMP_Destroy_packet(&pkt_from_server);

#if 0
	//send packet to server[ msg type 5 server bandwidth]
	RTMP_Packet pkt_to_server_BW;
	ret = RTMP_Create_packet(&pkt_to_server_BW, 0, RTMP_NETWORK_CHANNEL, RTMP_PT_SERVER_BW, 0, 0, 4);

	p = pkt_to_server_BW.data;
	RTMP_write4byte_to_buffer(&p, g_context.server_band_width);

	pkt_to_server_BW.data_size = p - pkt_to_server_BW.data;
	if (0 != RTMP_Send_packet(&pkt_to_server_BW))
	{
		printf("[RTMP_Connect] send packet error\n");
		return -1;
	}
#endif
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

	return 0;
}



int RTMP_Play()
{
	//send create server to 
}

