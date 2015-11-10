#ifndef _LIBAACDEC_H_
#define _LIBAACDEC_H_

//header file
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>

// ffmpeg headers
#include <stdlib.h>
#include "common.h"
#include "structs.h"


#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "neaacdec.h"
//#include "../../amadec/adec_write.h"
#include "../../amadec/adec-armdec-mgt.h"


#define    ACODEC_FMT_NULL   -1
#define    ACODEC_FMT_MPEG   0
#define    ACODEC_FMT_PCM_S16LE  1
#define    ACODEC_FMT_AAC   2
#define    ACODEC_FMT_AC3    3
#define    ACODEC_FMT_ALAW  4
#define    ACODEC_FMT_MULAW  5
#define    ACODEC_FMT_DTS  6
#define    ACODEC_FMT_PCM_S16BE  7
#define    ACODEC_FMT_FLAC  8
#define    ACODEC_FMT_COOK  9
#define    ACODEC_FMT_PCM_U8  10
#define    ACODEC_FMT_ADPCM  11
#define    ACODEC_FMT_AMR   12
#define    ACODEC_FMT_RAAC   13
#define    ACODEC_FMT_WMA   14
#define    ACODEC_FMT_WMAPRO    15
#define    ACODEC_FMT_PCM_BLURAY   16
#define    ACODEC_FMT_ALAC   17
#define    ACODEC_FMT_VORBIS     18
#define    ACODEC_FMT_AAC_LATM    19
#define    ACODEC_FMT_APE    20
#define    ACODEC_FMT_EAC3    21
#define    ACODEC_FMT_WIFIDISPLAY 22
//#include <mp4ff.h>
//int main(int argc, char *argv[]);

//extern audio_decoder_operations_t AudioAacDecoder;
int audio_dec_init(audio_decoder_operations_t *adec_ops);
int audio_dec_decode(audio_decoder_operations_t *adec_ops, char *outbuf, int *outlen, char *inbuf, int inlen);
int audio_dec_release(audio_decoder_operations_t *adec_ops);
int audio_dec_getinfo(audio_decoder_operations_t *adec_ops, void *pAudioInfo);
//void libaacdec(void);
#endif


