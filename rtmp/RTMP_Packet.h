#ifndef __RTMP_PACKET_H
#define __RTMP_PACKET_H
#include "common.h"
#include "rtmp.h"


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

	int32_t read_data_size;
	int is_read_complete;

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
void RTMP_AMF_write_null(uint8_t **dst);


int RTMP_Send_packet(RTMP_Packet *pkt);
int RTMP_Recv_packet(RTMP_Packet *pkt);

void RTMP_write4byte_to_buffer_b(uint8_t **p, uint32_t val);
void RTMP_write4byte_to_buffer_l(uint8_t **p, uint32_t val);

void RTMP_write3byte_to_buffer(uint8_t **p, uint32_t val);

void RTMP_write2byte_to_buffer_b(uint8_t **p, uint16_t val);
void RTMP_write2byte_to_buffer_l(uint8_t **p, uint16_t val);

void RTMP_write1byte_to_buffer(uint8_t **p, uint8_t val);

uint64_t RTMP_read8byte_from_buffer(uint8_t *p);
double RTMP_Int2double(uint64_t i);
uint64_t RTMP_Double2int(double f);


#endif