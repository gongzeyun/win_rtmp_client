#ifndef __RTMP_H 
#define __RTMP_H

#include "common.h"



typedef enum 
{
    AMF_DATA_TYPE_NUMBER      = 0x00,
    AMF_DATA_TYPE_BOOL        = 0x01,
    AMF_DATA_TYPE_STRING      = 0x02,
    AMF_DATA_TYPE_OBJECT      = 0x03,
    AMF_DATA_TYPE_NULL        = 0x05,
    AMF_DATA_TYPE_UNDEFINED   = 0x06,
    AMF_DATA_TYPE_REFERENCE   = 0x07,
    AMF_DATA_TYPE_MIXEDARRAY  = 0x08,
    AMF_DATA_TYPE_OBJECT_END  = 0x09,
    AMF_DATA_TYPE_ARRAY       = 0x0a,
    AMF_DATA_TYPE_DATE        = 0x0b,
    AMF_DATA_TYPE_LONG_STRING = 0x0c,
    AMF_DATA_TYPE_UNSUPPORTED = 0x0d,
} AMFDataType;


typedef enum 
{
    RTMP_NETWORK_CHANNEL = 2,   ///< channel for network-related messages (bandwidth report, ping, etc)
    RTMP_SYSTEM_CHANNEL,        ///< channel for sending server control messages
    RTMP_SOURCE_CHANNEL,        ///< channel for sending a/v to server
    RTMP_VIDEO_CHANNEL = 8,     ///< channel for video data
    RTMP_AUDIO_CHANNEL,         ///< channel for audio data
}RTMPChannel;

typedef enum 
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
}RTMPPacketType;

#endif