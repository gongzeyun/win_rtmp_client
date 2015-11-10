/*
 * libcurl registered as ffmpeg protocol
 * Copyright (c) amlogic,2013, senbai.tao<senbai.tao@amlogic.com>
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

#include "amconfigutils.h"
#include "libavformat/avformat.h"
#include "libavformat/internal.h"
#include "libavformat/url.h"
#include "libavutil/avstring.h"
#include "libavutil/opt.h"
#include "curl_fetch.h"
#include "curl_log.h"

typedef struct _CURLFFContext {
    const AVClass *class;
    char uri[MAX_CURL_URI_SIZE];
    int read_retry;
    int64_t read_waittime_s;
    CFContext * cfc_h;
} CURLFFContext;

static const AVOption options[] = {{NULL}};
static const AVClass curlffmpeg_class = {
    .class_name     = "Amlcurlffmpeg",
    .item_name      = av_default_item_name,
    .option             = options,
    .version            = LIBAVUTIL_VERSION_INT,
};

static int force_interrupt = 0;
static int curl_interrupt_call_cb(void)
{
    if (url_interrupt_cb() || force_interrupt == 1) {
        return 1;
    }
    return 0;
}

// add exception case in this function when need retry.
static int curl_ffmpeg_need_retry(int arg)
{
    int ret = -1;
    switch (arg) {
    case CURLERROR(56 + C_ERROR_PERFORM_BASE_ERROR):  // recv failure
    case CURLERROR(18 + C_ERROR_PERFORM_BASE_ERROR):  // partial file
    case CURLERROR(28 + C_ERROR_PERFORM_BASE_ERROR):  // operation timeout
    case CURLERROR(C_ERROR_PERFORM_SELECT_ERROR):
        ret = 0;
        break;
    case CURLERROR(7 + C_ERROR_PERFORM_BASE_ERROR): // couldn't connect
    case CURLERROR(6 + C_ERROR_PERFORM_BASE_ERROR): // couldn't resolve host
        ret = 1;
        break;
    default:
        break;
    }
    return ret;
}

static void curl_ffmpeg_register_interrupt(CURLFFContext *h, interruptcallback pfunc)
{
    if (!h || !h->cfc_h) {
        return;
    }
    curl_fetch_register_interrupt(h->cfc_h, pfunc);
    return;
}

static int curl_ffmpeg_open(URLContext *h, const char *uri, int flags)
{
    CLOGI("curl_ffmpeg_open enter, flags=%d\n", flags);
    int ret = -1;
    CURLFFContext * handle = NULL;
    if (!uri || strlen(uri) < 1 || strlen(uri) > MAX_CURL_URI_SIZE) {
        CLOGE("Invalid curl-ffmpeg uri\n");
        return ret;
    }
    int retries = 0;
RETRY:
    handle = (CURLFFContext *)av_mallocz(sizeof(CURLFFContext));
    if (!handle) {
        CLOGE("Failed to allocate memory for CURLFFContext handle\n");
        return ret;
    }
    memset(handle->uri, 0, sizeof(handle->uri));
    if (av_stristart(uri, "curl:", NULL)) {
        av_strlcpy(handle->uri, uri + 5, sizeof(handle->uri));
    } else {
        av_strlcpy(handle->uri, uri, sizeof(handle->uri));
    }
    handle->cfc_h = curl_fetch_init(handle->uri, h->headers, flags);
    if (!handle->cfc_h) {
        CLOGE("curl_fetch_init failed\n");
        return ret;
    }

    curl_ffmpeg_register_interrupt(handle, curl_interrupt_call_cb);
    force_interrupt = 0;
    ret = curl_fetch_open(handle->cfc_h);
    if (ret) {
        curl_fetch_close(handle->cfc_h);
        av_free(handle);
        handle = NULL;
        if (++retries < 3) {
            goto RETRY;
        }
        return ret;
    }
    handle->read_retry = (int)am_getconfig_float_def("libplayer.curl.readretry", 10);
    handle->read_waittime_s = (int64_t)am_getconfig_float_def("libplayer.curl.readwaitS", 3600);
    h->http_code = handle->cfc_h->http_code;
    h->is_slowmedia = 1;
    h->is_streamed = handle->cfc_h->seekable ? 0 : 1;
    h->location = handle->cfc_h->relocation;
    h->priv_data = handle;
    return ret;
}

static int curl_ffmpeg_read(URLContext *h, uint8_t *buf, int size)
{
    int ret = -1;
    CURLFFContext * s = (CURLFFContext *)h->priv_data;
    if (!s) {
        CLOGE("CURLFFContext invalid\n");
        return ret;
    }
    int counts = 200, retries = 0;
    int wait_flag = 0;
    int64_t start_watitime_s = 0;
#if 1
    do {
        if (url_interrupt_cb()) {
            return AVERROR(EINTR);
        }
        ret = curl_fetch_read(s->cfc_h, buf, size);

        if (ret == C_ERROR_EAGAIN) {
            usleep(10 * 1000);
        }
        if (ret >= 0 || ret == C_ERROR_UNKNOW) {
            break;
        }
        /* just temporary, need to modify later */
        if (ret < C_ERROR_EAGAIN) {
            if (!curl_ffmpeg_need_retry(ret)) {
                CLOGI("curl_ffmpeg_read need retry! retries=%d, ret=%d\n", retries, ret);
                if (retries++ < s->read_retry) {
                    curl_fetch_seek(s->cfc_h, s->cfc_h->cwd->size, SEEK_SET);
                    counts = 200;
                }
            } else if (curl_ffmpeg_need_retry(ret) == 1) {
                if (!wait_flag) {
                    start_watitime_s = av_gettime() / 1000000;
                    wait_flag = 1;
                }
                CLOGI("curl_ffmpeg_read need wait to reconnect!\n");
                if (av_gettime() / 1000000 - start_watitime_s <= s->read_waittime_s) {
                    curl_fetch_seek(s->cfc_h, s->cfc_h->cwd->size, SEEK_SET);
                    usleep(100 * 1000);
                    counts = 200;
                }
            } else if (ret == CURLERROR(33 + C_ERROR_PERFORM_BASE_ERROR)) {
                /*live stream with file size > 0, return -33 CURLE_RANGE_ERROR*/
                curl_fetch_seek(s->cfc_h, 0, SEEK_SET); // reset
                ret = AVERROR(ENOSR);
                break;
            } else {
                ret = AVERROR(ENETRESET);
                break;
            }
        }
    } while (counts-- > 0);
#else
    ret = curl_fetch_read(s->cfc_h, buf, size);
#endif


#if 0
    FILE * fp = fopen("/temp/curl_dump.dat", "ab+");
    if (fp) {
        fwrite(buf, 1, ret, fp);
        fflush(fp);
        fclose(fp);
    }
#endif

    return ret;
}

static int64_t curl_ffmpeg_seek(URLContext *h, int64_t off, int whence)
{
    CLOGI("curl_ffmpeg_seek enter\n");
    int64_t ret = -1;
    CURLFFContext * s = (CURLFFContext *)h->priv_data;
    if (!s) {
        CLOGE("CURLFFContext invalid\n");
        return ret;
    }
    if (!s->cfc_h) {
        CLOGE("CURLFFContext invalid CFContext handle\n");
        return ret;
    }
    if ((whence == SEEK_CUR && !off) ||
        (whence == SEEK_END && off < 0)) {
        force_interrupt = 0;
    } else {
        force_interrupt = 1;
    }
    if (whence == AVSEEK_CURL_HTTP_KEEPALIVE) {
        ret = curl_fetch_http_keepalive_open(s->cfc_h, NULL);
    } else if (whence == AVSEEK_SIZE) {
        ret = s->cfc_h->filesize;
    } else {
        ret = curl_fetch_seek(s->cfc_h, off, whence);
    }
    force_interrupt = 0;
    return ret;
}

static int curl_ffmpeg_close(URLContext *h)
{
    CLOGI("curl_ffmpeg_close enter\n");
    CURLFFContext * s = (CURLFFContext *)h->priv_data;
    if (!s) {
        return -1;
    }
    force_interrupt = 1;
    curl_fetch_close(s->cfc_h);
    av_free(s);
    s = NULL;
    return 0;
}

static int curl_ffmpeg_get_info(URLContext *h, uint32_t  cmd, uint32_t flag, int64_t *info)
{
    CURLFFContext * s = (CURLFFContext *)h->priv_data;
    if (!s) {
        return -1;
    }
    int ret = 0;
    if (cmd == AVCMD_GET_NETSTREAMINFO) {
        if (flag == 1) {
            int64_t tmp_info = 0;
            ret = curl_fetch_get_info(s->cfc_h, C_INFO_SPEED_DOWNLOAD, flag, (void *)&tmp_info);
            if (!ret) {
                *info = tmp_info;
            } else {
                *info = 0;
            }
        }
    }
    return 0;
}

URLProtocol ff_curl_protocol = {
    .name               = "curl",
    .url_open           = curl_ffmpeg_open,
    .url_read           = curl_ffmpeg_read,
    .url_write          = NULL,
    .url_seek           = curl_ffmpeg_seek,
    //.url_exseek           = curl_ffmpeg_seek,
    .url_close          = curl_ffmpeg_close,
    .url_getinfo            = curl_ffmpeg_get_info,
    .url_get_file_handle    = NULL,
    .url_check          = NULL,
    .priv_data_size       = 0,
    .priv_data_class      = NULL,
};
