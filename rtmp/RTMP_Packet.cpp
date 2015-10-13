#include "stdafx.h"
#include "RTMP_Packet.h"
#include "rtmp.h"
#include "common.h"
#include "RTMP_Socket.h"

union RTMP_intfloat64
{
	uint64_t i;
    double   f;
};


uint64_t RTMP_Double2int(double f)
{
    union RTMP_intfloat64 v;
    v.f = f;
    return v.i;
}


void RTMP_AMF_write_null(uint8_t **dst)
{
    //bytestream_put_byte(dst, AMF_DATA_TYPE_NULL);
	**dst = AMF_DATA_TYPE_NULL;

	(*dst) += 1;
}


void RTMP_AMF_write_string(uint8_t **dst, char *str)
{
    **dst = AMF_DATA_TYPE_STRING;
	(*dst) += 1;

    uint16_t str_len = strlen(str);
	**dst = str_len >> 8;
	(*dst) += 1;

	**dst = str_len & 0x00FF;
	(*dst) += 1;

	memcpy(*dst, str, strlen(str));

	*dst += strlen(str);

	return;
 
}

void RTMP_AMF_write_number(uint8_t **dst, double number)
{
	**dst = AMF_DATA_TYPE_NUMBER;
	*dst += 1;

	uint64_t int_number = RTMP_Double2int(number);
	//*(uint64_t*)(*dst) = int_number;

	**dst = int_number >> 56;
	(*dst) += 1;

	**dst = int_number >> 48;
	(*dst) += 1;

	**dst = int_number >> 40;
	(*dst) += 1;

	**dst = int_number >> 32;
	(*dst) += 1;

	**dst = int_number >> 24;
	(*dst) += 1;

	**dst = int_number >> 16;
	(*dst) += 1;

	**dst = int_number >> 8;
	(*dst) += 1;

	**dst = int_number & 0x00FF;
	(*dst) += 1;
}


void RTMP_AMF_write_object_start(uint8_t **dst)
{
    **dst = AMF_DATA_TYPE_OBJECT;
	*dst += 1;
}


void RTMP_AMF_write_object_end(uint8_t **dst)
{
	**dst = 0;
	*dst += 1;

	**dst = 0;
	*dst += 1;

	**dst = AMF_DATA_TYPE_OBJECT_END;
	*dst += 1;
}

void RTMP_AMF_write_field_name(uint8_t **dst, char *str)
{
	//*(uint16_t*)(*dst) = strlen(str);
	uint16_t str_len = strlen(str);

	**dst = str_len >> 8;
	(*dst) += 1;

	**dst = str_len & 0x00FF;	
	(*dst) += 1;

	memcpy(*dst, str, strlen(str));
	*dst += strlen(str);

}


void RTMP_AMF_write_bool(uint8_t **dst, int val)
{
    **dst = AMF_DATA_TYPE_BOOL;
	*dst += 1;

	**dst = val;
	*dst += 1;
}


int RTMP_Create_packet(RTMP_Packet* pkt, int fmt_type, RTMPChannel channel_id, 
						RTMPPacketType msg_type, uint32_t msg_stream_id, int32_t time_stamp, int32_t data_size)
{
	pkt->data = (uint8_t*)malloc(data_size);

	if (NULL == pkt->data)
	{
		return -1;
	}

	pkt->fmt_type = fmt_type;
	pkt->channel_id = channel_id;
	pkt->msg_type = msg_type;
	pkt->msg_stream_id = msg_stream_id;
	pkt->time_stamp = time_stamp;
	pkt->data_size = data_size;

	return 0;
}


void RTMP_write4byte_to_buffer(uint8_t **p, uint32_t val)
{
	//*(uint32_t*)(*p) = val;
	//*p += sizeof(val);

	uint8_t highest_byte = 0;
	uint8_t higher_byte = 0;
	uint8_t lower_byte = 0;
	uint8_t lowest_byte = 0;

	highest_byte = val >> 24;
	higher_byte = val >> 16;
	lower_byte = val >> 8;
	lowest_byte = val;


	**p = highest_byte;
	*p += sizeof(uint8_t);

	**p = higher_byte;
	*p += sizeof(uint8_t);

	**p = lower_byte;
	*p += sizeof(uint8_t);

	**p = lowest_byte;
	*p += sizeof(uint8_t);


	return;
}

void RTMP_write3byte_to_buffer(uint8_t **p, uint32_t val)
{
	uint8_t high_byte = 0;
	uint8_t mid_byte = 0;
	uint8_t low_byte = 0;

	high_byte = val >> 16;
	mid_byte =  val >> 8;
	low_byte = val;

	**p = high_byte;
	*p += sizeof(uint8_t);

	**p = mid_byte;
	*p += sizeof(uint8_t);

	**p = low_byte;
	*p += sizeof(uint8_t);

	return;

}


void RTMP_write1byte_to_buffer(uint8_t **p, uint8_t val)
{
	**p = val;
	*p += sizeof(val);

	return;
}


void RTMP_Destroy_packet(RTMP_Packet *pkt)
{
	if (NULL == pkt)
	{
		return;
	}

	free(pkt->data);
}

int RTMP_Recv_packet(RTMP_Packet *pkt)
{
	uint8_t hdr;
	uint8_t *p_hdr_recv;

	uint8_t hdr_recv[18] = {0};
	p_hdr_recv = hdr_recv;

	int read_ret = RTMP_Read((char*)&hdr, 1);
	if (0 >= read_ret)
	{
		printf("error, error no %d\n", WSAGetLastError());
		return -1;
	}

	//根据hdr取得fmt_type
	uint8_t fmt_type_recv = hdr >> 6;
	pkt->fmt_type = fmt_type_recv;

	uint8_t character_code_channel_id = hdr & 0x3F;
	uint8_t hdr_size_recv = 0;

	/*
		根据协议文档：
			Chunk Type = 0时，msg_header 有11个字节
			Chunk Type = 1时，msg_header 有7个字节
			Chunk Type = 2时，msg_header 有3个字节
			Chunk Type = 3时，msg_header 有0个字节
	*/
	if (0 == fmt_type_recv)
	{
		hdr_size_recv += 11;
	}
	else if (1 == fmt_type_recv)
	{
		hdr_size_recv += 7;
	}
	else if (2 == fmt_type_recv)
	{
		hdr_size_recv += 3;
	}
	else
	{
		hdr_size_recv += 0;
	}


	if (character_code_channel_id == 0)
	{
		hdr_size_recv += 1;
	}
	else if (character_code_channel_id == 1)
	{
		hdr_size_recv += 2;
	}
	else
	{
		hdr_size_recv += 0;
	}

	read_ret = RTMP_Read((char*)hdr_recv, hdr_size_recv); 
	if (read_ret <= 0)
	{
		printf("[RTMP_Recv_packet] line %d, error, errorno %d\n", __LINE__, WSAGetLastError());
		return -1;
	}

	if (character_code_channel_id == 0)
	{
		pkt->channel_id = (RTMPChannel)(hdr_recv[0] + 64);
		p_hdr_recv++;
	}
	else if (character_code_channel_id == 1)
	{
		pkt->channel_id = (RTMPChannel)(hdr_recv[1] * 256 + 64);
		p_hdr_recv++;
		p_hdr_recv++;
	}
	else
	{
		pkt->channel_id = (RTMPChannel)character_code_channel_id;
	}

	//只有Chunk Type为0时，才有time_stamp
	if (fmt_type_recv == 0)
	{
		pkt->time_stamp = ((*p_hdr_recv) << 16) | ((*(p_hdr_recv + 1)) << 8) | ((*(p_hdr_recv + 2)));
		p_hdr_recv += 3;
	}

	//只有Chunk Type为1和2时，才有time delta
	if (fmt_type_recv == 1 || fmt_type_recv == 2)
	{
		pkt->time_stamp = ((*p_hdr_recv) << 16) | ((*(p_hdr_recv + 1)) << 8) | ((*(p_hdr_recv + 2)));
		p_hdr_recv += 3;
	}

	//只有Chunk Type为0、1、2时，才有msg length
	if (fmt_type_recv < 3)
	{
		pkt->data_size = ((*p_hdr_recv) << 16) | ((*(p_hdr_recv + 1)) << 8) | ((*(p_hdr_recv + 2)));
		p_hdr_recv += 3;
	}

	//只有Chunk Type为0、1、2时，才有msg type
	//msg type 1个字节
	if (fmt_type_recv < 3)
	{
		pkt->msg_type = (RTMPPacketType)((*(p_hdr_recv)));
		++p_hdr_recv;
	}

	//只有Chunk Type 为0时，才有msg stream id
	//msg stream id为4个字节
	if (0 == fmt_type_recv)
	{
		pkt->msg_stream_id = ((*p_hdr_recv) << 24) | ((*(p_hdr_recv + 1)) << 16) | ((*(p_hdr_recv + 2)) << 8) | ((*(p_hdr_recv + 3)));
	}

	read_ret = RTMP_Read((char*)pkt->data, pkt->data_size);
	if (read_ret <= 0)
	{
		printf("[RTMP_Recv_packet] line %d, error, errorno %d\n", __LINE__, WSAGetLastError());
		return -1;
	}

	return 0;

}


int RTMP_Send_packet(RTMP_Packet *pkt)
{
	uint8_t pkt_header[18] = {0};	//根据协议文档，header 最大为14个字节
	uint8_t *p = pkt_header;

	if (pkt->fmt_type > 3)
	{
		printf("[RTMP_Send_packet] pkt fmt type error, fmt_type %d\n", pkt->fmt_type);
		return -1;
	}

	/*
		0 1 2 3 4 5 6 7
		+-+-+-+-+-+-+-+-+
		|fmt| cs id		|
		+-+-+-+-+-+-+-+-+
		Chunk basic header 1

		0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|fmt| 0			| cs id - 64	|
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		Chunk basic header 2


		0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|fmt| 1			| cs id - 64					|
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		Chunk basic header 3
	*/
	  
	/* 根据上面协议文档中的说明，对header进行拼接 */
	if (pkt->channel_id > 65599)
	{
		printf("[RTMP_Send_packet] pkt channel id error, channel id %d\n", pkt->channel_id);
	}

	if (pkt->channel_id < 64)
	{
		*p = (pkt->fmt_type << 6) | (pkt->channel_id);
		p++;
	}
	else if (pkt->channel_id >= 64 && pkt->channel_id < 320)
	{
		*p = (pkt->fmt_type << 6) | 0;
		p++;

		*p = pkt->channel_id - 64;
		p++;
	}
	else
	{
		*p = (pkt->fmt_type << 6) | 1;
		p++;
		*p = pkt->channel_id - 64;
		p++;
	}

	/*
		根据协议文档，当fmt_type为0时，
		timestamp为3个byte, msg_length 为3个byte,
		message type id 为1个byte,msg stream id 为4个byte
	*/
	if (pkt->fmt_type == 0)
	{
		//set timestamp
		RTMP_write3byte_to_buffer(&p, pkt->time_stamp);

		//set msg_length
		RTMP_write3byte_to_buffer(&p, pkt->data_size);

		//set msg_type
		RTMP_write1byte_to_buffer(&p, pkt->msg_type);

		//set msg stream id
		RTMP_write4byte_to_buffer(&p, pkt->msg_stream_id);
	}


	//msg header end
	int send_ret = -1;
	send_ret = RTMP_Write((char*)pkt_header, p - pkt_header);
	//if (pkt->)
	//uint8_t temp[8] = {0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x14};
	//send_ret = RTMP_Write((char*)temp, 8);
	if (send_ret <= 0)
	{
		printf("[RTMP_Send_packet] fmt type 0, send data error\n", WSAGetLastError());
		return -1;
	}

	int send_size = 0; 
	while (send_size < pkt->data_size)
	{
		int towrite = RTMP_MIN(128, pkt->data_size - send_size);
		send_ret = RTMP_Write((char*)pkt->data + send_size, towrite);
		if (send_ret <= 0)
		{
			printf("[RTMP_Send_packet] fmt type 0, send data error %d\n", WSAGetLastError());
			return -1;
		}
		send_size += send_ret;

		//write header
		if (send_size < pkt->data_size)
		{
			uint8_t marker = 0xC0 | pkt->channel_id;
			send_ret = RTMP_Write((char*)&marker, sizeof(marker));
			if (send_ret <= 0)
			{
				printf("[RTMP_Send_packet] fmt type 0, send header error\n", WSAGetLastError());
				return -1;
			}
		}
			
	}


	return 0;
}
