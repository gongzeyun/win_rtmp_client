// rtmp.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "RTMP_Core.h"
#include "RTMP_Packet.h"
#include "rtmp.h"


//this modify is to git test

RTMP_Context g_context;

char *TV_Channels[128][2] = 
						{
							{"rtmp://live.hkstv.hk.lxdns.com/live/hks", "rtmp://live.hkstv.hk.lxdns.com:1935/live"},
							{"rtmp://202.117.80.19/live/live2", "rtmp://202.117.80.19:1935/live"}
						};

int _tmain(int argc, _TCHAR* argv[])
{
	

select_again:
	printf("###########################################\n");
	printf("#  Now, Two TV Channels are supported:    #\n");
	printf("#    1.HongKong TV                        #\n");
	printf("#    2.CCTV13-news                        #\n");
	printf("###########################################\n");

	printf("please enter the number:\n");

	char ch_select = getchar();
	if (ch_select != '1' && ch_select != '2')
	{
		printf("++++wrong number, please select again++++\n");
		goto select_again;
	}

	g_context.cur_tv = ch_select - '0' - 1;
	g_context.in_chunk_size = 128;

	for (int i = 0; i < 10; i++)
	{
		RTMP_Create_packet(&g_context.prev_packets[i], 0, RTMP_SYSTEM_CHANNEL, RTMP_PT_CHUNK_SIZE, 
							0, 0, RTMP_PACKET_MAX_LENGTH);
	}
	RTMP_Open(TV_Channels[g_context.cur_tv][0]);
	//RTMP_Open("rtmp://live.hkstv.hk.lxdns.com/live/hks");
	//RTMP_Open("rtmp://lv1.ts33.net/tv/ttv09");
	//rtmp://202.117.80.19:1935/live/live2   //CCTV13
	return 0;
}

