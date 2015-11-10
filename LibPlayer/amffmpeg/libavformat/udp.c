/*
 * UDP prototype streaming system
 * Copyright (c) 2000, 2001, 2002 Fabrice Bellard
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
 * UDP protocol
 */

#define _BSD_SOURCE     /* Needed for using struct ip_mreq with recent glibc */

#include "avformat.h"
#include "avio_internal.h"
#include "libavutil/parseutils.h"
#include "libavutil/fifo.h"
#include <unistd.h>
#include "internal.h"
#include "network.h"
#include "os_support.h"
#include "url.h"
#include <amthreadpool.h>

#if HAVE_PTHREADS
#include <pthread.h>
#endif

#include <sys/time.h>

#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

typedef struct {
    int udp_fd;
    int ttl;
    int buffer_size;
    int is_multicast;
    int local_port;
    int reuse_socket;
    struct sockaddr_storage dest_addr;
    int dest_addr_len;
    int is_connected;

    /* Circular Buffer variables for use in UDP receive code */
    int circular_buffer_size;
    AVFifoBuffer *fifo;
    int circular_buffer_error;
#if HAVE_PTHREADS
    pthread_t circular_buffer_thread;
    pthread_mutex_t pthread_mutex;
    int request_exit;
#endif
} UDPContext;

#define IPPROTO_IPV6 41
#define IPV6_MULTICAST_HOPS 18


#define UDP_TX_BUF_SIZE 32768
#define UDP_MAX_PKT_SIZE 65536

static int udp_set_multicast_ttl(int sockfd, int mcastTTL,
                                 struct sockaddr *addr)
{
#ifdef IP_MULTICAST_TTL
    if (addr->sa_family == AF_INET) {
        if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &mcastTTL, sizeof(mcastTTL)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "setsockopt(IP_MULTICAST_TTL): %s\n", strerror(errno));
            return -1;
        }
    }
#endif
#if defined(IPPROTO_IPV6) && defined(IPV6_MULTICAST_HOPS)
    if (addr->sa_family == AF_INET6) {
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &mcastTTL, sizeof(mcastTTL)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "setsockopt(IPV6_MULTICAST_HOPS): %s\n", strerror(errno));
            return -1;
        }
    }
#endif
    return 0;
}

static int udp_join_multicast_group(int sockfd, struct sockaddr *addr)
{
#ifdef IP_ADD_MEMBERSHIP
    if (addr->sa_family == AF_INET) {
        struct ip_mreq mreq;

        mreq.imr_multiaddr.s_addr = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
        mreq.imr_interface.s_addr= INADDR_ANY;
        if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const void *)&mreq, sizeof(mreq)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "setsockopt(IP_ADD_MEMBERSHIP): %s\n", strerror(errno));
            return -1;
        }
    }
#endif
#if HAVE_STRUCT_IPV6_MREQ && defined(IPPROTO_IPV6)
    if (addr->sa_family == AF_INET6) {
        struct ipv6_mreq mreq6;
    	av_log(NULL,AV_LOG_INFO,"IPV6_ADD_MEMBERSHIP\n");
        memcpy(&mreq6.ipv6mr_multiaddr, &(((struct sockaddr_in6 *)addr)->sin6_addr), sizeof(struct in6_addr));
        mreq6.ipv6mr_interface= 0;
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(mreq6)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "setsockopt(IPV6_ADD_MEMBERSHIP): %s\n", strerror(errno));
            return -1;
        }
    }
#endif
    return 0;
}

static int udp_leave_multicast_group(int sockfd, struct sockaddr *addr)
{
#ifdef IP_DROP_MEMBERSHIP
    if (addr->sa_family == AF_INET) {
        struct ip_mreq mreq;

        mreq.imr_multiaddr.s_addr = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
        mreq.imr_interface.s_addr= INADDR_ANY;
        if (setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const void *)&mreq, sizeof(mreq)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "setsockopt(IP_DROP_MEMBERSHIP): %s\n", strerror(errno));
            return -1;
        }
    }
#endif
#if HAVE_STRUCT_IPV6_MREQ && defined(IPPROTO_IPV6)
    if (addr->sa_family == AF_INET6) {
        struct ipv6_mreq mreq6;

        memcpy(&mreq6.ipv6mr_multiaddr, &(((struct sockaddr_in6 *)addr)->sin6_addr), sizeof(struct in6_addr));
        mreq6.ipv6mr_interface= 0;
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, &mreq6, sizeof(mreq6)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "setsockopt(IPV6_DROP_MEMBERSHIP): %s\n", strerror(errno));
            return -1;
        }
    }
#endif
    return 0;
}

static struct addrinfo* udp_resolve_host(const char *hostname, int port,
                                         int type, int family, int flags)
{
    struct addrinfo hints, *res = 0;
    int error;
    char sport[16];
    const char *node = 0, *service = "0";

    if (port > 0) {
        snprintf(sport, sizeof(sport), "%d", port);
        service = sport;
    }
    if ((hostname) && (hostname[0] != '\0') && (hostname[0] != '?')) {
        node = hostname;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = type;
    hints.ai_family   = family;
    hints.ai_flags = flags;
    av_log(NULL,AV_LOG_INFO,"try get addr info,node=%s,services=%s\n",node,service);
    if ((error = getaddrinfo(node, service, &hints, &res))) {
        res = NULL;
        av_log(NULL, AV_LOG_ERROR, "udp_resolve_host: %s\n", gai_strerror(error));
    }

    return res;
}

static int udp_set_url(struct sockaddr_storage *addr,
                       const char *hostname, int port)
{
    struct addrinfo *res0;
    int addr_len;
    int family = AF_UNSPEC;
    if(am_getconfig_bool_def("media.libplayer.ipv4only",1))	
		family = AF_INET;
    res0 = udp_resolve_host(hostname, port, SOCK_DGRAM, family, 0);
    if (res0 == 0) return AVERROR(EIO);
    memcpy(addr, res0->ai_addr, res0->ai_addrlen);
    addr_len = res0->ai_addrlen;
    freeaddrinfo(res0);

    return addr_len;
}

static int udp_socket_create(UDPContext *s,
                             struct sockaddr_storage *addr, int *addr_len)
{
    int udp_fd = -1;
    struct addrinfo *res0 = NULL, *res = NULL;
    int family = AF_UNSPEC;
    if(am_getconfig_bool_def("media.libplayer.ipv4only",1))	
		family = AF_INET;
    if (((struct sockaddr *) &s->dest_addr)->sa_family)
        family = ((struct sockaddr *) &s->dest_addr)->sa_family;
    res0 = udp_resolve_host(0, s->local_port, SOCK_DGRAM, family, AI_PASSIVE);
    if (res0 == 0)
        goto fail;
    for (res = res0; res; res=res->ai_next) {
        udp_fd = socket(res->ai_family, SOCK_DGRAM, 0);
        if (udp_fd > 0) break;
        av_log(NULL, AV_LOG_ERROR, "socket: %s\n", strerror(errno));
    }

    if (udp_fd < 0)
        goto fail;

    memcpy(addr, res->ai_addr, res->ai_addrlen);
    *addr_len = res->ai_addrlen;

    freeaddrinfo(res0);

    return udp_fd;

 fail:
    if (udp_fd >= 0)
        closesocket(udp_fd);
    if(res0)
        freeaddrinfo(res0);
    return -1;
}

static int udp_port(struct sockaddr_storage *addr, int addr_len)
{
    char sbuf[sizeof(int)*3+1];

    if (getnameinfo((struct sockaddr *)addr, addr_len, NULL, 0,  sbuf, sizeof(sbuf), NI_NUMERICSERV) != 0) {
        av_log(NULL, AV_LOG_ERROR, "getnameinfo: %s\n", strerror(errno));
        return -1;
    }

    return strtol(sbuf, NULL, 10);
}


/**
 * If no filename is given to av_open_input_file because you want to
 * get the local port first, then you must call this function to set
 * the remote server address.
 *
 * url syntax: udp://host:port[?option=val...]
 * option: 'ttl=n'       : set the ttl value (for multicast only)
 *         'localport=n' : set the local port
 *         'pkt_size=n'  : set max packet size
 *         'reuse=1'     : enable reusing the socket
 *
 * @param h media file context
 * @param uri of the remote server
 * @return zero if no error.
 */
int ff_udp_set_remote_url(URLContext *h, const char *uri)
{
    UDPContext *s = h->priv_data;
    char hostname[256], buf[10];
    int port;
    const char *p;

    av_url_split(NULL, 0, NULL, 0, hostname, sizeof(hostname), &port, NULL, 0, uri);

    /* set the destination address */
    s->dest_addr_len = udp_set_url(&s->dest_addr, hostname, port);
    if (s->dest_addr_len < 0) {
        return AVERROR(EIO);
    }
    s->is_multicast = ff_is_multicast_address((struct sockaddr*) &s->dest_addr);
    p = strchr(uri, '?');
    if (p) {
        if (av_find_info_tag(buf, sizeof(buf), "connect", p)) {
            int was_connected = s->is_connected;
            s->is_connected = strtol(buf, NULL, 10);
            if (s->is_connected && !was_connected) {
                if (connect(s->udp_fd, (struct sockaddr *) &s->dest_addr,
                            s->dest_addr_len)) {
                    s->is_connected = 0;
                    av_log(h, AV_LOG_ERROR, "connect: %s\n", strerror(errno));
                    return AVERROR(EIO);
                }
            }
        }
    }

    return 0;
}

/**
 * Return the local port used by the UDP connection
 * @param h media file context
 * @return the local port number
 */
int ff_udp_get_local_port(URLContext *h)
{
    UDPContext *s = h->priv_data;
    return s->local_port;
}

/**
 * Return the udp file handle for select() usage to wait for several RTP
 * streams at the same time.
 * @param h media file context
 */
static int udp_get_file_handle(URLContext *h)
{
    UDPContext *s = h->priv_data;
    return s->udp_fd;
}

static void *circular_buffer_task( void *_URLContext)
{
    URLContext *h = _URLContext;
    UDPContext *s = h->priv_data;
    fd_set rfds;
    struct timeval tv;
    av_log(h, AV_LOG_INFO, "[%s:%d]Task starting!!!\n",__FUNCTION__,__LINE__);
    for(;;) {
        int left;
        int ret;
        int len;
	if (s->request_exit || url_interrupt_cb()) {
            s->circular_buffer_error = EINTR;
            av_log(h, AV_LOG_INFO, "[%s:%d]Eixt\n",__FUNCTION__,__LINE__);
            return NULL;
        }

        FD_ZERO(&rfds);
        FD_SET(s->udp_fd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        ret = select(s->udp_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (ff_neterrno() == AVERROR(EINTR))
                continue;
            s->circular_buffer_error = EIO;
            av_log(h, AV_LOG_INFO, "[%s:%d]EIO\n",__FUNCTION__,__LINE__);
            amthreadpool_thread_usleep(10);
            continue;
        }

        if (!(ret > 0 && FD_ISSET(s->udp_fd, &rfds)))
            continue;

        /* How much do we have left to the end of the buffer */
        /* Whats the minimum we can read so that we dont comletely fill the buffer */
        left = av_fifo_space(s->fifo);
        left = FFMIN(left, s->fifo->end - s->fifo->wptr);

        /* No Space left, error, what do we do now */
        if( !left) {
            pthread_mutex_lock(&s->pthread_mutex);
            int size = av_fifo_size(s->fifo);
            if (size >0) {
                size = FFMIN( size>>1, 188*7*1024);
                av_fifo_generic_read(s->fifo, NULL, size, NULL); //droped head data.
		av_log(h, AV_LOG_INFO, "circular_buffer fulled droped data %d,datalen=%d,space=%d\n",size,av_fifo_size(s->fifo),av_fifo_space(s->fifo));
            }
            pthread_mutex_unlock(&s->pthread_mutex);
            continue;
        }

        len = recv(s->udp_fd, s->fifo->wptr, left, 0);
        if (len <= 0) {
            if (ff_neterrno() != AVERROR(EAGAIN) && ff_neterrno() != AVERROR(EINTR)) {
                s->circular_buffer_error = EIO;
                av_log(h, AV_LOG_INFO, "[%s:%d]Recv error %d\n",__FUNCTION__,__LINE__,ff_neterrno() );
            }
            amthreadpool_thread_usleep(10);
            continue;
        }
        
        s->fifo->wptr += len;
        if (s->fifo->wptr >= s->fifo->end)
            s->fifo->wptr = s->fifo->buffer;
        s->fifo->wndx += len;
    }

    av_log(h, AV_LOG_INFO, "[%s:%d]Task end!!!\n",__FUNCTION__,__LINE__);
    return NULL;
}

/* put it in UDP context */
/* return non zero if error */
static int udp_open(URLContext *h, const char *uri, int flags)
{
    char hostname[1024];
    int port, udp_fd = -1, tmp, bind_ret = -1;
    UDPContext *s = NULL;
    int is_output;
    const char *p;
    char buf[256];
    struct sockaddr_storage my_addr;
    int len;
    int reuse_specified = 0;

    h->is_streamed = 1;
    h->max_packet_size = 1472;

    is_output = !(flags & AVIO_FLAG_READ);

    s = av_mallocz(sizeof(UDPContext));
    if (!s)
        return AVERROR(ENOMEM);

    h->priv_data = s;
    s->ttl = 16;
    s->buffer_size = is_output ? UDP_TX_BUF_SIZE : UDP_MAX_PKT_SIZE;

    s->circular_buffer_size = 7*188*4096;

    p = strchr(uri, '?');
    if (p) {
        if (av_find_info_tag(buf, sizeof(buf), "reuse", p)) {
            char *endptr=NULL;
            s->reuse_socket = strtol(buf, &endptr, 10);
            /* assume if no digits were found it is a request to enable it */
            if (buf == endptr)
                s->reuse_socket = 1;
            reuse_specified = 1;
        }
        if (av_find_info_tag(buf, sizeof(buf), "ttl", p)) {
            s->ttl = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "localport", p)) {
            s->local_port = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "pkt_size", p)) {
            h->max_packet_size = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "buffer_size", p)) {
            s->buffer_size = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "connect", p)) {
            s->is_connected = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "buf_size", p)) {
            s->circular_buffer_size = strtol(buf, NULL, 10)*188;
        }
    }

    /* fill the dest addr */
    av_url_split(NULL, 0, NULL, 0, hostname, sizeof(hostname), &port, NULL, 0, uri);

    /* XXX: fix av_url_split */
    if (hostname[0] == '\0' || hostname[0] == '?') {
        /* only accepts null hostname if input */
        if (!(flags & AVIO_FLAG_READ))
            goto fail;
    } else {
        if (ff_udp_set_remote_url(h, uri) < 0)
            goto fail;
    }

    if ((s->is_multicast || !s->local_port) && (h->flags & AVIO_FLAG_READ))
        s->local_port = port;
    udp_fd = udp_socket_create(s, &my_addr, &len);
    if (udp_fd < 0)
        goto fail;

    /* Follow the requested reuse option, unless it's multicast in which
     * case enable reuse unless explicitely disabled.
     */
    if (s->reuse_socket || (s->is_multicast && !reuse_specified)) {
        s->reuse_socket = 1;
        if (setsockopt (udp_fd, SOL_SOCKET, SO_REUSEADDR, &(s->reuse_socket), sizeof(s->reuse_socket)) != 0)
            goto fail;
    }

    /* the bind is needed to give a port to the socket now */
    /* if multicast, try the multicast address bind first */
    if (s->is_multicast && (h->flags & AVIO_FLAG_READ)) {
        bind_ret = bind(udp_fd,(struct sockaddr *)&s->dest_addr, len);
    }
    /* bind to the local address if not multicast or if the multicast
     * bind failed */
    if (bind_ret < 0 && bind(udp_fd,(struct sockaddr *)&my_addr, len) < 0)
        goto fail;

    len = sizeof(my_addr);
    getsockname(udp_fd, (struct sockaddr *)&my_addr, &len);
    s->local_port = udp_port(&my_addr, len);

    if (s->is_multicast) {
        if (!(h->flags & AVIO_FLAG_READ)) {
            /* output */
            if (udp_set_multicast_ttl(udp_fd, s->ttl, (struct sockaddr *)&s->dest_addr) < 0)
                goto fail;
        } else {
            /* input */
            if (udp_join_multicast_group(udp_fd, (struct sockaddr *)&s->dest_addr) < 0)
                goto fail;
        }
    }

    if (is_output) {
        /* limit the tx buf size to limit latency */
        tmp = s->buffer_size;
        if (setsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, &tmp, sizeof(tmp)) < 0) {
            av_log(h, AV_LOG_ERROR, "setsockopt(SO_SNDBUF): %s\n", strerror(errno));
            goto fail;
        }
    } else {
        /* set udp recv buffer size to the largest possible udp packet size to
         * avoid losing data on OSes that set this too low by default. */
        tmp = s->buffer_size;
        if (setsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, &tmp, sizeof(tmp)) < 0) {
            av_log(h, AV_LOG_WARNING, "setsockopt(SO_RECVBUF): %s\n", strerror(errno));
        }
        /* make the socket non-blocking */
        ff_socket_nonblock(udp_fd, 1);
    }
    if (s->is_connected) {
        if (connect(udp_fd, (struct sockaddr *) &s->dest_addr, s->dest_addr_len)) {
            av_log(h, AV_LOG_ERROR, "connect: %s\n", strerror(errno));
            goto fail;
        }
    }

    s->udp_fd = udp_fd;
#if HAVE_PTHREADS	
    s->circular_buffer_thread = 0;
    if (!is_output && s->circular_buffer_size) {
        /* start the task going */
        s->fifo = av_fifo_alloc(s->circular_buffer_size);
        pthread_mutex_init(&s->pthread_mutex, NULL);
	s->request_exit = 0;
        av_log(h, AV_LOG_INFO, "[%s:%d]start the udp circular receive\n",__FUNCTION__,__LINE__);
        if (amthreadpool_pthread_create_name(&s->circular_buffer_thread, NULL, circular_buffer_task, h,"ffmpeg_udp")) {
            av_log(h, AV_LOG_ERROR, "pthread_create failed\n");
            goto fail;
        }
    }
#endif
    h->is_slowmedia = 1;
    return 0;
 fail:
    if (udp_fd >= 0)
        closesocket(udp_fd);
    av_fifo_free(s->fifo);
    av_free(s);
    return AVERROR(EIO);
}

static int udp_read(URLContext *h, uint8_t *buf, int size)
{
    UDPContext *s = h->priv_data;
    int ret;
    int avail;
    fd_set rfds;
    struct timeval tv;
    
    if (s->fifo) {

        do {
            pthread_mutex_lock(&s->pthread_mutex);
            avail = av_fifo_size(s->fifo);
            if (avail) { // >=size) {
                // Maximum amount available
                size = FFMIN( avail, size);
                av_fifo_generic_read(s->fifo, buf, size, NULL);
                pthread_mutex_unlock(&s->pthread_mutex);        
                return size;
            }
            else {
                FD_ZERO(&rfds);
                FD_SET(s->udp_fd, &rfds);
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                ret = select(s->udp_fd + 1, &rfds, NULL, NULL, &tv);
                if (ret<0){
                    pthread_mutex_unlock(&s->pthread_mutex);
                    av_log(h, AV_LOG_INFO, "[%s:%d]select failed ret=%d\n",__FUNCTION__,ret);
                    return ret;
                 }
            }
            pthread_mutex_unlock(&s->pthread_mutex);
            if (url_interrupt_cb()) {
                return AVERROR_EXIT;
            }
        } while( 1);
    }

    if (!(h->flags & AVIO_FLAG_NONBLOCK)) {
        ret = ff_network_wait_fd(s->udp_fd, 0);
        if (ret < 0)
            return ret;
    }
    ret = recv(s->udp_fd, buf, size, 0);
    return ret < 0 ? ff_neterrno() : ret;
}

static int udp_write(URLContext *h, const uint8_t *buf, int size)
{
    UDPContext *s = h->priv_data;
    int ret;

    if (!(h->flags & AVIO_FLAG_NONBLOCK)) {
        ret = ff_network_wait_fd(s->udp_fd, 1);
        if (ret < 0)
            return ret;
    }

    if (!s->is_connected) {
        ret = sendto (s->udp_fd, buf, size, 0,
                      (struct sockaddr *) &s->dest_addr,
                      s->dest_addr_len);
    } else
        ret = send(s->udp_fd, buf, size, 0);

    return ret < 0 ? ff_neterrno() : ret;
}

static int udp_close(URLContext *h)
{
    UDPContext *s = h->priv_data;
    if(s->circular_buffer_thread != 0){
	s->request_exit = 1 ;
        amthreadpool_pthread_join(s->circular_buffer_thread,NULL);
}
    if (s->is_multicast && (h->flags & AVIO_FLAG_READ))
        udp_leave_multicast_group(s->udp_fd, (struct sockaddr *)&s->dest_addr);
    closesocket(s->udp_fd);
    av_fifo_free(s->fifo);
    pthread_mutex_destroy(&s->pthread_mutex);
    av_free(s);
    return 0;
}

URLProtocol ff_udp_protocol = {
    .name                = "udp",
    .url_open            = udp_open,
    .url_read            = udp_read,
    .url_write           = udp_write,
    .url_close           = udp_close,
    .url_get_file_handle = udp_get_file_handle,
};
