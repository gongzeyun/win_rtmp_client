#ifndef __RTMP_PACKET_H
#define __RTMP_PACKET_H
#include "common.h"


enum RTMPChannel 
{
    RTMP_NETWORK_CHANNEL = 2,   ///< channel for network-related messages (bandwidth report, ping, etc)
    RTMP_SYSTEM_CHANNEL,        ///< channel for sending server control messages
    RTMP_SOURCE_CHANNEL,        ///< channel for sending a/v to server
    RTMP_VIDEO_CHANNEL = 8,     ///< channel for video data
    RTMP_AUDIO_CHANNEL,         ///< channel for audio data
};

enum RTMPPacketType 
{
    RTMP_PT_CHUNK_SIZE   =  1,  ///< chunk size change
    RTMP_PT_BYTES_READ   =  3,  ///< number of bytes read
    RTMP_PT_PING,               ///< ping
    RTMP_PT_SERVER_BW,          ///< server bandwidth
    RTMP_PT_CLIENT_BW,          ///< client bandwidth
    RTMP_PT_AUDIO        =  8,  ///< audio packet
    RTMP_PT_VIDEO,              ///< video packet
    RTMP_PT_FLEX_STREAM  = 15,  ///< Flex shared stream
    RTMP_PT_FLEX_OBJECT,        ///< Flex shared object
    RTMP_PT_FLEX_MESSAGE,       ///< Flex shared message
    RTMP_PT_NOTIFY,             ///< some notification
    RTMP_PT_SHARED_OBJ,         ///< shared object
    RTMP_PT_INVOKE,             ///< invoke some stream action
    RTMP_PT_METADATA     = 22,  ///< FLV metadata
};


typedef struct RTMP_Packet
{
	uint8_t fmt_type;
	RTMPChannel channel_id;
	RTMPPacketType msg_type;		//
	uint32_t msg_stream_id;
	//int msg_stream_id;
	int time_stamp;
	int time_delta;
	uint8_t *data;
	int32_t data_size;

}RTMP_Packet;



int RTMP_Create_packet(RTMP_Packet* pkt, int fmt_type, RTMPChannel channel_id, 
						RTMPPacketType msg_type, uint32_t msg_stream_id, int32_t time_stamp, int32_t data_size);
void RTMP_Destroy_packet(RTMP_Packet *pkt);

void RTMP_AMF_write_string(uint8_t **dst, char *str);
void RTMP_AMF_write_number(uint8_t **dst, double number);
void RTMP_AMF_write_object_start(uint8_t **dst);
void RTMP_AMF_write_field_name(uint8_t **dst, char *str);
void RTMP_AMF_write_bool(uint8_t **dst, int val);
void RTMP_AMF_write_object_end(uint8_t **dst);

int RTMP_Send_packet(RTMP_Packet *pkt);
int RTMP_Recv_packet(RTMP_Packet *pkt);

void RTMP_write4byte_to_buffer(uint8_t **p, uint32_t val);

#endif