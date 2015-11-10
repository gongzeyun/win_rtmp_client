/*
 * Decryption protocol handler
 * Copyright (c) 2011 Martin Storsjo
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavformat/avformat.h"
#include "libavutil/avassert.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/avstring.h"
#include "libavutil/opt.h"
#include "libavformat/internal.h"
#include "list.h"
#include "libavformat/url.h"
#include "libavformat/avio.h"
#include <dlfcn.h>

//#define TEST_PASS_THROUGH 1

#define PR_READ_BLOCK   8192
//#define PR_BUF_SIZE     (8192*8)
#define PR_BUF_SIZE     (256*1024)
#define PR_MAX_STREAMS  4
#define PR_IV_MAXSIZE   16
#define PR_KEYID_SIZE   8
#define TS_SYNC_BYTE        0x47
#define TS_PACKET_SIZE      188
#define PR_AES_BLOCKSIZE   16
#define AES_BLOCK_SIZE 16
#define M3U_LINE_SIZE_MAX   (16*1024)
#define EXT_X_PLAYREADYHEADER       "#EXT-X-PLAYREADYHEADER"  //PlayReady DRM  tag

/*Defined in player_priv.h as well*/
#define PR_BURST_READ_SIZE   24064

/*Defined in PR*/
#define PR_CA_PID           0x1ffe
#define ECM_STREAM_ID       0xf0
#define ECM_MAX_SUBSAMPLE   500
#define BACK_FILE_PATH "/tmp/"

static int dbg_flag = 0;

#define PR_PRINT(args...)   av_log(NULL, AV_LOG_ERROR, ##args);

#define PR_DBG(args...) do { \
    if (dbg_flag) \
        av_log(NULL, AV_LOG_ERROR, ##args);\
}while(0)

typedef enum {
    DRM_LEVEL1     = 1,
    DRM_LEVEL2     = 2,
    DRM_LEVEL3     = 3,
    DRM_NONE       = 4,
} drm_level_t;

typedef struct drm_info {
    drm_level_t drm_level;
    int drm_flag;
    int drm_hasesdata;
    int drm_priv;
    unsigned int drm_pktsize;
    unsigned int drm_pktpts;
    unsigned long drm_phy;
    unsigned long drm_vir;
    unsigned int drm_remap;
    int data_offset;
    int extpad[8];
} drminfo_t;

/*Current TS Packet*/
typedef struct {
    uint16_t pid; /*according to ECM pid*/
    uint8_t scrambling_control; /*Encrypted or not*/
    uint32_t clr_len; /*Pass length*/
    uint32_t clr_residue; /*PES cut to the next TS, */
    uint32_t enc_len; /*Decrypt length*/
    uint32_t enc_residue; /*PES cut to the next TS, */
    uint64_t pts; /*pts copy from ECM sample info*/
    uint8_t iv[PR_IV_MAXSIZE]; /*iv copy from ECM sample info*/
    uint8_t iv_size;
    uint8_t mod_buf[PR_AES_BLOCKSIZE]; /*restore last encrypt ts 16 bytes align remain*/
    uint8_t mod_buf_size; /*residue size*/
} PRTSContext;

typedef struct {
    uint16_t pes_length; /*ECM pes length*/
    uint16_t pes_received; /*add ECM packet payload*/
    uint16_t packet_num;
    uint8_t* buf; /*ECM pes buffer*/
} ECMPacketInfo;

/*Key info for each encrypted PES*/
typedef struct {
    /*IV for each PES, HLS-PR iv size 8B*/
    uint8_t iv[PR_IV_MAXSIZE];
    uint8_t iv_size;
    /*Compare pts with PES pts for iv choosing*/
    uint64_t pts;
    /*How many bytes from PES payload to last clear data in this PES packet*/
    uint16_t clr_bytes;
    /*How many bytes from the beginning of encrypted data to the end of PES packet*/
    uint32_t enc_bytes;
    /*Current PES packet sequence number according to ECM*/
    uint32_t sample_num;
} ECMSubSample;

typedef struct {
    /*Parse all sample info from ECM*/
    ECMSubSample sample[ECM_MAX_SUBSAMPLE];
    uint16_t pid;
    /*1 ECM packet to N pes packets*/
    uint32_t pes_count;
    /*Current sample index */
    uint16_t sidx;
    /*No use for current HLS stream, as below*/
    uint8_t key_id[PR_KEYID_SIZE];
    uint8_t scrambling_stream_count;
    uint32_t pssh_len;
    uint16_t ecm_pes_length;
} ECMContext;

typedef struct {
    const AVClass *class;
    URLContext *hd;
    uint8_t keydat[AES_BLOCK_SIZE];
    uint8_t *key;
    int keylen;
    uint8_t *iv;
    int ivlen;
    /*Read packet into inbuffer*/
    uint8_t inbuffer [PR_BUF_SIZE + PR_BUF_SIZE];
    /*Decrypt into outbuffer*/
    uint8_t outbuffer[PR_BUF_SIZE + PR_BUF_SIZE];
    /*Read buffer from http to inbuffer, update in_getptr*/
    uint8_t *in_getptr;
    /*Comsume inbuffer for decrypt, update in_putptr*/
    uint8_t *in_putptr;
    /*Copy decrypted data to outbuffer, update out_getptr*/
    uint8_t *out_getptr;
    /*Copy data to vhls, update out_putptr*/
    uint8_t *out_putptr;
    /*TS info, for enc offset & len calculation*/
    PRTSContext ts;
    /*Current ECM to use, struct of a ECM packet, contains several subsample info*/
    ECMContext ecm[PR_MAX_STREAMS];
    /*For ECM packet conbination*/
    ECMPacketInfo ecm_packet;
    FILE* mBackupFile;
    uint32_t clr_border;
    uint32_t enc_border;
    uint32_t istvp;
} PRCryptoContext;

#define OFFSET(x) offsetof(PRCryptoContext, x)
static const AVOption options[] = {
    {"key", "AES decryption key", OFFSET(key), FF_OPT_TYPE_BINARY },
    {"iv",  "AES decryption initialization vector", OFFSET(iv),  FF_OPT_TYPE_BINARY },
    { NULL }
};

static const AVClass crypto_pr_class = {
    .class_name     = "cryptopr",
    .item_name      = av_default_item_name,
    .option         = options,
    .version        = LIBAVUTIL_VERSION_INT,
};

typedef int PRgetkeyFunc(char* keyurl, uint8_t* keydat);
typedef int PRDecryptFunc(int secure, const uint8_t key[16], const uint8_t iv[16], const void* srcPtr, int clearsize, int encsize, void* dstPtr);
typedef int PRCloseFunc(uint8_t* keydat);

static PRgetkeyFunc* PRgetkey = NULL;
static PRDecryptFunc* PRdecrypt = NULL;
static PRCloseFunc* PRclose = NULL;

static PRgetkeyFunc* pr_get_key()
{
    void * mLibHandle = dlopen("libprdrmpluginwrapper.so", RTLD_NOW);

    if (mLibHandle == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Unable to locate libprdrmpluginwrapper.so\n");
        return NULL;
    }
    av_log(NULL, AV_LOG_ERROR, "pr_get_key\n");

    return (PRgetkeyFunc*)dlsym(mLibHandle, "_ZN7android8prgetkeyEPcPh");
}

static PRDecryptFunc* pr_decrypt()
{
    void * mLibHandle = dlopen("libprdrmpluginwrapper.so", RTLD_NOW);

    if (mLibHandle == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Unable to locate libprdrmpluginwrapper.so\n");
        return NULL;
    }
    av_log(NULL, AV_LOG_ERROR, "pr_decrypt\n");

    return (PRDecryptFunc*)dlsym(mLibHandle, "_ZN7android9prDecryptEbPKhS1_PKviiPv");
}

static PRCloseFunc* pr_close()
{
    void * mLibHandle = dlopen("libprdrmpluginwrapper.so", RTLD_NOW);

    if (mLibHandle == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Unable to locate libprdrmpluginwrapper.so\n");
        return NULL;
    }
    av_log(NULL, AV_LOG_ERROR, "pr_close\n");

    return (PRCloseFunc*)dlsym(mLibHandle, "_ZN7android15closeallsessionEPc");
}

#define GETU32(pt) (((uint32_t)(pt)[0] << 24) | \
                    ((uint32_t)(pt)[1] << 16) | \
                    ((uint32_t)(pt)[2] <<  8) | \
                    ((uint32_t)(pt)[3]))

#define PUTU32(ct, st) do{ \
    (ct)[0] = (uint8_t)((st) >> 24); \
    (ct)[1] = (uint8_t)((st) >> 16); \
    (ct)[2] = (uint8_t)((st) >>  8); \
    (ct)[3] = (uint8_t)(st); }while(0)

/* increment counter (128-bit int) by 1, only modify block offset field(bottom 4 bytes) */
static void iv_blk_offset_inc(uint8_t *iv, uint32_t count)
{
    unsigned long c;

    /* Grab bottom dword of counter and increment */
    c = GETU32(iv + 12);
    c += count;
    c &= 0xFFFFFFFF;
    PUTU32(iv + 12, c);
}

/* increment counter (128-bit int) by 1, only modify block offset field(bottom 4 bytes) */
static void iv_byte_offset_set(uint8_t *iv, uint32_t count)
{
    unsigned long c;

    /* Grab bottom dword of counter and increment */
    c = GETU32(iv + 8);
    c = count;
    PUTU32(iv + 8, c);
}

static int drm_stronedrminfo(char *outpktdata, char *addr, int size)
{
    drminfo_t drminfo = {0};
#define TYPE_DRMINFO   0x80

    drminfo.drm_level = DRM_LEVEL1;
    drminfo.drm_pktsize = size;
    drminfo.drm_phy = (unsigned long)addr;
    drminfo.drm_flag = TYPE_DRMINFO;
    memcpy(outpktdata, &drminfo, sizeof(drminfo_t));
    return 0;
}

/*Reset ptr in open and seek*/
static void reset_inout_ptr(PRCryptoContext* c)
{
    c->in_getptr = c->in_putptr = c->inbuffer;
    c->out_getptr = c->out_putptr = c->outbuffer;
    c->clr_border = 0;
    c->enc_border = 0;
    PR_DBG("[reset_inout_ptr]\n");
}

/**
* @brief  prts_resync  Make sure the first byte of buffer is TS SYNC header
*
* @param[in]   buffer read from ffurl_read, size > 4KB
*
* @return      bytes passed after we find TS SYNC BYTE
*/
static int prts_resync(uint8_t *buf, uint8_t *end)
{
    uint8_t *ppbuf = buf;
    int scannum = TS_PACKET_SIZE;
    while (scannum-- > 0 && ppbuf[0] != TS_SYNC_BYTE) {
        ppbuf++;
        //PR_DBG("[prts_resync] ppbuf %x, i %d\n", ppbuf, i);
    }
    if (((ppbuf + TS_PACKET_SIZE * 3) <= end) && ppbuf[0] == TS_SYNC_BYTE &&
        ppbuf[TS_PACKET_SIZE] == TS_SYNC_BYTE &&
        ppbuf[TS_PACKET_SIZE * 2] == TS_SYNC_BYTE) {
        return (ppbuf - buf);
    }
    if (((ppbuf + TS_PACKET_SIZE * 3) > end) && ppbuf[0] == TS_SYNC_BYTE) {
        PR_PRINT("last 2 ts packet?\n");
        return (ppbuf - buf);
    }
    PR_PRINT("PRTS_RESYNC FAILED\n");
    return -1;
}

/**
* Compare pid from ecm sample info and pid from this TS packet
*/
static inline int pid_to_stream_index(PRCryptoContext* c, uint16_t pid)
{
    int i;
    for (i = 0; i < PR_MAX_STREAMS; i++) {
        /**
              * pid 0, not initialized, for ECM installation at the first time
              * compare pid, for ecm struct rewrite when we received other
              *         new ECM packets.
              */
        if (c->ecm[i].pid == 0 || c->ecm[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

/**
* @brief  parse_ecm_packet  parse ecm pes packet after we received all ECM ts packet
*
* @param[in]   c     global struct
* @param[in]   buf  ECM pes packet buffer
*/
static void parse_ecm_packet(PRCryptoContext *c, uint8_t *buf)
{
    uint32_t i, j, k, l, pssh_count, str_para_inc, sample_count,
             sub_sample_count, iv_size, str_idx;
    ECMContext* e = av_mallocz(sizeof(ECMContext));
    uint8_t* p = buf;

    e->ecm_pes_length = AV_RB16(buf + 4);
    e->sidx = 0;
    pssh_count = buf[6] & 0xf;

    p += (7 + pssh_count);
    e->scrambling_stream_count = p[0];
    //av_assert0(e->scrambling_stream_count < PR_MAX_STREAMS);
    p += 1;
    do {
        str_para_inc = p[0] & 0x20;
        e->pid = ((p[0] & 0x1f) << 8) | p[1];
        //PR_DBG("e->pid %x\n", e->pid);
        p += 2;
        if (str_para_inc) {
            iv_size = p[3];
            //PR_DBG("iv_size %x\n", iv_size);
            p += 20;
        }
        //av_assert0(iv_size <= 16);
        e->pes_count = p[0];
        p += 1;
        for (j = 0; j < e->pes_count; j++) {
            e->sample[j].pts = AV_RB32(p) << 1 | (p[4]) >> 7;
            //PR_DBG("e->pes_count %d, j %d, pts %lld\n", e->pes_count, j, e->sample[j].pts);
            sample_count = p[4] & 0x7f;
            p += 5;
            for (k = 0; k < sample_count; k++) {
                //if(k>0){
                //    j++;
                //    e->sample[j].pts = e->sample[j-1].pts;//copy
                //}
                //PR_DBG("sample_count %d, j %d\n", sample_count, j);
                e->sample[j].iv_size = iv_size;
                memcpy(e->sample[j].iv, p, iv_size);
                sub_sample_count = AV_RB16(p + iv_size);
                p += (2 + iv_size);
                for (l = 0; l < sub_sample_count; l++) {
                    //if(l>0){
                    //    j++;
                    //    e->sample[j].pts = e->sample[j-1].pts;//copy
                    //    e->sample[j].iv_size = iv_size;//copy
                    //    memcpy(e->sample[j].iv, e->sample[j-1].iv, iv_size);//copy
                    //}
                    //PR_DBG("sub_sample_count %d, j %d, p %x\n",
                    //        sub_sample_count, j, p);
                    e->sample[j].clr_bytes = AV_RB16(p);
                    e->sample[j].enc_bytes = AV_RB32(p + 2);
                    p += 6;
                }
            }
        }
        str_idx = pid_to_stream_index(c, e->pid);
        memcpy(&c->ecm[str_idx], e, sizeof(ECMContext));
    } while (--e->scrambling_stream_count);

    av_free(e);
    return;
}

/**
* @brief  handle_ecm_packet  handle ECM TS packet, malloc buffer to restore
*            ECM payload, update pes_received, if we get all, parse ecm packet
*
* @param[in]   c            global struct
* @param[out]  buf        buffer from pes payload
* @param[in]   pes_off   pes payload offset in this ts packet
*/
static void handle_ecm_packet(PRCryptoContext *c, uint32_t is_start,
                              uint8_t  *buf, uint32_t pes_off)
{
    ECMPacketInfo* e = &c->ecm_packet;
    uint16_t ecm_payload = TS_PACKET_SIZE - pes_off;
    /*Start code of ECM packet*/
    if (is_start) {
        e->pes_length = AV_RB16(buf + 4);
        e->packet_num = 0;
        e->pes_received = 0;
        /*free in prcrypto_close*/
        e->buf = av_realloc(e->buf, e->pes_length + TS_PACKET_SIZE);
        if (!e->buf) {
            PR_PRINT("[handle_ecm_packet] No Memory.\n");
            return;
        }
        memset(e->buf, 0, e->pes_length + TS_PACKET_SIZE);
    }

    if (!e->buf) {
        PR_PRINT("[handle_ecm_packet] e->buf NULL\n");
        return;
    }
    memcpy(e->buf + e->pes_received, buf, ecm_payload);
    e->packet_num++;
    e->pes_received += ecm_payload;

    /*All we get, parse it*/
    if (e->pes_received >= e->pes_length) {
        parse_ecm_packet(c, e->buf);
    }
}

/**
 * Parse MPEG-PES five-byte timestamp
 */
static inline int64_t ff_parse_pes_pts(const uint8_t *buf)
{
    return (int64_t)(*buf & 0x0e) << 29 |
           (AV_RB16(buf + 1) >> 1) << 15 |
           AV_RB16(buf + 3) >> 1;
}

/**
* @brief  update_from_sampleinfo  install ts info from ECM sample info,
*                   lookup ECM stream index with pid, compare pts in sample
*                   info with current pts from pes, make sure the iv
*                   description is right for this PES packet.
*
* @param[in]    c           global struct
* @param[in]    pid        pid of this TS packet
* @param[in]    pes_pts  pts from pes info
*/
static uint32_t update_from_sampleinfo(PRCryptoContext* c, uint16_t pid,
                                       uint64_t pes_pts)
{
    /*sample index++, then compare PES PTS with ECM PTS*/
    int str_idx, samp_idx, i;
    uint64_t ecm_pts;

    str_idx = pid_to_stream_index(c, pid);
    //PR_DBG("update_from_sampleinfo str_idx %d\n", str_idx);
    if (str_idx == -1) {
        memset(&c->ts, 0 , sizeof(PRTSContext));
        PR_PRINT("update_from_sampleinfo memset ts struct\n");
    }
    samp_idx = c->ecm[str_idx].sidx++;
    ecm_pts = c->ecm[str_idx].sample[samp_idx].pts;

    if (pes_pts != ecm_pts) {
        ECMContext* e = &c->ecm[str_idx];
        for (i = 0; i < e->pes_count; i++) {
            PR_DBG("ecm pts %lld, pes pts %lld\n", e->sample[i].pts, pes_pts);
            if (ecm_pts == e->sample[i].pts) {
                c->ecm[str_idx].sidx = samp_idx = i;
                break;
            }
        }
        if (i >= e->pes_count) {
            PR_DBG("pts not valid: %lld\n", pes_pts);
            return -1;
        }
    }
    //PR_DBG("ecm_pts %lld, samp_idx %d\n", ecm_pts, samp_idx);
    /*Update clear bytes and encrypted bytes to c->ts*/
    c->ts.clr_len = c->ecm[str_idx].sample[samp_idx].clr_bytes;
    c->ts.enc_len = c->ecm[str_idx].sample[samp_idx].enc_bytes;
    c->ts.clr_residue = c->ts.clr_len;
    c->ts.enc_residue = c->ts.enc_len;
    c->ts.pts = c->ecm[str_idx].sample[samp_idx].pts;
    c->ts.iv_size = c->ecm[str_idx].sample[samp_idx].iv_size;
    c->ts.mod_buf_size = 0;
    memset(c->ts.mod_buf, 0, PR_AES_BLOCKSIZE);
    memset(c->ts.iv, 0, PR_IV_MAXSIZE);
    memcpy(c->ts.iv, c->ecm[str_idx].sample[samp_idx].iv, c->ts.iv_size);

    //PR_DBG("c->ts.clr_len %d, c->ts.enc_len %d\n", c->ts.clr_len, c->ts.enc_len);
    return 0;
}

/**
* @brief  calculate_enc_offset  calculate encrypt data offset and length
*                       in this TS packet, but pes payload may slice to several
*                       ts packets, so the graph below is not correct, just
*                       for comprehansion
*     TS HEADER       PES HEADER     PES CLEAR PAYLOAD    PES ENCRYPTED PAYLOAD
*     |--------pes offset------ ||---clear data bytes---||----encrypt data bytes----|
*
* @param[in]     ts          TS info struct
* @param[in]     pes_off   PES payload offset in this ts packet
* @param[out]   offset     encrypt data offset in this ts packet
* @param[out]   len         encrypt data length int this ts packet
*/
static void calculate_enc_offset(PRTSContext* ts,
                                 uint32_t pes_off, uint32_t* offset, uint32_t* len)
{
    uint16_t clr_end = ts->clr_residue + pes_off;
    uint32_t enc_end = ts->enc_residue + pes_off;

    //PR_DBG("calculate_enc_offset pes_off %d, ts->clr_residue %d,"
    //        "ts->enc_residue %d\n", pes_off, ts->clr_residue, ts->enc_residue);

    if (ts->clr_residue > 0 && clr_end < TS_PACKET_SIZE) {
        /*clear data + encrypted data*/
        *offset = clr_end;
        *len = TS_PACKET_SIZE - clr_end;
        ts->clr_residue = 0;
        ts->enc_residue -= *len;
    } else if (ts->clr_residue > 0 && clr_end >= TS_PACKET_SIZE) {
        /*clear data*/
        *offset = 0;
        *len = 0;
        ts->clr_residue -= (TS_PACKET_SIZE - pes_off);
    } else if (ts->clr_residue == 0 && enc_end <= TS_PACKET_SIZE) {
        /*last encrypted data*/
        *offset = pes_off;
        *len = TS_PACKET_SIZE - pes_off;//ts->enc_residue;
        if (TS_PACKET_SIZE - pes_off != ts->enc_residue) {
            PR_DBG("pes_off %d, TS-pes_off %d, enc_residue %d\n",
                   pes_off, TS_PACKET_SIZE - pes_off, ts->enc_residue);
        }
        ts->enc_residue = 0;
    } else if (ts->clr_residue == 0 && enc_end > TS_PACKET_SIZE) {
        /*encrypted data*/
        *offset = pes_off;
        *len = TS_PACKET_SIZE - pes_off;
        ts->enc_residue -= *len;
    } else
        PR_DBG("calculate enc offset error."
               "clear residue %d, encrypt residue %d, pes offset %d",
               ts->clr_residue, ts->enc_residue, pes_off);
}


/**
* @brief  handle_ts_packet  Parse ts packet.
*                   ECM TS packet: restore it for future parsing and pass it to vhls for filter.
*                   no payload, no encrypt, no pes: pass it to vhls.
*                   start of encrypt pes: update info to ts struct.
*                   encrypt pes: calculate encrypt data offset and length.
*
* @param[in]    c         global struct
* @param[in]    buf      buffer read from ffurl_read, start from ts header
* @param[out]  offset  encrypt data offset in this ts packet
* @param[out]  len      encrypt data length int this ts packet
*/
static void handle_ts_packet(PRCryptoContext *c, uint8_t *buf,
                             uint32_t* offset, uint32_t* len)
{
    uint32_t pid, afc, is_start, has_adaptation, has_payload, stream_id, ret, cc;
    uint8_t *p, *p_end;
    uint64_t pes_pts;
    uint32_t pes_off;

    *offset = TS_PACKET_SIZE;
    *len = 0;

    /*Get pid from ts*/
    pid = AV_RB16(buf + 1) & 0x1fff;
    is_start = buf[1] & 0x40;

    /* skip adaptation field */
    afc = (buf[3] >> 4) & 3;
    has_adaptation = afc & 2;
    has_payload = afc & 1;
    cc = (buf[3] & 0xf);

    //PR_DBG("TS packet pid %d, is_start %d, buf %x, cc %d\n", pid, is_start, buf, cc);
    if (!has_payload) {
        goto pass_through;
    }

    p = buf + 4;
    if (has_adaptation) {
        /* skip adapation field */
        p += p[0] + 1;
    }
    /* if past the end of packet, ignore */
    p_end = buf + TS_PACKET_SIZE;
    if (p >= p_end) {
        goto pass_through;
    }

    pes_off = p - buf;

    /*pass ECM packet to vhls & mpegts, they filter it themselves*/
    if (pid == PR_CA_PID) {
        handle_ecm_packet(c, is_start, p, pes_off);
        goto pass_through;
    }

    /*process pes state*/
    if (is_start) {
        if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01) {
            stream_id = p[3];
            c->ts.scrambling_control = p[6] & 0x30;
            if (c->ts.scrambling_control == 0) {
                goto pass_through;
            }
            p[6] = p[6] & 0xcf; //reset scrambling_control bit
            /*Get PTS in PES*/
            if (p[7] & 0xc0) {
                pes_pts = ff_parse_pes_pts(p + 9);
                ret = update_from_sampleinfo(c, pid, pes_pts);
                if (ret) {
                    PR_DBG("error pes_pts %lld, pid %d \n", pes_pts, pid);
                }
            }
            pes_off += 9; //header size
            pes_off += p[8];
        } else { /* PAT/PMT, no pes packet with */
            goto pass_through;
        }
    }
    /*Get encrypted data offset & len in this TS packet*/
    calculate_enc_offset(&c->ts, pes_off, offset, len);
    return;

pass_through:
    //PR_DBG("pass_through pid %d\n", pid);
    return;
}

char sbuf[1024 * 4];

static void hex_dump_internal(char *buf, int size)
{
    int len, i, j, c;
    char sbuf[1024 * 4];
    int off = 0;
    int printed = 0;

#undef fprintf
#define PRINT(...) do {\
    printed= snprintf(sbuf+off,1024*4-off, __VA_ARGS__); \
    if (printed>0) off+=printed;\
} while(0)

    for (i = 0; i < size; i += 16) {
        len = size - i;
        if (len > 16) {
            len = 16;
        }
        PRINT("%08x: ", i);
        for (j = 0; j < 16; j++) {
            if (j < len) {
                PRINT(" %02x", buf[i + j]);
            } else {
                PRINT("   ");
            }
        }
        PRINT(" ");
        for (j = 0; j < len; j++) {
            c = buf[i + j];
            if (c < ' ' || c > '~') {
                c = '.';
            }
            PRINT("%c", c);
        }
        PRINT("\n");
    }
    if (off > 0 && off < 1024 * 4) {
        sbuf[off] = '\0';
        PR_DBG("%s\n", sbuf);
    }
#undef PRINT
}

/**
* @brief  decrypt_ts_packet  decrypt ts packet
*
* @param[in]    c         global struct
* @param[in]    ibuf     encrypt data in c->in_putptr
* @param[in]    off      encrypt data offset in this ts packet
* @param[in]    len      encrypt data length int this ts packet
* @param[out]  obuf    clear data in c->out_getptr
*/
static void decrypt_ts_packet(PRCryptoContext* c, uint32_t clr_len,
                              uint32_t enc_len, uint32_t rd_off, uint8_t *buf, int rd_size)
{
    uint8_t ivec[AES_BLOCK_SIZE];
    PRTSContext* ts = &c->ts;
    uint32_t phyAddr = c->out_getptr - c->outbuffer;
    uint8_t* ibuf = c->in_putptr;
    uint8_t* obuf = c->out_getptr;

    memcpy(ivec, ts->iv, PR_IV_MAXSIZE);

    if (PRdecrypt == NULL) {
        PRdecrypt =  pr_decrypt();
        if (PRdecrypt == NULL) {
            av_log(NULL, AV_LOG_ERROR, "dlsym failed with error: %s\n",  dlerror());
            return;
        }
    }

    if (c->istvp) {
        PRdecrypt(0, c->keydat, ivec, ibuf, clr_len, enc_len, &phyAddr);
        if (rd_size != PR_BURST_READ_SIZE) {
            memcpy(buf + rd_off, ibuf, clr_len);
            memset(buf + rd_off + clr_len, 0, enc_len);
        }
        if (rd_off == 0 && (rd_size == PR_BURST_READ_SIZE)) {
            drm_stronedrminfo(buf, phyAddr, rd_size);
        }
        usleep(10);
    } else if (clr_len + enc_len) {
        PR_DBG("NOTVP: ibuf %x, buf %x, rd_off %d\n", ibuf, buf, rd_off);
        PRdecrypt(0, c->keydat, ivec, ibuf, clr_len, enc_len, buf + rd_off);
    }

    if (enc_len) {
        iv_blk_offset_inc(ts->iv, (ts->mod_buf_size + enc_len) / AES_BLOCK_SIZE);
        ts->mod_buf_size = (ts->mod_buf_size + enc_len) % AES_BLOCK_SIZE;
        iv_byte_offset_set(ts->iv, ts->mod_buf_size);
    }
    return;
}

static int prcrypto_read(URLContext *h, uint8_t *buf, int size)
{
    PRCryptoContext* c = h->priv_data;
    uint32_t clr_len = 0, enc_len = 0;
    int  n, ts_off = 0, iseof = 0;
    uint32_t rdsize = size, dosize = 0;
    uint32_t clr_ts, enc_ts;

    PR_DBG("[prcrypto_read] buf %p, size %d\n", buf, size);
    if (rdsize > PR_BUF_SIZE) {
        /*BUG*/
        PR_PRINT("\n\n\n [prcrypto_read] rdsize is %d \n\n\n", rdsize);
    }
    /* Read 16KB, consume 8KB, leave 8KB residue, */
    while (c->in_getptr - c->in_putptr < rdsize+TS_PACKET_SIZE*3) {
        n = url_lpread(c->hd, c->in_getptr, PR_READ_BLOCK);
        //PR_PRINT("url_lpread,  c->clr_border %d, c->enc_border %d\n",
        //                c->clr_border, c->enc_border);
        //hex_dump_internal(c->in_getptr, 16);
        if (n <= 0) {
            int insize = c->in_getptr - c->in_putptr;
            /*last buffer*/
            av_log(h, AV_LOG_ERROR, "n %d, insize %d, size %d\n",
                   n, insize, size);
            if (n == 0 && insize && insize % TS_PACKET_SIZE == 0) {
                av_log(h, AV_LOG_ERROR, "EOF : insize %d, size %d\n", insize, size);
                iseof = 1;
                break;
            }
            return n;
        }
        c->in_getptr += n;
    }

    while (c->in_getptr - c->in_putptr >= TS_PACKET_SIZE) {
        /*handle the rest content in packet, had red half of it last time*/
        if (c->clr_border + c->enc_border) {
            if (c->clr_border >= rdsize) {
                decrypt_ts_packet(c, rdsize, 0, dosize, buf, rdsize);
                c->in_putptr += rdsize;
                c->out_getptr += rdsize;
                c->clr_border -= rdsize;
                dosize += rdsize;
            } else if (c->clr_border + c->enc_border >= rdsize) {
                decrypt_ts_packet(c, c->clr_border, rdsize - c->clr_border, dosize, buf, rdsize);
                c->in_putptr += rdsize;
                c->out_getptr += rdsize;
                c->enc_border -= (rdsize - c->clr_border);
                c->clr_border = 0;
                dosize += rdsize;
            } else {
                PR_DBG("\n\n\n c->clr_border %d, c->enc_border %d\n\n\n", c->clr_border, c->enc_border);
                decrypt_ts_packet(c, c->clr_border, c->enc_border, dosize, buf, rdsize);
                c->in_putptr += (c->clr_border + c->enc_border);
                c->out_getptr += (c->clr_border + c->enc_border);
                dosize += (c->clr_border + c->enc_border);
                c->clr_border = 0;
                c->enc_border = 0;
            }
        }
        ts_off = prts_resync(c->in_putptr, c->in_getptr);
        /*TS packet, typically ts_off is 0*/
        if (ts_off > 0) {
            PR_DBG("\n\n\n ts_off %d\n\n\n", ts_off);
            hex_dump_internal(c->in_putptr, 16);
            if (ts_off > rdsize) {
                decrypt_ts_packet(c, rdsize, 0, dosize, buf, rdsize);
                c->in_putptr += rdsize;
                c->out_getptr += rdsize;
                dosize += rdsize;
                PR_DBG("ts_off %d, read_size %d, break\n", ts_off, rdsize);
                break;
            }
            decrypt_ts_packet(c, ts_off, 0, dosize, buf, rdsize);
            c->in_putptr += ts_off;
            c->out_getptr += ts_off;
            dosize += ts_off;
        }
        if (ts_off >= 0) {
            handle_ts_packet(c, c->in_putptr, &clr_len, &enc_len);
        } else {
            clr_len = TS_PACKET_SIZE;
            enc_len = 0;
        }

        if ((uint32_t)(dosize + TS_PACKET_SIZE) >= rdsize) {
            if (rdsize - dosize > clr_len) {/*clr 1, enc 1*/
                clr_ts = clr_len;
                enc_ts = rdsize - dosize - clr_len;
                c->clr_border = 0;
                c->enc_border = enc_len - enc_ts;
            } else { /*clr 1, enc 0*/
                clr_ts = rdsize - dosize;
                enc_ts = 0;
                c->clr_border = clr_len - clr_ts;
                c->enc_border = enc_len;
            }
            decrypt_ts_packet(c, clr_ts, enc_ts, dosize, buf, rdsize);
            c->in_putptr += (clr_ts + enc_ts);
            c->out_getptr += (clr_ts + enc_ts);
            dosize += (clr_ts + enc_ts);
            break;
        }

        /* decrypt from enc offset to enc offset+len in outbuffer, update in_putptr, out_getptr */
        decrypt_ts_packet(c, clr_len, enc_len, dosize, buf, rdsize);

        if (clr_len + enc_len != TS_PACKET_SIZE) {
            PR_PRINT("BUG: \n\n\n clr_len %d, enc_len %d \n\n\n", clr_len, enc_len);
        }
        dosize += (clr_len + enc_len);
        c->in_putptr += (clr_len + enc_len);
        c->out_getptr += (clr_len + enc_len);
    }

    /*Send end of packet flag*/
    decrypt_ts_packet(c, 0, 0, dosize, buf, rdsize);

    if (c->in_getptr - c->in_putptr < TS_PACKET_SIZE) {
        PR_PRINT("bug: c->in_getptr - c->in_putptr %d\n", c->in_getptr - c->in_putptr);
    }

    c->out_putptr += rdsize;
    if (c->out_getptr - c->outbuffer >= (PR_BUF_SIZE - PR_BURST_READ_SIZE)) {
        PR_DBG("reset out ptr\n\n\n");
        c->out_getptr = c->out_putptr = c->outbuffer;
    }

    if (c->in_putptr - c->inbuffer >= (PR_BUF_SIZE - PR_BURST_READ_SIZE)) {
        PR_DBG("reset in buffer ptr, buffer %x, put %x, get %x\n\n\n",
               c->inbuffer, c->in_putptr, c->in_getptr);
        memmove(c->inbuffer, c->in_putptr, c->in_getptr - c->in_putptr);
        c->in_getptr -= (c->in_putptr - c->inbuffer);
        c->in_putptr = c->inbuffer;
    }

    /*dump NOTVP clear data*/
    if (c->mBackupFile != NULL && rdsize > 0) {
        fwrite(buf, 1, rdsize, c->mBackupFile);
        fflush(c->mBackupFile);
    }
    if (iseof && c->istvp) {
        PR_PRINT("[prcrypto_read] EOF return %d\n", size);
        return size;
    }
    return rdsize;
}

static int prcrypto_setkey(PRCryptoContext *c, const char* uri)
{
    URLContext* smallh = NULL;
    int32_t ssize, offset = 0, offsetLF = 0;
    uint8_t *sbuf = NULL;
    uint8_t *line = NULL;
    int ret = -1, reason_code = 0, n, read = 0;

    if ((ret = ffurl_open_h(&smallh, uri,
                            AVIO_FLAG_READ | AVIO_FLAG_NONBLOCK | URL_SEGMENT_MEDIA,
                            NULL, &reason_code)) < 0) {
        PR_PRINT("[prcrypto_open]Unable to open small file,reason:%d\n", reason_code);
        return ret;
    }
    ssize = ffurl_size(smallh);
    PR_DBG("[prcrypto_setkey] ffurl_size %d\n", ssize);
    if (ssize > 0) {
        sbuf = av_mallocz(ssize);
        line = av_malloc(M3U_LINE_SIZE_MAX);
        while (read < ssize) {
            n = ffurl_read(smallh, sbuf + read, ssize);
            if (n > 0) {
                read += n;
            }
        }
        if (sbuf > 0 && line > 0) {
            while (offset < ssize) {
                //fetch line data
                offsetLF = offset;
                while (offsetLF < ssize && sbuf[offsetLF] != '\n' && sbuf[offsetLF] != '\0') {
                    ++offsetLF;
                }
                if (offsetLF > ssize) {
                    break;
                }
                memset(line, 0, M3U_LINE_SIZE_MAX);
                if (offsetLF > offset && sbuf[offsetLF - 1] == '\r') {
                    memcpy(line, &sbuf[offset], offsetLF - offset - 1);
                    line[offsetLF - offset - 1] = '\0';
                } else {
                    memcpy(line, &sbuf[offset], offsetLF - offset);
                    line[offsetLF - offset] = '\0';
                }
                offset = offsetLF + 1;
                if (strlen(line) == 0) {
                    continue;
                }
                if (!strncmp(line, EXT_X_PLAYREADYHEADER, strlen(EXT_X_PLAYREADYHEADER))) {
                    PRgetkey = pr_get_key();
                    if (PRgetkey) {
                        /*add #EXT-X-PLAYREADYHEADER: */
                        ret = PRgetkey(line + strlen(EXT_X_PLAYREADYHEADER) + 1, &c->keydat);
                        av_hex_dump(NULL, c->keydat, AES_BLOCK_SIZE);
                        break;
                    }
                }
            }
        }
    }

    if (sbuf) {
        av_free(sbuf);
    }
    if (line) {
        av_free(line);
    }
    return ret;
}

static int prcrypto_open(URLContext *h, const char *uri, int flags)
{
    const char *nested_url;
    uint8_t furl[MAX_URL_SIZE];
    int ret;
    PRCryptoContext *c = h->priv_data;
    int reason_code = 0;

    /*remove cryptopr header, add vhls header*/
    if (!av_strstart(uri, "cryptopr+", &nested_url) &&
        !av_strstart(uri, "cryptopr:", &nested_url)) {
        PR_DBG("Unsupported url %s\n", uri);
        ret = AVERROR(EINVAL);
        goto err;
    }
    memset(furl, 0, MAX_URL_SIZE);
    snprintf(furl, MAX_URL_SIZE, "vhls:%s", nested_url);
    PR_DBG("prcrypto_open furl %s\n", furl);

    if (nested_url[0] == 's') {
        nested_url++;
    }
    /* Download small file, parse playready header, set header, get key id */
    if ((c->istvp = prcrypto_setkey(c, nested_url)) < 0) {
        PR_DBG("[prcrypto_open]Set key fail, ret:%d\n", ret);
        goto err;
    }

    /*Call vhls*/
    if ((ret = ffurl_open_h(&c->hd, furl, flags | AVIO_FLAG_READ,
                            h->headers, &reason_code)) < 0) {
        PR_PRINT("[prcrypto_open]Unable to open input,reason:%d\n", reason_code);
        goto err;
    }
    ret = url_lpopen(c->hd, PR_BUF_SIZE);

    if (!c->istvp) {
        h->is_slowmedia = 1;    //set slow media for notvp
    }
    h->is_streamed = 1;
    h->is_segment_media = 1;
    h->max_packet_size = TS_PACKET_SIZE;//for tvp: do not use avio loop buffer
    h->fastdetectedinfo = 1;
    h->http_code = reason_code;
    h->support_time_seek = c->hd->support_time_seek;
    reset_inout_ptr(c);

    /*Dump NOTVP clear data*/
    if (am_getconfig_bool("libplayer.prts.dump")) {
        c->mBackupFile = NULL;
        char backup[MAX_URL_SIZE];
        char* fstart = strrchr(uri, '/');
        char* stime = NULL;
        int stlen = 0;

        memset(backup, 0, MAX_URL_SIZE);
        strcpy(backup, BACK_FILE_PATH);
        snprintf(backup + strlen(BACK_FILE_PATH), MAX_URL_SIZE - strlen(BACK_FILE_PATH), "%s", fstart);
        backup[strlen(backup) + 1] = '\0';
        PR_DBG("prcrypto backup file path %s\n", backup);
        c->mBackupFile = fopen(backup, "wb");
        if (c->mBackupFile == NULL) {
            PR_DBG("Failed to create backup file");
        }
    }
    if (am_getconfig_bool("libplayer.prts.dbg")) {
        dbg_flag = 1;
    } else {
        dbg_flag = 0;
    }
    return 0;
err:
    h->http_code = reason_code;
    return ret;
}

static int prcrypto_close(URLContext *h)
{
    PRCryptoContext *c = h->priv_data;
    if (c->hd) {
        ffurl_close(c->hd);
    }
    if (c->ecm_packet.buf) {
        av_free(c->ecm_packet.buf);
        c->ecm_packet.buf = NULL;
    }
    if (c->mBackupFile) {
        fclose(c->mBackupFile);
    }

    PRclose = pr_close();
    if (PRclose) {
        PRclose(c->keydat);
    }
    PR_DBG("[prcrypto_close]\n");
    return 0;
}

static int64_t prcrypto_seek(URLContext *h, int64_t off, int whence)
{
    PRCryptoContext *c = h->priv_data;

    PR_DBG("r prcrypto_seek %lld, c->hd %x, off %lld, whence %d\n",
           off, c->hd, off, whence);
    if (c->hd) {
        if ((whence == 0 && off == 0)) {
            reset_inout_ptr(c);
            return url_lpseek(c->hd, off, whence);
        }
    }
    return -1;
}

static int64_t prcrypto_exseek(URLContext *h, int64_t off, int whence)
{
    PRCryptoContext *c = h->priv_data;
    int64_t pos;

    if (c->hd) {
        pos =  url_lpexseek(c->hd, off, whence);
        if (whence == AVSEEK_TO_TIME) {
            reset_inout_ptr(c);
        }
        PR_DBG("prcrypto_exseek whence %d, off %lld, pos %lld\n", whence, off, pos);
        return pos;
    }
    PR_PRINT("prcrypto_exseek whence %d, return error\n", whence);
    return AVERROR(EINVAL);
}

URLProtocol pr_crypto_protocol = {
    .name            = "cryptopr",
    .url_open        = prcrypto_open,
    .url_read        = prcrypto_read,
    .url_seek        = prcrypto_seek,
    .url_exseek      = prcrypto_exseek,
    .url_close       = prcrypto_close,
    .priv_data_size  = sizeof(PRCryptoContext),
    .priv_data_class = &crypto_pr_class,
    .flags           = URL_PROTOCOL_FLAG_NESTED_SCHEME,
};

