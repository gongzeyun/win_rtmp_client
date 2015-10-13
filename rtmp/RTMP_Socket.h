#ifndef RTMP_SOCKET_H
#define RTMP_SOCKET_H

#include "common.h"
#include "RTMP_Packet.h"

int RTMP_Socket_open(IN char *host_name, IN int port);
int RTMP_Write(char *buf_write, uint32_t size_write);
int RTMP_Read(char *buf_read, uint32_t size_read);



#endif