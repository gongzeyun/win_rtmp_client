/*
 * RTP network protocol
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

/**
 * @file
 * RTP protocol
 */

#include "libavutil/parseutils.h"
#include "libavutil/avstring.h"
#include "avformat.h"
#include "avio_internal.h"
#include "rtpdec.h"
#include "url.h"

#include <unistd.h>
#include <stdarg.h>
#include "internal.h"
#include "network.h"
#include "os_support.h"
#include <fcntl.h>
#if HAVE_POLL_H
#include <sys/poll.h>
#endif
#include <sys/time.h>
#include "libavcodec/get_bits.h"
#include <amthreadpool.h>

#define RTP_TX_BUF_SIZE  (64 * 1024)
#define RTP_RX_BUF_SIZE  (128 * 1024)
#define RTPPROTO_RECVBUF_SIZE 3 * RTP_MAX_PACKET_LENGTH
#define MIN_CACHE_PACKET_SIZE 20

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
/**
 * If no filename is given to av_open_input_file because you want to
 * get the local port first, then you must call this function to set
 * the remote server address.
 *
 * @param h media file context
 * @param uri of the remote server
 * @return zero if no error.
 */

int rtp_set_remote_url(URLContext *h, const char *uri)
{
    RTPContext *s = h->priv_data;
    char hostname[256];
    int port;

    char buf[1024];
    char path[1024];

    av_url_split(NULL, 0, NULL, 0, hostname, sizeof(hostname), &port,
                 path, sizeof(path), uri);

    ff_url_join(buf, sizeof(buf), "udp", NULL, hostname, port, "%s", path);
    ff_udp_set_remote_url(s->rtp_hd, buf);

    ff_url_join(buf, sizeof(buf), "udp", NULL, hostname, port + 1, "%s", path);
    ff_udp_set_remote_url(s->rtcp_hd, buf);
    return 0;
}


/**
 * add option to url of the form:
 * "http://host:port/path?option1=val1&option2=val2...
 */

static void url_add_option(char *buf, int buf_size, const char *fmt, ...)
{
    char buf1[1024];
    va_list ap;

    va_start(ap, fmt);
    if (strchr(buf, '?'))
        av_strlcat(buf, "&", buf_size);
    else
        av_strlcat(buf, "?", buf_size);
    vsnprintf(buf1, sizeof(buf1), fmt, ap);
    av_strlcat(buf, buf1, buf_size);
    va_end(ap);
}

static void build_udp_url(char *buf, int buf_size,
                          const char *hostname, int port,
                          int local_port, int ttl,
                          int max_packet_size, int connect,int setbufsize)
{
    ff_url_join(buf, buf_size, "udp", NULL, hostname, port, NULL);
    if (local_port >= 0)
        url_add_option(buf, buf_size, "localport=%d", local_port);
    if (ttl >= 0)
        url_add_option(buf, buf_size, "ttl=%d", ttl);
    if (max_packet_size >=0)
        url_add_option(buf, buf_size, "pkt_size=%d", max_packet_size);
    if (connect)
        url_add_option(buf, buf_size, "connect=1");
    if(setbufsize > 0)
    	 url_add_option(buf, buf_size, "buffer_size=655360");

    url_add_option(buf, buf_size, "buf_size=0");
}

#define MAX_RTP_SEQ 65536
#define MAX_RTP_SEQ_SPAN 60000
static int seq_greater(int first,int second){
	if(first==second){
		return 0;
	}
	else if(abs(first-second)>MAX_RTP_SEQ_SPAN){
		if(first<second)
			return 1;
		else
			return 0;
	}
	else if(first>second){
		return 1;
	}
	else 
		return 0;

}

static int seq_less(int first,int second){
	if(first==second){
		return 0;
	}
	else if(abs(first-second)>MAX_RTP_SEQ_SPAN){
		if(first>second)
			return 1;
		else
			return 0;
	}
	else if(first<second){
		return 1;
	}
	else 
		return 0;

}

static int seq_greater_and_equal(int first,int second){
	if(first==second)
		return 1;
	else
		return seq_greater(first,second);
}

static int seq_less_and_equal(int first,int second){
	if(first==second)
		return 1;
	else
		return seq_less(first,second);
}

static int seq_subtraction(int first,int second){
	if(first==second){
		return 0;
	}
	else if(abs(first-second)>MAX_RTP_SEQ_SPAN&&first<second){
		return first+MAX_RTP_SEQ-second;
	}
	else {
		return first-second;
	}
}

static int rtp_free_packet(void * apkt)
{
    RTPPacket * lpkt = apkt;
    if(lpkt != NULL){
    	if(lpkt->buf != NULL)
    		av_free(lpkt->buf);
    	av_free(lpkt);
    }
    apkt = NULL;
    return 0;
}

static int inner_rtp_read(RTPContext *s, uint8_t *buf, int size)
{
    struct sockaddr_storage from;
    socklen_t from_len;
    int len, n;
    struct pollfd p[2] = {{s->rtp_fd, POLLIN, 0}, {s->rtcp_fd, POLLIN, 0}};

    for(;;) {
        if (url_interrupt_cb())
            return AVERROR_EXIT;
        /* build fdset to listen to RTP and RTCP packets */
        n = poll(p, 2, 100);
        if (n > 0) {
            /* first try RTCP */
            if (p[1].revents & POLLIN) {
                from_len = sizeof(from);
                len = recvfrom (s->rtcp_fd, buf, size, 0,
                                (struct sockaddr *)&from, &from_len);
                if (len < 0) {
                    if (ff_neterrno() == AVERROR(EAGAIN) ||
                        ff_neterrno() == AVERROR(EINTR))
                        continue;
                    return AVERROR(EIO);
                }
                break;
            }
            /* then RTP */
            if (p[0].revents & POLLIN) {
                from_len = sizeof(from);
                len = recvfrom (s->rtp_fd, buf, size, 0,
                                (struct sockaddr *)&from, &from_len);
                if (len < 0) {
                    if (ff_neterrno() == AVERROR(EAGAIN) ||
                        ff_neterrno() == AVERROR(EINTR))
                        continue;
                    return AVERROR(EIO);
                }                
                break;
            }
        } else if (n < 0) {
            if (ff_neterrno() == AVERROR(EINTR))
                continue;
            return AVERROR(EIO);
        }
    }

    return len;
}

static int inner_rtp_read1(RTPContext *s, uint8_t *buf, int size)
{
    struct sockaddr_storage from;
    socklen_t from_len;
    int len, n;
    struct pollfd p[1] = {{s->rtp_fd, POLLIN, 0}};

    for(;;) {
        if (url_interrupt_cb())
            return AVERROR_EXIT;
        /* build fdset to listen to only RTP packets */
        n = poll(p, 1, 100);
        if (n > 0) {
            /* then RTP */
            if (p[0].revents & POLLIN) {
                from_len = sizeof(from);
                len = recvfrom (s->rtp_fd, buf, size, 0,
                                (struct sockaddr *)&from, &from_len);
                if (len < 0) {
                    if (ff_neterrno() == AVERROR(EAGAIN) ||
                        ff_neterrno() == AVERROR(EINTR))
                        continue;
                    return AVERROR(EIO);
                }                
                break;
            }
        } else if (n < 0) {
            if (ff_neterrno() == AVERROR(EINTR))
                continue;
            return AVERROR(EIO);
        }
    }

    return len;
}


static int rtp_enqueue_packet(struct itemlist *itemlist, RTPPacket * lpkt){
    RTPPacket *ltailpkt=NULL;	
    itemlist_peek_tail_data(itemlist, (unsigned long)&ltailpkt) ;
    if(NULL == ltailpkt || (ltailpkt != NULL &&seq_less(ltailpkt->seq,lpkt->seq)==1)){
    	// append to the tail
	itemlist_add_tail_data(itemlist, (unsigned long)lpkt) ;  
    	return 0;
    }

    RTPPacket *lheadpkt=NULL;	
    itemlist_peek_tail_data(itemlist, (unsigned long)&lheadpkt) ;
    ITEM_LOCK(itemlist);
    if(itemlist->item_count>=MIN_CACHE_PACKET_SIZE&&lheadpkt!=NULL&&seq_greater(lheadpkt->seq,lpkt->seq)==1){
    	ITEM_UNLOCK(itemlist);    
    	av_log(NULL, AV_LOG_INFO, "[%s:%d]Out of range, seq=%d,headseq=%d\n", __FUNCTION__, __LINE__,lpkt->seq,lheadpkt->seq);
    	rtp_free_packet((void *)lpkt);
	lpkt=NULL;
	return 0;
    }
    ITEM_UNLOCK(itemlist);    

    // insert to the queue
    struct item *item = NULL;
    struct item *newitem = NULL;
    struct list_head *llist=NULL, *tmplist=NULL;
    RTPPacket *llistpkt=NULL;

    ITEM_LOCK(itemlist);
    list_for_each_safe(llist, tmplist, &itemlist->list) {
	item = list_entry(llist, struct item, list);
	llistpkt = (RTPPacket *)(item->item_data);
	if(lpkt->seq == llistpkt->seq){
		av_log(NULL, AV_LOG_INFO, "[%s:%d]The Replication packet, seq=%d\n", __FUNCTION__, __LINE__,lpkt->seq);
		rtp_free_packet((void *)lpkt);
		lpkt=NULL;
		break;
	}
	else if (seq_less(lpkt->seq,llistpkt->seq)==1) {
		// insert to front
		newitem = item_alloc(itemlist->item_ext_buf_size);
    		if (newitem == NULL) {
    			ITEM_UNLOCK(itemlist);    
       		return -12;//noMEM
    		}
		newitem->item_data = (unsigned long)lpkt;
		
		list_add_tail(&(newitem->list), &(item->list));
		itemlist->item_count++;
	      	break;
	}
    }
    ITEM_UNLOCK(itemlist);    

    return 0;	
}
/*
FILE *g_dumpFile=NULL;
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
		g_dumpFile=fopen("/data/tmp/rtp1.ts","wb");

	if(g_dumpFile)
		fwrite(lpoffset,1,len - offset,g_dumpFile);
		
}
*/

static void *rtp_recv_task( void *_RTPContext)
{
    av_log(NULL, AV_LOG_INFO, "[%s:%d]rtp recv_buffer_task start running!!!\n", __FUNCTION__, __LINE__);
    RTPContext * s=(RTPContext *)_RTPContext;
    if(NULL == s){
    	av_log(NULL, AV_LOG_INFO, "[%s:%d]Null handle!!!\n", __FUNCTION__, __LINE__);
    	goto rtp_thread_end;
    }

    RTPPacket * lpkt = NULL;
    int datalen=0 ;
    int payload_type=0; 

    uint8_t * lpoffset=NULL;
    int offset=0;
    uint8_t * lpkt_buf=NULL;
    int len=0;
    int ext=0;
    
    while(s->brunning > 0) {
	if (url_interrupt_cb()) {
            	goto rtp_thread_end;
       }

	if(lpkt != NULL){
		rtp_free_packet((void *)lpkt);
		lpkt=NULL;
	}
	
	// malloc the packet buffer
       lpkt = av_mallocz(sizeof(RTPPacket));
       if(NULL == lpkt)
       	goto rtp_thread_end;
       lpkt->buf= av_malloc(RTPPROTO_RECVBUF_SIZE);
       if(NULL == lpkt->buf)
       	goto rtp_thread_end;

       // recv data
	lpkt->len = inner_rtp_read1(s, lpkt->buf, RTPPROTO_RECVBUF_SIZE);
	if(lpkt->len <=12){
		av_log(NULL, AV_LOG_INFO, "[%s:%d]receive wrong packet len=%d \n", __FUNCTION__, __LINE__,lpkt->len);
		amthreadpool_thread_usleep(10);
		continue;	
	} 
	
	// paser data and buffer the packat
	payload_type = lpkt->buf[1] & 0x7f;
	lpkt->seq = AV_RB16(lpkt->buf + 2);

	if(lpkt->len < 1000)
		av_log(NULL, AV_LOG_INFO, "[%s:%d]receive short packet len=%d seq=%d\n", __FUNCTION__, __LINE__,lpkt->len,lpkt->seq);

       if(payload_type == 33){		// mpegts packet
		//av_log(NULL, AV_LOG_ERROR, "[%s:%d]mpegts packet req = %d\n", __FUNCTION__, __LINE__, lpkt->seq);	
		// parse the rtp playload data  
		lpkt_buf=lpkt->buf;
		len=lpkt->len;
	
		if (lpkt_buf[0] & 0x20){					// remove the padding data
			int padding = lpkt_buf[len - 1];
			if (len >= 12 + padding)
			    len -= padding;
		}

		if(len<=12){
			av_log(NULL, AV_LOG_ERROR, "[%s:%d]len<=12,len=%d\n",__FUNCTION__,__LINE__,len);
			continue;
		}

		// output the playload data
		offset = 12 ;
		lpoffset = lpkt_buf + 12;

		ext = lpkt_buf[0] & 0x10;
		if(ext > 0){
			if(len < offset + 4){
				av_log(NULL, AV_LOG_ERROR, "[%s:%d]len < offset + 4\n",__FUNCTION__,__LINE__);
				continue;
			}	

			ext = (AV_RB16(lpoffset + 2) + 1) << 2;
			if(len < ext + offset){
				av_log(NULL, AV_LOG_ERROR, "[%s:%d]len < ext + offset\n",__FUNCTION__,__LINE__);
				continue;
			}	
			offset+=ext ;
			lpoffset+=ext ;
		}
		lpkt->valid_data_offset=offset;
		
		if(rtp_enqueue_packet(&(s->recvlist), lpkt)<0)
			goto rtp_thread_end;
	}
	else{
		av_log(NULL, AV_LOG_ERROR, "[%s:%d]unknow payload type = %d, seq=%d\n", __FUNCTION__, __LINE__, payload_type,lpkt->seq);
		continue;
	}

	lpkt = NULL;
   }

rtp_thread_end:    
    s->brunning =0;
    av_log(NULL, AV_LOG_ERROR, "[%s:%d]rtp recv_buffer_task end!!!\n", __FUNCTION__, __LINE__);
    return NULL;	
}

/**
 * url syntax: rtp://host:port[?option=val...]
 * option: 'ttl=n'            : set the ttl value (for multicast only)
 *         'rtcpport=n'       : set the remote rtcp port to n
 *         'localrtpport=n'   : set the local rtp port to n
 *         'localrtcpport=n'  : set the local rtcp port to n
 *         'pkt_size=n'       : set max packet size
 *         'connect=0/1'      : do a connect() on the UDP socket
 * deprecated option:
 *         'localport=n'      : set the local port to n
 *
 * if rtcpport isn't set the rtcp port will be the rtp port + 1
 * if local rtp port isn't set any available port will be used for the local
 * rtp and rtcp ports
 * if the local rtcp port is not set it will be the local rtp port + 1
 */

static int rtp_open(URLContext *h, const char *uri, int flags)
{
    RTPContext *s;
    int rtp_port, rtcp_port,
        ttl, connect,
        local_rtp_port, local_rtcp_port, max_packet_size; 
    char hostname[256];
    char buf[1024];
    char path[1024];
    const char *p;
    av_log(NULL, AV_LOG_INFO, "rtp_open %s\n", uri);
    s = av_mallocz(sizeof(RTPContext));
    if (!s)
        return AVERROR(ENOMEM);
    h->priv_data = s;

    av_url_split(NULL, 0, NULL, 0, hostname, sizeof(hostname), &rtp_port,
                 path, sizeof(path), uri);
    /* extract parameters */
    ttl = -1;
    rtcp_port = rtp_port+1;
    local_rtp_port = -1;
    local_rtcp_port = -1;
    max_packet_size = -1;
    connect = 0;

    p = strchr(uri, '?');
    if (p) {
        if (av_find_info_tag(buf, sizeof(buf), "ttl", p)) {
            ttl = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "rtcpport", p)) {
            rtcp_port = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "localport", p)) {
            local_rtp_port = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "localrtpport", p)) {
            local_rtp_port = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "localrtcpport", p)) {
            local_rtcp_port = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "pkt_size", p)) {
            max_packet_size = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "connect", p)) {
            connect = strtol(buf, NULL, 10);
        }/*
        if (av_find_info_tag(buf, sizeof(buf), "use_cache", p)) {
            s->use_cache = strtol(buf, NULL, 10);
        }  */
    }
    s->use_cache =(flags&AVIO_FLAG_CACHE);

    build_udp_url(buf, sizeof(buf),
                  hostname, rtp_port, local_rtp_port, ttl, max_packet_size,
                  connect,1);
    av_log(NULL, AV_LOG_INFO, "[%s:%d]Setup udp session:%s\n",__FUNCTION__,__LINE__,buf);
    if (ffurl_open(&s->rtp_hd, buf, flags) < 0)
        goto fail;
    /* just to ease handle access. XXX: need to suppress direct handle
       access */    
    s->rtp_fd = ffurl_get_file_handle(s->rtp_hd);

    if(!s->use_cache) {   
	if (local_rtp_port>=0 && local_rtcp_port<0)
	    local_rtcp_port = ff_udp_get_local_port(s->rtp_hd) + 1;

	build_udp_url(buf, sizeof(buf),
	              hostname, rtcp_port, local_rtcp_port, ttl, max_packet_size,
	              connect,0);
	av_log(NULL, AV_LOG_INFO, "[%s:%d]Setup udp session:%s\n",__FUNCTION__,__LINE__,buf);
	if (ffurl_open(&s->rtcp_hd, buf, flags) < 0)
	    goto fail;
	/* just to ease handle access. XXX: need to suppress direct handle
	   access */
	s->rtcp_fd = ffurl_get_file_handle(s->rtcp_hd);
    }
    
    if(s->use_cache){
	s->recvlist.max_items = 0;
	s->recvlist.item_ext_buf_size = 0;   
	s->recvlist.muti_threads_access = 1;
	s->recvlist.reject_same_item_data = 0;  
	itemlist_init(&s->recvlist) ;

	s->brunning = 1;
       av_log(NULL, AV_LOG_INFO, "[%s:%d]use cache mode\n",__FUNCTION__,__LINE__);
       if (amthreadpool_pthread_create_name(&(s->recv_thread), NULL, rtp_recv_task, s,"ffmpeg_rtp")) {
		av_log(NULL, AV_LOG_ERROR, "[%s:%d]ffmpeg_pthread_create failed\n",__FUNCTION__,__LINE__);
		goto fail;
       }
    }
    h->max_packet_size = s->rtp_hd->max_packet_size;
    h->is_streamed = 1;
    h->is_slowmedia = 1;
    
    return 0;

 fail:
    if (s->rtp_hd)
        ffurl_close(s->rtp_hd);
    if (s->rtcp_hd)
        ffurl_close(s->rtcp_hd);
    av_free(s);
    return AVERROR(EIO);
}
/*
FILE *g_dumpFile=NULL;
static void dumpFile(char *buf,int len){
	if(g_dumpFile==NULL)
		g_dumpFile=fopen("/data/tmp/rtp.ts","wb");

	if(g_dumpFile)
		fwrite(buf,1,len,g_dumpFile);
}

*/
static int rtp_read(URLContext *h, uint8_t *buf, int size)
{
    RTPContext *s = h->priv_data;

    if(s->use_cache){
      	RTPPacket *lpkt = NULL;
      	//uint8_t * lpkt_buf=NULL;
      	//int len=0;
      	int readsize=0;
      	int single_readsize=0;
      	while(s->brunning > 0 && size>readsize) {
		if (url_interrupt_cb()) 
			return AVERROR(EIO);


		if(s->recvlist.item_count<=MIN_CACHE_PACKET_SIZE){
			amthreadpool_thread_usleep(10);
			continue;
		}

		if(itemlist_peek_head_data(&(s->recvlist), (unsigned long)&lpkt) != 0 || lpkt == NULL){
			amthreadpool_thread_usleep(10);
			continue;
		}

		single_readsize=min(lpkt->len-lpkt->valid_data_offset, size-readsize);
	       memcpy(buf+readsize,lpkt->buf+lpkt->valid_data_offset,single_readsize);

	       readsize+=single_readsize;
	       lpkt->valid_data_offset+=single_readsize;
	       if(lpkt->valid_data_offset>=lpkt->len){       	
	       	if((s->last_seq+1)%MAX_RTP_SEQ != lpkt->seq){
				av_log(NULL, AV_LOG_ERROR, "[%s:%d]discontinuity seq=%d, the right seq=%d\n",__FUNCTION__,__LINE__, lpkt->seq,(s->last_seq+1)%MAX_RTP_SEQ);
			}
			s->last_seq=lpkt->seq;

			// already read, no valid data clean it
	       	itemlist_del_match_data_item(&(s->recvlist), (unsigned long)lpkt);   	
			rtp_free_packet((void *)lpkt);
			lpkt=NULL;
	       }
      	}

	return readsize;
    }
    else{
	return inner_rtp_read(s,buf,size);
    }
}
/*
static int rtp_read(URLContext *h, uint8_t *buf, int size)
{
    RTPContext *s = h->priv_data;
    struct sockaddr_storage from;
    socklen_t from_len;
    int len, n;
    struct pollfd p[2] = {{s->rtp_fd, POLLIN, 0}, {s->rtcp_fd, POLLIN, 0}};

#if 0
    for(;;) {
        from_len = sizeof(from);
        len = recvfrom (s->rtp_fd, buf, size, 0,
                        (struct sockaddr *)&from, &from_len);
        if (len < 0) {
            if (ff_neterrno() == AVERROR(EAGAIN) ||
                ff_neterrno() == AVERROR(EINTR))
                continue;
            return AVERROR(EIO);
        }
        break;
    }
#else
    for(;;) {
        if (url_interrupt_cb())
            return AVERROR_EXIT;
        // build fdset to listen to RTP and RTCP packets 
        n = poll(p, 2, 100);
        if (n > 0) {
            //first try RTCP 
            if (p[1].revents & POLLIN) {
                from_len = sizeof(from);
                len = recvfrom (s->rtcp_fd, buf, size, 0,
                                (struct sockaddr *)&from, &from_len);
                if (len < 0) {
                    if (ff_neterrno() == AVERROR(EAGAIN) ||
                        ff_neterrno() == AVERROR(EINTR))
                        continue;
                    return AVERROR(EIO);
                }
                break;
            }
            // then RTP 
            if (p[0].revents & POLLIN) {
                from_len = sizeof(from);
                len = recvfrom (s->rtp_fd, buf, size, 0,
                                (struct sockaddr *)&from, &from_len);
                if (len < 0) {
                    if (ff_neterrno() == AVERROR(EAGAIN) ||
                        ff_neterrno() == AVERROR(EINTR))
                        continue;
                    return AVERROR(EIO);
                }

                // ----------------------------------------------
                // if payload type is mpegts,  cut off the rtp header to output
                if(len <= 12)
			goto BREAK_POS ;
           
                if((buf[1] & 0x7f) == 33 && h->priv_flags == 1)	// payload_type
                {
                	int offset = 12 ;
		  	uint8_t * lpoffset = buf + 12;

                	int ext = buf[0] & 0x10;
                	if(ext > 0)
                	{
                		if(len < offset + 4)
 					goto BREAK_POS ;
                		
                		ext = (AV_RB16(lpoffset + 2) + 1) << 2;
                    		if(len < ext + offset)
					goto BREAK_POS ; 
                    		
                    		offset+=ext ;
                    		lpoffset+=ext ;
                	}

			memmove(buf, lpoffset, len - offset) ;
			len -= offset ;
                }
                
BREAK_POS:                
                break;
            }
        } else if (n < 0) {
            if (ff_neterrno() == AVERROR(EINTR))
                continue;
            return AVERROR(EIO);
        }
    }
#endif
    return len;
}
*/
static int rtp_write(URLContext *h, const uint8_t *buf, int size)
{
    RTPContext *s = h->priv_data;
    int ret;
    URLContext *hd;

    if (buf[1] >= RTCP_SR && buf[1] <= RTCP_APP) {
        /* RTCP payload type */
        hd = s->rtcp_hd;
    } else {
        /* RTP payload type */
        hd = s->rtp_hd;
    }

    ret = ffurl_write(hd, buf, size);
#if 0
    {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 10 * 1000000;
        nanosleep(&ts, NULL);
    }
#endif
    return ret;
}

static int rtp_close(URLContext *h)
{
    RTPContext *s = h->priv_data;

    if(s->use_cache){
	s->brunning = 0;
	amthreadpool_pthread_join(s->recv_thread, NULL);
	s->recv_thread = 0;

	itemlist_clean(&s->recvlist, rtp_free_packet);
    }

    if (s->rtp_hd)
    	ffurl_close(s->rtp_hd);
    if (s->rtcp_hd)
    	ffurl_close(s->rtcp_hd);
    av_free(s);
    return 0;
}

/**
 * Return the local rtp port used by the RTP connection
 * @param h media file context
 * @return the local port number
 */

int rtp_get_local_rtp_port(URLContext *h)
{
    RTPContext *s = h->priv_data;
    return ff_udp_get_local_port(s->rtp_hd);
}

/**
 * Return the local rtcp port used by the RTP connection
 * @param h media file context
 * @return the local port number
 */

int rtp_get_local_rtcp_port(URLContext *h)
{
    RTPContext *s = h->priv_data;
    return ff_udp_get_local_port(s->rtcp_hd);
}

static int rtp_get_file_handle(URLContext *h)
{
    RTPContext *s = h->priv_data;
    return s->rtp_fd;
}

int rtp_get_rtcp_file_handle(URLContext *h) {
    RTPContext *s = h->priv_data;
    return s->rtcp_fd;
}

URLProtocol ff_rtp_protocol = {
    .name                = "rtp",
    .url_open            = rtp_open,
    .url_read            = rtp_read,
    .url_write           = rtp_write,
    .url_close           = rtp_close,
    .url_get_file_handle = rtp_get_file_handle,
};
