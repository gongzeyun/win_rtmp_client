/*
 * RTSP demuxer
 * Copyright (c) 2002 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/avstring.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "avformat.h"

#include "internal.h"
#include "network.h"
#include "os_support.h"
#include "rtsp.h"
#include "rdt.h"
#include "url.h"
#include <itemlist.h>
#include <amthreadpool.h>

typedef struct RTSPRecvContext {
    struct itemlist bufferlist;
    volatile int brunning;
    volatile int recv_buffer_error;
#if HAVE_PTHREADS
    pthread_t recv_buffer_thread;
#endif
}RTSPRecvContext;

static int _rtsp_read_packet(AVFormatContext *s, AVPacket *pkt) ;
static int rtsp_read_flush(AVFormatContext *s);
void rtsp_invoke(void *para);

static void *recv_buffer_task( void *_AVFormatContext)
{
    AVFormatContext * lpCtx=_AVFormatContext;
    if(NULL == lpCtx)
        return NULL;

    RTSPState *rt = lpCtx->priv_data;
    if(NULL == rt)
        return NULL;

    RTSPRecvContext *lpRTSPCtx = rt->priv_data;
    if(NULL == rt)
        return NULL;
    
    av_log(lpCtx, AV_LOG_ERROR, "[%s:%d]recv_buffer_task start running!!!\n", __FUNCTION__, __LINE__);
    AVPacket * lpkt = NULL ;
    int ret=0;
    while(lpRTSPCtx->brunning > 0) {
        if (url_interrupt_cb()) {
            ret = AVERROR_EXIT;
            av_log(lpCtx, AV_LOG_ERROR, "[%s:%d]url_interrupt_cb!!!\n", __FUNCTION__, __LINE__);
            break;
        }

        if (lpRTSPCtx->bufferlist.max_items > 0 && lpRTSPCtx->bufferlist.max_items <= lpRTSPCtx->bufferlist.item_count) {
            amthreadpool_thread_usleep(10);
            continue;
        }

        if (NULL == lpkt)
            lpkt = av_mallocz(sizeof(AVPacket)) ;

        if (NULL == lpkt) {
            amthreadpool_thread_usleep(10);
            continue;
        }
        av_init_packet(lpkt);

        ret=_rtsp_read_packet(lpCtx, lpkt);
        if (ret == 0) {
            itemlist_add_tail_data(&(lpRTSPCtx->bufferlist), (unsigned long) lpkt) ;
            //av_free_packet(lpkt);
            //av_free(lpkt);
            lpkt=NULL;
        }
        else {
            av_free_packet(lpkt);
            if (ret == AVERROR(EAGAIN)) {
                continue;
            }
            else if (ret == AVERROR_EXIT)
                av_log(NULL, AV_LOG_ERROR, "[%s:%d] AVERROR_EXIT!\n", __FUNCTION__, __LINE__);
            else if (ret == AVERROR_EOF)
                av_log(NULL, AV_LOG_ERROR, "[%s:%d] File to EOF!\n", __FUNCTION__, __LINE__);

            av_free(lpkt);
            lpkt=NULL;
            break;
        }
    }

    if (lpRTSPCtx->brunning == 0)
        ret = AVERROR_EXIT;
    
    lpRTSPCtx->recv_buffer_error = ret;
    lpRTSPCtx->brunning=0;
    av_log(lpCtx, AV_LOG_ERROR, "[%s:%d]recv_buffer_task end!!!\n", __FUNCTION__, __LINE__);
    return NULL;
}

static int free_packet(void * apkt)
{
    AVPacket * lpkt = apkt;
    if(lpkt != NULL){
    	av_free_packet(lpkt);
    	av_free(lpkt);
    }
    apkt = NULL;
    return 0;
}

static int rtsp_read_flush(AVFormatContext *s)
{
    RTSPState *rt = s->priv_data;
    RTSPRecvContext *lpRTSPCtx = rt->priv_data;
    AVPacket *lpkt;
    int ret;

    if(NULL == rt)
        return AVERROR(ENOMEM);

    if(NULL == lpRTSPCtx)
	return 0;
    
    /* delay for inbound packets */
    //usleep (300000);

    #if 0
    /* fixme: long delay, 10 seconds timeout !!! */
    /* empty the socket of the media stream */
    lpkt = av_mallocz (sizeof(AVPacket)) ;
    if (NULL == lpkt)
        return AVERROR(ENOMEM);

    /* flush socket data */
    av_init_packet(lpkt);
    while (1) {
       if (url_interrupt_cb()) {
                break;
        }
       ret = ff_rtsp_fetch_packet(s, lpkt);
       if(ret == 0)
               av_free_packet(lpkt);
       else
           break;
    }
    av_free(lpkt);
    #endif

    /* drop all received packets */
    itemlist_clean(&lpRTSPCtx->bufferlist, free_packet);

    return 0;
}

static int rtsp_read_play(AVFormatContext *s)
{
    RTSPState *rt = s->priv_data;
    RTSPMessageHeader reply1, *reply = &reply1;
    int i;
    char cmd[1024];
	 int length;
    av_log(s, AV_LOG_INFO, "hello state=%d\n", rt->state);
    rt->nb_byes = 0;

    if (!(rt->server_type == RTSP_SERVER_REAL && rt->need_subscription)) {
        if (rt->transport == RTSP_TRANSPORT_RTP) {
            for (i = 0; i < rt->nb_rtsp_streams; i++) {
                RTSPStream *rtsp_st = rt->rtsp_streams[i];
                RTPDemuxContext *rtpctx = rtsp_st->transport_priv;
                if (!rtpctx)
                    continue;
                ff_rtp_reset_packet_queue(rtpctx);
                rtpctx->last_rtcp_ntp_time  = AV_NOPTS_VALUE;
                rtpctx->first_rtcp_ntp_time = AV_NOPTS_VALUE;
                rtpctx->base_timestamp      = 0;
                rtpctx->rtcp_ts_offset      = 0;
            }
        }
        if (rt->state == RTSP_STATE_PAUSED) {
            cmd[0] = 0;
        } else {
#if 1
            snprintf(cmd, sizeof(cmd),
                     "Range: npt=%"PRId64".%03"PRId64"-\r\n",
                     rt->seek_timestamp / AV_TIME_BASE,
                     rt->seek_timestamp / (AV_TIME_BASE / 1000) % 1000);
#else
	       // time scale function
               // seek, fast-forward/fast-rewind triggered seek 
               if (rt->playback_rate_permille != rt->playback_rate_permille_next)
                       rt->playback_rate_permille = rt->playback_rate_permille_next;

               if (rt->playback_rate_permille >= 0) {
                       // forward
                       snprintf(cmd, sizeof(cmd),
                               "Range: npt=%"PRId64".%03"PRId64"-\r\n",
                               rt->seek_timestamp / AV_TIME_BASE,
                               rt->seek_timestamp / (AV_TIME_BASE / 1000) % 1000);
               } else {
                       // backward
                       snprintf(cmd, sizeof(cmd),
                               "Range: npt=%"PRId64".%03"PRId64"-0\r\n",
                               rt->seek_timestamp / AV_TIME_BASE,
                               rt->seek_timestamp / (AV_TIME_BASE / 1000) % 1000);
               }
               length = strlen (cmd);
               snprintf(&cmd [length], sizeof(cmd) - length,
                               "Scale: %d.%d\r\n",
                               rt->playback_rate_permille / 1000,
                               rt->playback_rate_permille % 1000);
               // length = strlen (cmd); 
#endif
        }
        ff_rtsp_send_cmd(s, "PLAY", rt->control_uri, cmd, reply, NULL);
        if (reply->status_code != RTSP_STATUS_OK) {
            return -1;
        }
        if (rt->transport == RTSP_TRANSPORT_RTP &&
            reply->range_start != AV_NOPTS_VALUE) {
            for (i = 0; i < rt->nb_rtsp_streams; i++) {
                RTSPStream *rtsp_st = rt->rtsp_streams[i];
                RTPDemuxContext *rtpctx = rtsp_st->transport_priv;
                AVStream *st = NULL;
                if (!rtpctx || rtsp_st->stream_index < 0)
                    continue;
                st = s->streams[rtsp_st->stream_index];
                rtpctx->range_start_offset =
                    av_rescale_q(reply->range_start, AV_TIME_BASE_Q,
                                 st->time_base);
            }
        }
    }
    rt->state = RTSP_STATE_STREAMING;

    if (rt->rtsp_hd != NULL && rt->rtsp_hd->invoke == NULL) {
        rt->rtsp_hd->invoke=rtsp_invoke;
        rt->rtsp_hd->invoke_para=s;
    }

    if (rt->priv_data != NULL) {
#if HAVE_PTHREADS
        RTSPRecvContext *lpRTSPCtx = rt->priv_data;
        if (lpRTSPCtx != NULL) {
            av_log(NULL, AV_LOG_INFO, "[%s:%d]start the rtsp demux thread\n", __FUNCTION__, __LINE__);
            lpRTSPCtx->brunning = 1;
            if (amthreadpool_pthread_create_name(&lpRTSPCtx->recv_buffer_thread, NULL, recv_buffer_task, s,"rtsp demuxor thread")) {
                av_log(s, AV_LOG_ERROR, "pthread_create failed\n");
                av_free(lpRTSPCtx);
                return -1 ;
            }
        }
#endif
    }
    return 0;
}

/* pause the stream */
static int rtsp_read_pause(AVFormatContext *s)
{
    RTSPState *rt = s->priv_data;
    RTSPMessageHeader reply1, *reply = &reply1;
    
    if(rt->priv_data!=NULL){    
#if HAVE_PTHREADS
	RTSPRecvContext *lpRTSPCtx = rt->priv_data;
	if(lpRTSPCtx != NULL){
		lpRTSPCtx->brunning = 0;
		amthreadpool_pthread_join(lpRTSPCtx->recv_buffer_thread, NULL);
		lpRTSPCtx->recv_buffer_thread = 0;
		#if 0
		/* do'nt flush media packets in 'play->pause->play' use case */
		itemlist_clean(&lpRTSPCtx->bufferlist, free_packet);
		#endif
	}	
#endif 
    }

    if (rt->state != RTSP_STATE_STREAMING)
        return 0;
    else if (!(rt->server_type == RTSP_SERVER_REAL && rt->need_subscription)) {
        ff_rtsp_send_cmd(s, "PAUSE", rt->control_uri, NULL, reply, NULL);
        if (reply->status_code != RTSP_STATUS_OK) {
            return -1;
        }
    }
    rt->state = RTSP_STATE_PAUSED;
    return 0;
}

int ff_rtsp_setup_input_streams(AVFormatContext *s, RTSPMessageHeader *reply)
{
    RTSPState *rt = s->priv_data;
    char cmd[1024];
    unsigned char *content = NULL;
    int ret;

    /* describe the stream */
    snprintf(cmd, sizeof(cmd),
             "Accept: application/sdp\r\n");
    if (rt->server_type == RTSP_SERVER_REAL) {
        /**
         * The Require: attribute is needed for proper streaming from
         * Realmedia servers.
         */
        av_strlcat(cmd,
                   "Require: com.real.retain-entity-for-setup\r\n",
                   sizeof(cmd));
    }
    ff_rtsp_send_cmd(s, "DESCRIBE", rt->control_uri, cmd, reply, &content);
    if (!content)
        return AVERROR_INVALIDDATA;
    if (reply->status_code != RTSP_STATUS_OK) {
        av_freep(&content);
        return AVERROR_INVALIDDATA;
    }

    av_log(s, AV_LOG_INFO, "SDP:\n%s\n", content);
    /* now we got the SDP description, we parse it */
    ret = ff_sdp_parse(s, (const char *)content);
    av_freep(&content);
    if (ret < 0)
        return ret;

    // rtsp demuxer unsupport the payload
    if(rt->state==RTSP_STATE_NOSUPPORT_PT){
    	av_log(s, AV_LOG_ERROR, "rtsp demuxer with playload is mpegts is not support\n");
	return AVERROR_MODULE_UNSUPPORT;
    }
    return 0;
}

static int rtsp_probe(AVProbeData *p)
{
    if (av_strstart(p->filename, "rtsp:", NULL))
        return AVPROBE_SCORE_MAX;
    return 0;
}

static int rtsp_read_header(AVFormatContext *s,
                            AVFormatParameters *ap)
{
    RTSPState *rt = s->priv_data;
    int ret;
    av_log(NULL, AV_LOG_INFO, "[%s:%d]\n", __FUNCTION__, __LINE__);
    rt->send_keepalive=0;
    ret = ff_rtsp_connect(s);
    if (ret)
        return ret;

    rt->real_setup_cache = av_mallocz(2 * s->nb_streams * sizeof(*rt->real_setup_cache));
    if (!rt->real_setup_cache)
        return AVERROR(ENOMEM);
    rt->real_setup = rt->real_setup_cache + s->nb_streams;

#if FF_API_FORMAT_PARAMETERS
    if (ap->initial_pause)
        rt->initial_pause = ap->initial_pause;
#endif

    if(!(s->flags&AVFMT_FLAG_RTSP_PROTOCOL)){
#if HAVE_PTHREADS
	RTSPRecvContext *lpRTSPCtx = rt->priv_data;
	if(NULL == lpRTSPCtx){
		lpRTSPCtx = av_mallocz(sizeof(RTSPRecvContext)) ;
		if(NULL == lpRTSPCtx)
		return -1;

		lpRTSPCtx->bufferlist.max_items = am_getconfig_float_def("libplayer.rtspdemux.maxpacket",5000);
		lpRTSPCtx->bufferlist.item_ext_buf_size = 0;   
		lpRTSPCtx->bufferlist.muti_threads_access = 1;
		lpRTSPCtx->bufferlist.reject_same_item_data = 1;  
		itemlist_init(&lpRTSPCtx->bufferlist) ;
		rt->priv_data = lpRTSPCtx;
	}
	av_log(NULL, AV_LOG_INFO, "[%s:%d]init protocol mode. max_items=%d\n", __FUNCTION__, __LINE__,lpRTSPCtx->bufferlist.max_items);
#endif
    }
    else{
    	av_log(NULL, AV_LOG_INFO, "[%s:%d]init non-protocol mode\n", __FUNCTION__, __LINE__);
    	rt->priv_data=NULL;
    }
    
    if (rt->initial_pause) {
         /* do not start immediately */
    } else {
         if (rtsp_read_play(s) < 0) {
            ff_rtsp_close_streams(s);
            ff_rtsp_close_connections(s);
            return AVERROR_INVALIDDATA;
        }
    } 

    av_log(s, AV_LOG_INFO, "[%s]transport=%d timeout=%d div=%d\n",__FUNCTION__,rt->transport,rt->timeout,rt->keeplive_div);
    return 0;
}
/*
FILE *g_dumpFile=NULL;
uint16_t last_rtp_req=0;
uint16_t last_rtcp_req=0;
static void check_header(char *lpkt_buf,int len){
	uint16_t seq2 = AV_RB16(lpkt_buf + 2);
	int16_t diff2=0;
	if (lpkt_buf[1] >= RTCP_SR && lpkt_buf[1] <= RTCP_APP) {
		diff2 = seq2 - last_rtcp_req;
		if (diff2 != 1) {
			av_log(NULL, AV_LOG_WARNING, "RTCP over tcp: diff2 = %d seq2=%d last_rtcp_req=%d type=%d\n",diff2,seq2,last_rtcp_req,lpkt_buf[1] );
		}
		last_rtcp_req=seq2;				
	}
	else{
		diff2 = seq2 - last_rtp_req;
		if (diff2 != 1) {
			av_log(NULL, AV_LOG_WARNING, "RTP over tcp: diff2 = %d seq2=%d last_rtp_req=%d\n",diff2,seq2,last_rtp_req);
		}
		last_rtp_req=seq2;		
	}
}
static void dump(char *lpkt_buf,int len){
	if (lpkt_buf[0] & 0x20){					// remove the padding data
		int padding = lpkt_buf[len - 1];
		if (len >= 12 + padding)
		    len -= padding;
	}

	if(len<=12){
		av_log(NULL, AV_LOG_ERROR, "[%s:%d]len<=12,len=%d\n",__FUNCTION__,__LINE__,len);
		return;
	}

	// output the playload data
	int offset = 12 ;
	uint8_t * lpoffset = lpkt_buf + 12;

	int ext = lpkt_buf[0] & 0x10;
	if(ext > 0){
		if(len < offset + 4){
			av_log(NULL, AV_LOG_ERROR, "[%s:%d]len < offset + 4\n",__FUNCTION__,__LINE__);
			return;
		}	

		ext = (AV_RB16(lpoffset + 2) + 1) << 2;
		if(len < ext + offset){
			av_log(NULL, AV_LOG_ERROR, "[%s:%d]len < ext + offset\n",__FUNCTION__,__LINE__);
			return;
		}	
		offset+=ext ;
		lpoffset+=ext ;
	}
	
	if(g_dumpFile==NULL)
		g_dumpFile=fopen("/data/tmp/rtsp.ts","wb");

	if(g_dumpFile)
		fwrite(lpoffset,1,len - offset,g_dumpFile);
		
}
*/
int ff_rtsp_tcp_read_packet(AVFormatContext *s, RTSPStream **prtsp_st,
                            uint8_t *buf, int buf_size)
{
    RTSPState *rt = s->priv_data;
    int id, len, i, ret;
    RTSPStream *rtsp_st;

    av_dlog(s, "tcp_read_packet:\n");
redo:
    for (;;) {
        RTSPMessageHeader reply;

        ret = ff_rtsp_read_reply(s, &reply, NULL, 1, NULL);
        if (ret < 0)
            return ret;
        if (ret == 1) /* received '$' */
            break;
        /* XXX: parse message */
        if (rt->state != RTSP_STATE_STREAMING)
            return 0;
    }
    ret = ffurl_read_complete(rt->rtsp_hd, buf, 3);
    if (ret != 3) {
        av_log(NULL, AV_LOG_ERROR, "[%s:%d] read fail, ret=%d", __FUNCTION__, __LINE__, ret);
        return ret;
    }
    id  = buf[0];
    len = AV_RB16(buf + 1);
    av_dlog(s, "id=%d len=%d\n", id, len);
    if (len > buf_size || len < 12)
        goto redo;
    /* get the data */
    ret = ffurl_read_complete(rt->rtsp_hd, buf, len);
    if (ret != len) {
        av_log(NULL, AV_LOG_ERROR, "[%s:%d] read fail, ret=%d, len=%d", __FUNCTION__, __LINE__, ret, len);
        if (ret > 0) {
            return -1;
        } else {
            return ret;
        }
    }
    if (rt->transport == RTSP_TRANSPORT_RDT &&
        ff_rdt_parse_header(buf, len, &id, NULL, NULL, NULL, NULL) < 0)
        return -1;

    /* find the matching stream */
    for (i = 0; i < rt->nb_rtsp_streams; i++) {
        rtsp_st = rt->rtsp_streams[i];
        if (id >= rtsp_st->interleaved_min &&
            id <= rtsp_st->interleaved_max)
            goto found;
    }
    goto redo;
found:
    *prtsp_st = rtsp_st;
    return len;
}

static int resetup_tcp(AVFormatContext *s)
{
    RTSPState *rt = s->priv_data;
    char host[1024];
    int port;

    av_url_split(NULL, 0, NULL, 0, host, sizeof(host), &port, NULL, 0,
                 s->filename);
    ff_rtsp_undo_setup(s);
    return ff_rtsp_make_setup_request(s, host, port, RTSP_LOWER_TRANSPORT_TCP,
                                      rt->real_challenge);
}

static void rtsp_keep_alive(AVFormatContext *s)
{
    RTSPState *rt = s->priv_data;
    int ret=0;
    if (NULL == rt)
        return;

    int keeplive_div=rt->keeplive_div>0?rt->keeplive_div:4;

    /* send dummy request to keep TCP connection alive */
    if ((av_gettime() - rt->last_cmd_time) / 1000000 >= (rt->timeout / keeplive_div) || rt->send_keepalive == 0) {
        if (rt->server_type == RTSP_SERVER_WMS ||
           (rt->server_type != RTSP_SERVER_REAL &&
            rt->get_parameter_supported)) {
            ret=ff_rtsp_send_cmd_async(s, "GET_PARAMETER", rt->control_uri, NULL);
        } else {
            ret=ff_rtsp_send_cmd_async(s, "OPTIONS", "*", NULL);
        }

        if (ret >= 0 && rt->send_keepalive == 0) {
            av_log(NULL, AV_LOG_INFO, "[%s:%d]Success to send first keep alive!\n",__FUNCTION__,__LINE__);
            rt->send_keepalive=1;
        }
    }
}

void rtsp_invoke(void *para)
{
    URLContext *h=para;
    if ((h->priv_flags & FLAGS_KEEPALIVE) && h->invoke_para != NULL) {
        AVFormatContext *s=h->invoke_para;
        rtsp_keep_alive(s);
    }
}

static int _rtsp_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    RTSPState *rt = s->priv_data;
    int ret;
    RTSPMessageHeader reply1, *reply = &reply1;
    char cmd[1024];

retry:
    if (rt->server_type == RTSP_SERVER_REAL) {
        int i;

        for (i = 0; i < s->nb_streams; i++)
            rt->real_setup[i] = s->streams[i]->discard;

        if (!rt->need_subscription) {
            if (memcmp (rt->real_setup, rt->real_setup_cache,
                        sizeof(enum AVDiscard) * s->nb_streams)) {
                snprintf(cmd, sizeof(cmd),
                         "Unsubscribe: %s\r\n",
                         rt->last_subscription);
                ff_rtsp_send_cmd(s, "SET_PARAMETER", rt->control_uri,
                                 cmd, reply, NULL);
                if (reply->status_code != RTSP_STATUS_OK)
                    return AVERROR_INVALIDDATA;
                rt->need_subscription = 1;
            }
        }

        if (rt->need_subscription) {
            int r, rule_nr, first = 1;

            memcpy(rt->real_setup_cache, rt->real_setup,
                   sizeof(enum AVDiscard) * s->nb_streams);
            rt->last_subscription[0] = 0;

            snprintf(cmd, sizeof(cmd),
                     "Subscribe: ");
            for (i = 0; i < rt->nb_rtsp_streams; i++) {
                rule_nr = 0;
                for (r = 0; r < s->nb_streams; r++) {
                    if (s->streams[r]->id == i) {
                        if (s->streams[r]->discard != AVDISCARD_ALL) {
                            if (!first)
                                av_strlcat(rt->last_subscription, ",",
                                           sizeof(rt->last_subscription));
                            ff_rdt_subscribe_rule(
                                rt->last_subscription,
                                sizeof(rt->last_subscription), i, rule_nr);
                            first = 0;
                        }
                        rule_nr++;
                    }
                }
            }
            av_strlcatf(cmd, sizeof(cmd), "%s\r\n", rt->last_subscription);
            ff_rtsp_send_cmd(s, "SET_PARAMETER", rt->control_uri,
                             cmd, reply, NULL);
            if (reply->status_code != RTSP_STATUS_OK)
                return AVERROR_INVALIDDATA;
            rt->need_subscription = 0;

            if (rt->state == RTSP_STATE_STREAMING)
                rtsp_read_play (s);
        }
    }

    if(rt->state == RTSP_STATE_PAUSED){
	ret=AVERROR(EAGAIN);
	goto END_READ;
    }

    rtsp_keep_alive(s);

    ret = ff_rtsp_fetch_packet(s, pkt);
    if (ret < 0) {
        if (ret == AVERROR(ETIMEDOUT) && !rt->packets) {
            if (rt->lower_transport == RTSP_LOWER_TRANSPORT_UDP &&
                rt->lower_transport_mask & (1 << RTSP_LOWER_TRANSPORT_TCP)) {
                RTSPMessageHeader reply1, *reply = &reply1;
                av_log(s, AV_LOG_WARNING, "UDP timeout, retrying with TCP\n");
                if (rtsp_read_pause(s) != 0)
                    return -1;
                // TEARDOWN is required on Real-RTSP, but might make
                // other servers close the connection.
                if (rt->server_type == RTSP_SERVER_REAL)
                    ff_rtsp_send_cmd(s, "TEARDOWN", rt->control_uri, NULL,
                                     reply, NULL);
                rt->session_id[0] = '\0';
                if (resetup_tcp(s) == 0) {
                    rt->state = RTSP_STATE_IDLE;
                    rt->need_subscription = 1;
                    if (rtsp_read_play(s) != 0)
                        return -1;
                    goto retry;
                }
            }
        }

        // try to reset.
        if (ret == AVERROR(ETIMEDOUT) && rt->lower_transport == RTSP_LOWER_TRANSPORT_TCP) {
            RTSPMessageHeader reply1, *reply = &reply1;
            if (rtsp_read_pause(s) != 0) {
                av_log(NULL, AV_LOG_ERROR, "[%s:%d] read pause fail !", __FUNCTION__, __LINE__);
                return -1;
            }
            ff_rtsp_send_cmd(s, "TEARDOWN", rt->control_uri, NULL, reply, NULL);
            rt->session_id[0] = '\0';
            if (resetup_tcp(s) == 0) {
                rt->state = RTSP_STATE_IDLE;
                rt->need_subscription = 1;
                if (rtsp_read_play(s) != 0) {
                    av_log(NULL, AV_LOG_ERROR, "[%s:%d] read play fail !", __FUNCTION__, __LINE__);
                    return -1;
                }
                av_log(NULL, AV_LOG_ERROR, "[%s:%d] resetup success !", __FUNCTION__, __LINE__);
                goto retry;
            }
            av_log(NULL, AV_LOG_ERROR, "[%s:%d] resetup fail !", __FUNCTION__, __LINE__);
        }
        return ret;
    }
    rt->packets++;

    ret =0;
END_READ:

    return ret;
}

static int rtsp_read_packet(AVFormatContext *s, AVPacket *pkt)
{
	RTSPState *rt = s->priv_data;
	if(NULL == rt)
		return -1;
	
    	RTSPRecvContext *lpRTSPCtx = rt->priv_data;
	if(lpRTSPCtx == NULL)
		return _rtsp_read_packet(s, pkt);
	
	unsigned long ldata = 0;
	while(lpRTSPCtx->brunning > 0) {
		if (url_interrupt_cb()) 
            		return AVERROR_EXIT;

		if(itemlist_get_head_data(&lpRTSPCtx->bufferlist, &ldata) == 0)
			break;	
		amthreadpool_thread_usleep(10);
	}

	if(ldata == 0){
		if(lpRTSPCtx->recv_buffer_error != 0){
			return lpRTSPCtx->recv_buffer_error;
		}		
		 return AVERROR(EAGAIN);
	}

	AVPacket *lpkt = (AVPacket *)ldata;
	(*pkt) = (*lpkt) ;
	pkt->destruct = av_destruct_packet;
	av_free(lpkt);
	return 0;
}

static int rtsp_read_seek(AVFormatContext *s, int stream_index,
                          int64_t timestamp, int flags)
{
    RTSPState *rt = s->priv_data;

    if(NULL == rt) 
    	return AVERROR(ENOMEM);
    if(stream_index>=0&&s->streams[stream_index]!=NULL){
       rt->seek_timestamp = av_rescale_q(timestamp,
                                      s->streams[stream_index]->time_base,
                                      AV_TIME_BASE_Q);
    }
    else{
       rt->seek_timestamp = timestamp;
    }

    av_log(NULL, AV_LOG_INFO, "[%s:%d]seek_timestamp=%lld\n",__FUNCTION__,__LINE__,rt->seek_timestamp);
    switch(rt->state) {
    default:
    case RTSP_STATE_IDLE:
        break;
    case RTSP_STATE_STREAMING:
        if (rtsp_read_pause(s) != 0)
            return -1;
       /* flush inbound media packets */
       rtsp_read_flush(s);

        rt->state = RTSP_STATE_SEEKING;
        if (rtsp_read_play(s) != 0){
	    av_log(NULL, AV_LOG_ERROR, "[%s:%d]rtsp_read_play failed\n",__FUNCTION__,__LINE__);
            return -1;
        }
        break;
    case RTSP_STATE_PAUSED:
        if (rt->playback_rate_permille == rt->playback_rate_permille_next) {
               rt->state = RTSP_STATE_IDLE;
       } else {
               /* trick mode */
               /* flush inbound media packets */
                rtsp_read_flush(s);
               rt->state = RTSP_STATE_SEEKING;
               if (rtsp_read_play(s) != 0) {
                       return -1;
               }
       }

        break;
    }
    return 0;
}

static int rtsp_read_close(AVFormatContext *s)
{
    RTSPState *rt = s->priv_data;
	RTSPMessageHeader reply1, *reply = &reply1;
#if 0
    /* NOTE: it is valid to flush the buffer here */
    if (rt->lower_transport == RTSP_LOWER_TRANSPORT_TCP) {
        avio_close(&rt->rtsp_gb);
    }
#endif
	av_log(NULL, AV_LOG_INFO, "[%s:%d\n]", __FUNCTION__, __LINE__);
    //ff_rtsp_send_cmd_async(s, "TEARDOWN", rt->control_uri, NULL);
	ff_rtsp_send_cmd(s, "TEARDOWN", rt->control_uri, NULL,
                                     reply, NULL);
    if(rt->priv_data!=NULL){
#if HAVE_PTHREADS
	RTSPRecvContext *lpRTSPCtx = rt->priv_data;
	if(lpRTSPCtx != NULL){
		lpRTSPCtx->brunning = 0;
		amthreadpool_pthread_join(lpRTSPCtx->recv_buffer_thread, NULL);
		lpRTSPCtx->recv_buffer_thread = 0;

		itemlist_clean(&lpRTSPCtx->bufferlist, free_packet);
		av_free(lpRTSPCtx);
	}	
	rt->priv_data = NULL;
#endif 
    }
    ff_rtsp_close_streams(s);
    ff_rtsp_close_connections(s);
    ff_network_close();
    rt->real_setup = NULL;
    av_freep(&rt->real_setup_cache);
    return 0;
}

static const AVOption options[] = {
    { "initial_pause",  "Don't start playing the stream immediately", offsetof(RTSPState, initial_pause),  FF_OPT_TYPE_INT, {.dbl = 0}, 0, 1, AV_OPT_FLAG_DECODING_PARAM },
    { NULL },
};

const AVClass rtsp_demuxer_class = {
    .class_name     = "RTSP demuxer",
    .item_name      = av_default_item_name,
    .option         = options,
    .version        = LIBAVUTIL_VERSION_INT,
};

AVInputFormat ff_rtsp_demuxer = {
    "rtsp",
    NULL_IF_CONFIG_SMALL("RTSP input format"),
    sizeof(RTSPState),
    rtsp_probe,
    rtsp_read_header,
    rtsp_read_packet,
    rtsp_read_close,
    rtsp_read_seek,
    .flags = AVFMT_NOFILE,
    .read_play = rtsp_read_play,
    .read_pause = rtsp_read_pause,
    .priv_class = &rtsp_demuxer_class,
};

// ----------------------------------------------------------------------------------------
// rtsp protocol. Used only by mpegts over rtsp. This is only an interface to work like an protocol.
// Add by le.yang. 2013/12/24
#include <itemlist.h>
#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

typedef struct RTSPProtocolContext {
    struct itemlist bufferlist;
    AVFormatContext *ctx;

    int dumpmode;
    volatile int brunning;
    volatile int pausing;
    int recv_prot_buffer_error;
#if HAVE_PTHREADS
    pthread_t recv_prot_buffer_thread;
#endif   
}RTSPProtocolContext;

#define RTSP_PROT_DUMP

#ifdef RTSP_PROT_DUMP
static FILE *g_dumpFile=NULL;
#endif

static void *recv_prot_buffer_task( void *_rtspCtx)
{
    RTSPProtocolContext* rtspCtx = (RTSPProtocolContext*)_rtspCtx;
    if(NULL == rtspCtx)
	return AVERROR(ENOMEM);    

    av_log(NULL, AV_LOG_ERROR, "[%s:%d] Thread start!brunning=%d pausing=%d\n", 
    			__FUNCTION__, __LINE__,rtspCtx->brunning,rtspCtx->pausing);
    AVPacket *wpkt=NULL;
    int ret=0;
    while (rtspCtx->brunning) {
        if (url_interrupt_cb()) {
            ret=AVERROR_EXIT;
            av_log(NULL, AV_LOG_ERROR, "[%s:%d]url_interrupt_cb!\n", __FUNCTION__, __LINE__);
            break;
        }

        if (rtspCtx->bufferlist.max_items>0 && rtspCtx->bufferlist.max_items <= rtspCtx->bufferlist.item_count) {
            amthreadpool_thread_usleep(10);
            continue;
        }

        if (NULL == wpkt)
            wpkt=av_mallocz(sizeof(AVPacket));

        if (NULL == wpkt) {
            amthreadpool_thread_usleep(10);
            continue;
        }

        av_init_packet(wpkt);
        ret= rtspCtx->ctx->iformat->read_packet(rtspCtx->ctx, wpkt);
        if (ret < 0) {
            av_free_packet(wpkt);
            if (ret == AVERROR(EAGAIN)) {
                if (rtspCtx->pausing == 1)
                    amthreadpool_thread_usleep(10);
                continue;
            }
            else if (ret == AVERROR_EXIT)
                av_log(NULL, AV_LOG_ERROR, "[%s:%d] AVERROR_EXIT!\n", __FUNCTION__, __LINE__);
            else if (ret == AVERROR_EOF)
                av_log(NULL, AV_LOG_ERROR, "[%s:%d] File to EOF!\n", __FUNCTION__, __LINE__);

            av_free(wpkt);
            wpkt=NULL;
            break;
        }

        if (wpkt->size <= 0) {
            av_free_packet(wpkt);
            continue;
        }

#ifdef RTSP_PROT_DUMP
        if (rtspCtx->dumpmode == 1 && g_dumpFile != NULL)
            fwrite(wpkt->data,1,wpkt->size,g_dumpFile);
#endif

        // add to the buffer list
        itemlist_add_tail_data(&(rtspCtx->bufferlist), (unsigned long) wpkt);
        wpkt=NULL;
    }

    av_log(NULL, AV_LOG_ERROR, "[%s:%d] Thread end brunning=%d ret = %d!\n", __FUNCTION__, __LINE__,rtspCtx->brunning,ret);
    if(rtspCtx->brunning==0)
        ret=AVERROR_EXIT;

    rtspCtx->recv_prot_buffer_error=ret;
    rtspCtx->brunning=0;

    return NULL;
}

static int rtsp_protocol_open(URLContext *h, const char *uri, int flags)
{
    int ret=0;
    AVDictionary *tmp = NULL;
    RTSPProtocolContext *rtspCtx = av_mallocz(sizeof(RTSPProtocolContext)) ;
    if(NULL==rtspCtx)
	return AVERROR(ENOMEM);
    
    rtspCtx->ctx=avformat_alloc_context();
    if (NULL==rtspCtx->ctx)
    	return AVERROR(ENOMEM);

    if ((ret = av_opt_set_dict(rtspCtx->ctx, &tmp)) < 0){
    	ret = AVERROR(ENOMEM);
       goto fail;
    }

    rtspCtx->ctx->iformat = &ff_rtsp_demuxer;
    
    /* allocate private data */
    if (rtspCtx->ctx->iformat->priv_data_size > 0) {
        if (!(rtspCtx->ctx->priv_data = av_mallocz(rtspCtx->ctx->iformat->priv_data_size))) {
            ret = AVERROR(ENOMEM);
            av_log(NULL, AV_LOG_ERROR, "[%s:%d\n]ENOMEM", __FUNCTION__, __LINE__);
            goto fail;
        }
        if (rtspCtx->ctx->iformat->priv_class) {
            *(const AVClass**)rtspCtx->ctx->priv_data =rtspCtx->ctx->iformat->priv_class;
            av_opt_set_defaults(rtspCtx->ctx->priv_data);
            if ((ret = av_opt_set_dict(rtspCtx->ctx->priv_data, &tmp)) < 0){
            	  av_log(NULL, AV_LOG_ERROR, "[%s:%d\n]av_opt_set_dict failed", __FUNCTION__, __LINE__);
                goto fail;
            }
        }
    } 

    AVFormatParameters ap = { 0 };
    RTSPState *rt = rtspCtx->ctx->priv_data;
    rt->use_protocol_mode=1;
    rtspCtx->ctx->flags |= AVFMT_FLAG_RTSP_PROTOCOL;
    av_strlcpy(rtspCtx->ctx->filename, uri+1, 1024);
    if ((ret = rtspCtx->ctx->iformat->read_header(rtspCtx->ctx, &ap)) < 0){
    	av_log(NULL, AV_LOG_ERROR, "[%s:%d\n]read_header failed ret=%d,filename=%s", __FUNCTION__, __LINE__,ret,rtspCtx->ctx->filename);
    	goto fail;
    }

    rtspCtx->bufferlist.max_items = am_getconfig_float_def("libplayer.rtspprotocol.maxpacket",5000);
    rtspCtx->bufferlist.item_ext_buf_size = 0;   
    rtspCtx->bufferlist.muti_threads_access = 1;
    rtspCtx->bufferlist.reject_same_item_data = 0;  
    itemlist_init(&rtspCtx->bufferlist) ;
    
    h->support_time_seek = 1;
    h->seekflags=0;
    h->is_slowmedia = 1;			
    h->is_streamed = 1;
    h->priv_data = rtspCtx;

    rtspCtx->dumpmode=0;
#ifdef RTSP_PROT_DUMP
    float value=0;
    if(am_getconfig_float("libplayer.rtsp.dumpmode", &value)==0){
	rtspCtx->dumpmode=value;
	if(rtspCtx->dumpmode>0){
	    	g_dumpFile=fopen("/data/tmp/rtspdump.ts","wb");
	    	if(NULL==g_dumpFile)
	    		av_log(NULL, AV_LOG_ERROR, "[%s:%d]g_dumpFile create null\n",__FUNCTION__,__LINE__);
	}
    }
#endif
 #if HAVE_PTHREADS
    rtspCtx->brunning = 1;
    rtspCtx->pausing=0;
    if (amthreadpool_pthread_create_name(&rtspCtx->recv_prot_buffer_thread, NULL, recv_prot_buffer_task, rtspCtx, "rtsp protocol thread")) {
	av_log(NULL, AV_LOG_ERROR, "pthread_create failed\n");
	goto fail;
    }
#endif   
    av_log(NULL, AV_LOG_INFO, "[%s:%d]open success uri=%s max_items=%d\n", __FUNCTION__, __LINE__,uri,rtspCtx->bufferlist.max_items);
    return 0;
fail:
    avformat_free_context(rtspCtx->ctx);
    av_free(rtspCtx);
    return ret;
}
static int rtsp_protocol_read(URLContext *h, uint8_t *buf, int size)
{
    RTSPProtocolContext* rtspCtx = (RTSPProtocolContext*)h->priv_data;
    if(NULL == rtspCtx)
	return AVERROR(ENOMEM);
    
    int ret = 0;
    int readsize=0;
    AVPacket *rpkt=NULL;
    AVPacket *wpkt=NULL;
/*    AVPacket *wpkt=av_mallocz(sizeof(AVPacket));
    if(NULL == wpkt)
    	return AVERROR(ENOMEM);
    
 retry_read:
    if (url_interrupt_cb()){ 
    	av_free(wpkt);
	return AVERROR_EXIT;
    }

    // add to the buffer list
    av_init_packet(wpkt);
    ret= rtspCtx->ctx->iformat->read_packet(rtspCtx->ctx, wpkt);
    if(ret < 0){
    	av_free_packet(wpkt);
    	av_free(wpkt);
   	if(ret == AVERROR(EAGAIN)) {
   		av_log(NULL, AV_LOG_ERROR, "[%s:%d] retry_read!\n", __FUNCTION__, __LINE__);    
 		goto retry_read;
   	}
   	else if(ret == AVERROR_EXIT)
   		av_log(NULL, AV_LOG_ERROR, "[%s:%d] AVERROR_EXIT!\n", __FUNCTION__, __LINE__);
   	else if(ret == AVERROR_EOF)
		av_log(NULL, AV_LOG_ERROR, "[%s:%d] File to EOF!\n", __FUNCTION__, __LINE__);
   	return ret;
    }
    if(wpkt->size<=0){
    	 av_free_packet(wpkt);
    	 goto retry_read;
    }
    itemlist_add_tail_data(&(rtspCtx->bufferlist), (unsigned long) wpkt);
*/
    //av_log(NULL, AV_LOG_ERROR, "[%s:%d]\n", __FUNCTION__, __LINE__);
    // read from the buffer list  
    int single_readsize=0;
    while(size>readsize&&rtspCtx->brunning){
    	if (url_interrupt_cb()) 
	     	return AVERROR_EXIT;
    	
    	if(itemlist_peek_head_data(&(rtspCtx->bufferlist), (unsigned long *)&rpkt) == -1){
		// no data to read, waited
		amthreadpool_thread_usleep(10);
		continue;			
	}	
	single_readsize=min(rpkt->size-rpkt->offset, size-readsize);
       memcpy(buf+readsize,rpkt->data+rpkt->offset,single_readsize);

       readsize+=single_readsize;
       rpkt->offset+=single_readsize;
       if(rpkt->offset>=rpkt->size){
       	// already read, no valid data clean it
       	itemlist_del_match_data_item(&rtspCtx->bufferlist, (unsigned long)rpkt);
		av_free_packet(rpkt);
		av_free(rpkt);
       }
       rpkt=NULL;
    }

#ifdef RTSP_PROT_DUMP
    if(rtspCtx->dumpmode==2&&g_dumpFile!=NULL)
    	fwrite(buf,1,readsize,g_dumpFile);
#endif
    if(readsize>0)
    	return readsize;

    return rtspCtx->recv_prot_buffer_error;
}
static int64_t rtsp_protocol_exseek (URLContext *h, int64_t pos, int whence)
{
       RTSPProtocolContext* rtspCtx = (RTSPProtocolContext*)h->priv_data;
       struct AVInputFormat *iformat;
       int64_t ret = AVERROR(EPIPE);

       rtspCtx = (RTSPProtocolContext*)h->priv_data;
       if((NULL == rtspCtx) || (NULL == rtspCtx->ctx) || (NULL == rtspCtx->ctx->iformat))
               return AVERROR(ENOMEM);

       iformat = rtspCtx->ctx->iformat;
       switch (whence) {
       case AVSEEK_FULLTIME:
            if (rtspCtx->ctx->duration > 0) {
                av_log(NULL, AV_LOG_INFO, "[%s:%d] duration in millis =%lld",__FILE__,__LINE__,rtspCtx->ctx->duration);
                return  (int64_t)(rtspCtx->ctx->duration/AV_TIME_BASE);
            }
            av_log(NULL, AV_LOG_ERROR, "[%s:%d] can't fetch duration",__FILE__,__LINE__);
            break;
       case AVSEEK_TO_TIME:
               if (iformat->read_seek) {
#if HAVE_PTHREADS
                    rtspCtx->brunning = 0;
                    amthreadpool_pthread_join(rtspCtx->recv_prot_buffer_thread, NULL);
                    rtspCtx->recv_prot_buffer_thread = 0;
                    rtspCtx->recv_prot_buffer_error=AVERROR(EAGAIN);
#endif

#ifdef RTSP_PROT_DUMP
                    if (g_dumpFile != NULL) {
                        fclose(g_dumpFile);
                        g_dumpFile=NULL;
                    }
#endif
                    av_log(NULL, AV_LOG_INFO, "[%s:%d]read_seek pos=%lld\n",__FUNCTION__,__LINE__,pos);
                    if (!iformat->read_seek (rtspCtx->ctx, -1, (pos*AV_TIME_BASE), NULL))
                        ret = pos;

#ifdef RTSP_PROT_DUMP
                    if (rtspCtx->dumpmode > 0) {
                        g_dumpFile=fopen("/data/tmp/rtspdump.ts","wb");
                        if (NULL == g_dumpFile)
                            av_log(NULL, AV_LOG_ERROR, "[%s:%d]g_dumpFile create null\n",__FUNCTION__,__LINE__);
                    }
#endif

#if HAVE_PTHREADS
                       rtspCtx->recv_prot_buffer_thread = 0;
                       rtspCtx->recv_prot_buffer_error=0;
                       rtspCtx->brunning = 1;
                       rtspCtx->pausing=0;
                       if (amthreadpool_pthread_create_name(&rtspCtx->recv_prot_buffer_thread, NULL, recv_prot_buffer_task, rtspCtx, "rtsp protocol thread")) {
                               av_log(NULL, AV_LOG_ERROR, "pthread_create failed\n");
                               return AVERROR(ENOMEM);
                       }
#endif
               }
               break;
       case AVSEEK_BUFFERED_TIME:
       		return 0;
       default:
               break;
       }
       return ret;
}

static int rtsp_protocol_close(URLContext *h)
{
    RTSPProtocolContext* rtspCtx = (RTSPProtocolContext*)h->priv_data;
    if(NULL == rtspCtx)
	return AVERROR(ENOMEM);
    
    av_log(NULL, AV_LOG_INFO, "[%s:%d]\n", __FUNCTION__, __LINE__);
#if HAVE_PTHREADS
    rtspCtx->brunning = 0;
    amthreadpool_pthread_join(rtspCtx->recv_prot_buffer_thread, NULL);
    rtspCtx->recv_prot_buffer_thread = 0;
    rtspCtx->recv_prot_buffer_error=0;
    rtspCtx->pausing=0;
#endif 

#ifdef RTSP_PROT_DUMP
   if(g_dumpFile!=NULL)
   	fclose(g_dumpFile);
   g_dumpFile=NULL;
#endif

    itemlist_clean(&rtspCtx->bufferlist, free_packet);
    if(rtspCtx->ctx != NULL){
    	rtspCtx->ctx->iformat->read_close(rtspCtx->ctx);
    	avformat_free_context(rtspCtx->ctx);
    }
    av_free(rtspCtx);

    return 0;
}
static int rtsp_protocol_read_pause (URLContext *h, int pause)
{
    RTSPProtocolContext* rtspCtx;
    struct AVInputFormat *iformat;

    rtspCtx = (RTSPProtocolContext*)h->priv_data;
    if((NULL == rtspCtx) || (NULL == rtspCtx->ctx) || (NULL == rtspCtx->ctx->iformat))
       return AVERROR(ENOMEM);

    iformat = rtspCtx->ctx->iformat;
    if (iformat->read_pause) {
       av_log(NULL, AV_LOG_INFO, "[%s:%d]pause=%d\n", __FUNCTION__, __LINE__,pause);
       if (pause){
#if HAVE_PTHREADS
	   rtspCtx->pausing=1;
           rtspCtx->brunning = 0;
           amthreadpool_pthread_join(rtspCtx->recv_prot_buffer_thread, NULL);
           rtspCtx->recv_prot_buffer_thread = 0;
           rtspCtx->recv_prot_buffer_error=AVERROR(EAGAIN);
#endif
           iformat->read_pause (rtspCtx->ctx);

 #if HAVE_PTHREADS
 	   rtspCtx->pausing=0;
           rtspCtx->recv_prot_buffer_thread = 0;
           rtspCtx->recv_prot_buffer_error=0;
           rtspCtx->brunning = 1;
           if (amthreadpool_pthread_create_name(&rtspCtx->recv_prot_buffer_thread, NULL, recv_prot_buffer_task, rtspCtx, "rtsp protocol thread")) {
               av_log(NULL, AV_LOG_ERROR, "pthread_create failed\n");
               return AVERROR(ENOMEM);
           }
#endif
       }
       else{
           iformat->read_play (rtspCtx->ctx);
       }

    }
    return 0;
}

static int64_t rtsp_protocol_read_seek (URLContext *h, int stream_index, int64_t timestamp, int flags)
{
    RTSPProtocolContext* rtspCtx;
    struct AVInputFormat *iformat;

    rtspCtx = (RTSPProtocolContext*)h->priv_data;
    if((NULL == rtspCtx) || (NULL == rtspCtx->ctx) || (NULL == rtspCtx->ctx->iformat))
       return AVERROR(ENOMEM);

    iformat = rtspCtx->ctx->iformat;
    if (iformat->read_seek){
#if HAVE_PTHREADS
       rtspCtx->brunning = 0;
       amthreadpool_pthread_join(rtspCtx->recv_prot_buffer_thread, NULL);
       rtspCtx->recv_prot_buffer_thread = 0;
       rtspCtx->recv_prot_buffer_error=AVERROR(EAGAIN);
#endif
       iformat->read_seek (rtspCtx->ctx, stream_index, timestamp, flags);
 #if HAVE_PTHREADS
       rtspCtx->recv_prot_buffer_thread = 0;
       rtspCtx->recv_prot_buffer_error=0;
       rtspCtx->brunning = 1;
       rtspCtx->pausing=0;
       if (amthreadpool_pthread_create_name(&rtspCtx->recv_prot_buffer_thread, NULL, recv_prot_buffer_task, rtspCtx, "rtsp protocol thread")) {
               av_log(NULL, AV_LOG_ERROR, "pthread_create failed\n");
               return AVERROR(ENOMEM);
       }
#endif

    }

    return 0;
}
static int rtsp_protocol_getinfo (URLContext *h, int cmd, int flag, void *info)
{
    RTSPProtocolContext *rtspCtx;

    rtspCtx = (RTSPProtocolContext *)h->priv_data;
    if(NULL == rtspCtx)
       return AVERROR(ENOMEM);

    switch (cmd) {
    case AVCMD_TOTAL_DURATION:
       *(int64_t *)info = rtspCtx->ctx->duration;
       av_log(NULL, AV_LOG_INFO, "[%s:%d] duration in millis =%lld",__FILE__,__LINE__,rtspCtx->ctx->duration);
       break;
    default:
       break;
    }
    return 0;
}
static int rtsp_protocol_setcmd (URLContext *h, int cmd, int flag, unsigned long info)
{
    RTSPProtocolContext *rtspCtx;
    RTSPState *rt;

    rtspCtx = (RTSPProtocolContext *)h->priv_data;
    if(NULL == rtspCtx)
               return AVERROR(ENOMEM);

    rt = rtspCtx->ctx->priv_data;
    if(NULL == rt)
       return AVERROR(ENOMEM);

    switch (cmd) {
    case AVCMD_SET_PLAYBACK_RATE_PERMILLE:
        /* flag: 1= fast-forward, 0: fast-rewind */
        if (flag)
            rt->playback_rate_permille_next = (int)info;
        else
            rt->playback_rate_permille_next = 0 - (int)info;

       break;
    default:
       break;
    }

    return 0;
}






URLProtocol ff_rtsp_protocol = {
    .name                = "rtsp",
    .url_open            = rtsp_protocol_open,
    .url_read            = rtsp_protocol_read,
    .url_write           = NULL,
    .url_seek            = NULL,
    .url_exseek          = rtsp_protocol_exseek,
    .url_close           = rtsp_protocol_close,
    .url_read_pause      = rtsp_protocol_read_pause,
    //.url_read_seek       = rtsp_protocol_read_seek,
    .url_get_file_handle = NULL,
    .url_getinfo         = rtsp_protocol_getinfo,
    .url_setcmd                 = rtsp_protocol_setcmd,
};
