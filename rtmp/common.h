#ifndef __COMMON_H
#define __COMMON_H

#include <windows.h>
#include <stdint.h>

//#define D_DEBUG


#define RTMP_MIN(a,b)	(((a) < (b)) ? (a) : (b))


#define RTMP_DEFAULT_PORT	1935

#define IN  
#define OUT

#define RTMP_PACKET_MAX_LENGTH 1024 * 1024


#define AV_RB32(x)                                \
    (((uint32_t)((const uint8_t*)(x))[0] << 24) |    \
               (((const uint8_t*)(x))[1] << 16) |    \
               (((const uint8_t*)(x))[2] <<  8) |    \
                ((const uint8_t*)(x))[3])






#endif