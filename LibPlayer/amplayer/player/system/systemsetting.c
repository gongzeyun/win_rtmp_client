

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log_print.h>
#include <codec_type.h>
#include <libavcodec/avcodec.h>
#include "systemsetting.h"
#include <unistd.h>
#include "player_priv.h"

int PlayerSettingIsEnable(const char* path)
{
    char value[1024];
    if (GetSystemSettingString(path, value, NULL) > 0) {
        if ((!strcmp(value, "1") || !strcmp(value, "true") || !strcmp(value, "enable"))) {
            log_print("%s is enabled\n", path);
            return 1;
        }
    }
    log_print("%s is disabled\n", path);
    return 0;
}


float PlayerGetSettingfloat(const char* path)
{
    char value[1024];
    float ret = 0.0;
    if (GetSystemSettingString(path, value, NULL) > 0) {
        if ((sscanf(value, "%f", &ret)) > 0) {
            log_print("%s is set to %f\n", path, ret);
            return ret;
        }
    }
    log_print("%s is not set\n", path);
    return ret;
}

#define FILTER_VFMT_MPEG12	(1 << 0)
#define FILTER_VFMT_MPEG4	(1 << 1)
#define FILTER_VFMT_H264	(1 << 2)
#define FILTER_VFMT_MJPEG	(1 << 3)
#define FILTER_VFMT_REAL	(1 << 4)
#define FILTER_VFMT_JPEG	(1 << 5)
#define FILTER_VFMT_VC1		(1 << 6)
#define FILTER_VFMT_AVS		(1 << 7)
#define FILTER_VFMT_SW		(1 << 8)
#define FILTER_VFMT_HMVC    (1 << 9)
#define FILTER_VFMT_HEVC	(1 << 10)

int PlayerGetVFilterFormat(play_para_t*am_p)
{
	signed short video_index = am_p->vstream_info.video_index;
	char value[1024];
	int filter_fmt = 0;
	unsigned int codec_id;
	
	if (video_index != -1) {
		AVStream *pStream;
		AVCodecContext  *pCodecCtx;
		pStream = am_p->pFormatCtx->streams[video_index];
		pCodecCtx = pStream->codec;
		if (am_p->stream_type == STREAM_ES && pCodecCtx->codec_tag != 0) {
			codec_id=pCodecCtx->codec_tag;
		}
		else {
			codec_id=pCodecCtx->codec_id;
		}

		if ((pCodecCtx->codec_id == CODEC_ID_H264MVC) && (!am_p->vdec_profile.hmvc_para.exist)) {
			filter_fmt |= FILTER_VFMT_HMVC;
		}
		if ((pCodecCtx->codec_id == CODEC_ID_H264) && (!am_p->vdec_profile.h264_para.exist)) {
			filter_fmt |= FILTER_VFMT_H264;
		}
		if ((pCodecCtx->codec_id == CODEC_ID_HEVC) && (!am_p->vdec_profile.hevc_para.exist)) {
			filter_fmt |= FILTER_VFMT_HEVC;
		}
	}
	
    if (GetSystemSettingString("media.amplayer.disable-vcodecs", value, NULL) > 0) {
		log_print("[%s:%d]disable_vdec=%s\n", __FUNCTION__, __LINE__, value);
		if (match_types(value,"MPEG12") != NULL || match_types(value,"mpeg12") != NULL) {
			filter_fmt |= FILTER_VFMT_MPEG12;
		} 
		if (match_types(value,"MPEG4") != NULL || match_types(value,"mpeg4") != NULL) {
			filter_fmt |= FILTER_VFMT_MPEG4;
		} 
		if (match_types(value,"H264") != NULL || match_types(value,"h264") != NULL) {
			filter_fmt |= FILTER_VFMT_H264;
		}
		if (match_types(value,"HEVC") != NULL || match_types(value,"hevc") != NULL) {
			filter_fmt |= FILTER_VFMT_HEVC;
		} 
		if (match_types(value,"MJPEG") != NULL || match_types(value,"mjpeg") != NULL) {
			filter_fmt |= FILTER_VFMT_MJPEG;
		} 
		if (match_types(value,"REAL") != NULL || match_types(value,"real") != NULL) {
			filter_fmt |= FILTER_VFMT_REAL;
		} 
		if (match_types(value,"JPEG") != NULL || match_types(value,"jpeg") != NULL) {
			filter_fmt |= FILTER_VFMT_JPEG;
		} 
		if (match_types(value,"VC1") != NULL || match_types(value,"vc1") != NULL) {
			filter_fmt |= FILTER_VFMT_VC1;
		} 
		if (match_types(value,"AVS") != NULL || match_types(value,"avs") != NULL) {
			filter_fmt |= FILTER_VFMT_AVS;
		} 
		if (match_types(value,"SW") != NULL || match_types(value,"sw") != NULL) {
			filter_fmt |= FILTER_VFMT_SW;
		}
		if (match_types(value,"HMVC") != NULL || match_types(value,"hmvc") != NULL){
			filter_fmt |= FILTER_VFMT_HMVC;
		}
		/*filter by codec id*/
		if (match_types(value,"DIVX3") != NULL || match_types(value,"divx3") != NULL){
			if (codec_id == CODEC_TAG_DIV3)
				filter_fmt |= FILTER_VFMT_MPEG4;
		}
		if (match_types(value,"DIVX4") != NULL || match_types(value,"divx4") != NULL){
			if (codec_id == CODEC_TAG_DIV4)
				filter_fmt |= FILTER_VFMT_MPEG4;
		}
		if (match_types(value,"DIVX5") != NULL || match_types(value,"divx5") != NULL){
			if (codec_id == CODEC_TAG_DIV5)
				filter_fmt |= FILTER_VFMT_MPEG4;
		}
    }
	log_print("[%s:%d]filter_vfmt=%x\n", __FUNCTION__, __LINE__, filter_fmt);
    return filter_fmt;
}

#define FILTER_AFMT_MPEG		(1 << 0)
#define FILTER_AFMT_PCMS16L	    (1 << 1)
#define FILTER_AFMT_AAC			(1 << 2)
#define FILTER_AFMT_AC3			(1 << 3)
#define FILTER_AFMT_ALAW		(1 << 4)
#define FILTER_AFMT_MULAW		(1 << 5)
#define FILTER_AFMT_DTS			(1 << 6)
#define FILTER_AFMT_PCMS16B		(1 << 7)
#define FILTER_AFMT_FLAC		(1 << 8)
#define FILTER_AFMT_COOK		(1 << 9)
#define FILTER_AFMT_PCMU8		(1 << 10)
#define FILTER_AFMT_ADPCM		(1 << 11)
#define FILTER_AFMT_AMR			(1 << 12)
#define FILTER_AFMT_RAAC		(1 << 13)
#define FILTER_AFMT_WMA			(1 << 14)
#define FILTER_AFMT_WMAPRO		(1 << 15)
#define FILTER_AFMT_PCMBLU		(1 << 16)
#define FILTER_AFMT_ALAC		(1 << 17)
#define FILTER_AFMT_VORBIS		(1 << 18)
#define FILTER_AFMT_AAC_LATM		(1 << 19)
#define FILTER_AFMT_APE		       (1 << 20)
#define FILTER_AFMT_EAC3		       (1 << 21)
int PlayerGetAFilterFormat(const char *prop)
{
	char value[1024];
	int filter_fmt = 0;	
#ifndef 	USE_ARM_AUDIO_DEC
    /* check the dts/ac3 firmware status */
    if(access("/system/etc/firmware/audiodsp_codec_ddp_dcv.bin",F_OK)){
//#ifndef DOLBY_DAP_EN
        if(access("/system/lib/libstagefright_soft_ddpdec.so",F_OK))
            filter_fmt |= (FILTER_AFMT_AC3|FILTER_AFMT_EAC3);
//#endif
    }
    if(access("/system/etc/firmware/audiodsp_codec_dtshd.bin",F_OK) ){
        if(access("/system/lib/libstagefright_soft_dtshd.so",F_OK))
            filter_fmt  |= FILTER_AFMT_DTS;
    }
#else
    if(access("/system/lib/libstagefright_soft_dcvdec.so",F_OK)){
#ifndef DOLBY_DAP_EN
	filter_fmt |= (FILTER_AFMT_AC3|FILTER_AFMT_EAC3);
#endif
    }
    if(access("/system/lib/libstagefright_soft_dtshd.so",F_OK) ){
		filter_fmt  |= FILTER_AFMT_DTS;
    }
#endif	
    if (GetSystemSettingString(prop, value, NULL) > 0) {
		log_print("[%s:%d]disable_adec=%s\n", __FUNCTION__, __LINE__, value);
		if (match_types(value,"mpeg") != NULL || match_types(value,"MPEG") != NULL) {
			filter_fmt |= FILTER_AFMT_MPEG;
		} 
		if (match_types(value,"pcms16l") != NULL || match_types(value,"PCMS16L") != NULL) {
			filter_fmt |= FILTER_AFMT_PCMS16L;
		} 
		if (match_types(value,"aac") != NULL || match_types(value,"AAC") != NULL) {
			filter_fmt |= FILTER_AFMT_AAC;
		} 
		if (match_types(value,"ac3") != NULL || match_types(value,"AC3") != NULL) {
			filter_fmt |= FILTER_AFMT_AC3;
		}		
		if (match_types(value,"alaw") != NULL || match_types(value,"ALAW") != NULL) {
			filter_fmt |= FILTER_AFMT_ALAW;
		} 
		if (match_types(value,"mulaw") != NULL || match_types(value,"MULAW") != NULL) {
			filter_fmt |= FILTER_AFMT_MULAW;
		} 
		if (match_types(value,"dts") != NULL || match_types(value,"DTS") != NULL) {
			filter_fmt |= FILTER_AFMT_DTS;
		} 
		if (match_types(value,"pcms16b") != NULL || match_types(value,"PCMS16B") != NULL) {
			filter_fmt |= FILTER_AFMT_PCMS16B;
		} 
		if (match_types(value,"flac") != NULL || match_types(value,"FLAC") != NULL) {
			filter_fmt |= FILTER_AFMT_FLAC;
		}
		if (match_types(value,"cook") != NULL || match_types(value,"COOK") != NULL) {
			filter_fmt |= FILTER_AFMT_COOK;
		} 
		if (match_types(value,"pcmu8") != NULL || match_types(value,"PCMU8") != NULL) {
			filter_fmt |= FILTER_AFMT_PCMU8;
		} 
		if (match_types(value,"adpcm") != NULL || match_types(value,"ADPCM") != NULL) {
			filter_fmt |= FILTER_AFMT_ADPCM;
		} 
		if (match_types(value,"amr") != NULL || match_types(value,"AMR") != NULL) {
			filter_fmt |= FILTER_AFMT_AMR;
		} 
		if (match_types(value,"raac") != NULL || match_types(value,"RAAC") != NULL) {
			filter_fmt |= FILTER_AFMT_RAAC;
		}
		if (match_types(value,"wma") != NULL || match_types(value,"WMA") != NULL) {
			filter_fmt |= FILTER_AFMT_WMA;
		} 
		if (match_types(value,"wmapro") != NULL || match_types(value,"WMAPRO") != NULL) {
			filter_fmt |= FILTER_AFMT_WMAPRO;
		} 
		if (match_types(value,"pcmblueray") != NULL || match_types(value,"PCMBLUERAY") != NULL) {
			filter_fmt |= FILTER_AFMT_PCMBLU;
		} 
		if (match_types(value,"alac") != NULL || match_types(value,"ALAC") != NULL) {
			filter_fmt |= FILTER_AFMT_ALAC;
		} 
		if (match_types(value,"vorbis") != NULL || match_types(value,"VORBIS") != NULL) {
			filter_fmt |= FILTER_AFMT_VORBIS;
		}
		if (match_types(value,"aac_latm") != NULL || match_types(value,"AAC_LATM") != NULL) {
			filter_fmt |= FILTER_AFMT_AAC_LATM;
		} 
		if (match_types(value,"ape") != NULL || match_types(value,"APE") != NULL) {
			filter_fmt |= FILTER_AFMT_APE;
		} 		
		if (match_types(value,"eac3") != NULL || match_types(value,"EAC3") != NULL) {
			filter_fmt |= FILTER_AFMT_EAC3;
		} 		
    }
	log_print("[%s:%d]filter_afmt=%x\n", __FUNCTION__, __LINE__, filter_fmt);
    return filter_fmt;
}


