/*****************************************************************
**                                                              **
**  Copyright (C) 2007 Amlogic,Inc.                             **
**  All rights reserved                                         **
**                                                              **
**                                                              **
*****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

#include "amsub_dec.h"
#include "amsub_dec_output.h"



#include <android/log.h>

#define  LOG_TAG    "amsub_xsub_dec"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

char *restbuf = NULL;
int restlen = 0;


//static char *pixData1, *pixData2;
#define OSD_HALF_SIZE (1920*1280/8)

#define str2ms(s) (((s[1]-0x30)*3600*10+(s[2]-0x30)*3600+(s[4]-0x30)*60*10+(s[5]-0x30)*60+(s[7]-0x30)*10+(s[8]-0x30))*1000+(s[10]-0x30)*100+(s[11]-0x30)*10+(s[12]-0x30))


typedef struct _DivXSubPictColor
{
    char red;
    char green;
    char blue;
} DivXSubPictColor;

#pragma pack(1)
typedef struct _DivXSubPictHdr
{
    char duration[27];
    unsigned short width;
    unsigned short height;
    unsigned short left;
    unsigned short top;
    unsigned short right;
    unsigned short bottom;
    unsigned short field_offset;
    DivXSubPictColor background;
    DivXSubPictColor pattern1;
    DivXSubPictColor pattern2;
    DivXSubPictColor pattern3;
    unsigned char *rleData;
} DivXSubPictHdr;

typedef struct _DivXSubPictHdr_HD
{
    char duration[27];
    unsigned short width;
    unsigned short height;
    unsigned short left;
    unsigned short top;
    unsigned short right;
    unsigned short bottom;
    unsigned short field_offset;
    DivXSubPictColor background;
    DivXSubPictColor pattern1;
    DivXSubPictColor pattern2;
    DivXSubPictColor pattern3;
    unsigned char background_transparency;  //HD profile only
    unsigned char pattern1_transparency;    //HD profile only
    unsigned char pattern2_transparency;    //HD profile only
    unsigned char pattern3_transparency;    //HD profile only
    unsigned char *rleData;
} DivXSubPictHdr_HD;
#pragma pack()


int xsub_subtitle_decode(amsub_dec_t *amsub_dec, int read_handle);


static unsigned short DecodeRL(unsigned short RLData, unsigned short *pixelnum, unsigned short *pixeldata)
{
    unsigned short nData = RLData;
    unsigned short nShiftNum;
    unsigned short nDecodedBits;
    if (nData & 0xc000)
        nDecodedBits = 4;
    else if (nData & 0x3000)
        nDecodedBits = 8;
    else if (nData & 0x0c00)
        nDecodedBits = 12;
    else
        nDecodedBits = 16;
    nShiftNum = 16 - nDecodedBits;
    *pixeldata = (nData >> nShiftNum) & 0x0003;
    *pixelnum = nData >> (nShiftNum + 2);
    return nDecodedBits;
}

static unsigned short GetWordFromPixBuffer(unsigned short bitpos, unsigned short *pixelIn)
{
    unsigned char hi = 0, lo = 0, hi_ = 0, lo_ = 0;
    char *tmp = (char *)pixelIn;
    hi = *(tmp + 0);
    lo = *(tmp + 1);
    hi_ = *(tmp + 2);
    lo_ = *(tmp + 3);
    if (bitpos == 0)
    {
        return (hi << 0x8 | lo);
    }
    else
    {
        return(((hi << 0x8 | lo) << bitpos) | ((hi_ << 0x8 | lo_) >> (16 - bitpos)));
    }
}


unsigned char FillPixel(char *ptrPXDRead, char *pixelOut, int n, amsub_para_s *sub_frame, int field_offset)
{
    unsigned short nPixelNum = 0, nPixelData = 0;
    unsigned short nRLData, nBits;
    unsigned short nDecodedPixNum = 0;
    unsigned short i, j;
    unsigned short rownum = sub_frame->spu_width;//
    unsigned short height = sub_frame->spu_height;//
    unsigned short nAddPixelAtStart = 0, nAddPixelAtEnd = 1;
    unsigned short bg_data = 0;
    unsigned char spu_data = 0xff;
    unsigned short pixTotalNum = 0;
    unsigned short PXDBufferBitPos  = 0, WrOffset = 16, PXDRdOffsetEven = 0, PXDRdOffsetOdd = 0;
    unsigned short totalBits = 0;
    pixTotalNum = height * rownum;
    unsigned short *ptrPXDWrite = pixelOut;
    unsigned short *ptrPXDWriteEnd = 0;
    int current_pixdata = 1;
    // 4 buffer for pix data
    //    if (n==1) { // 1 for odd
    //        if (current_pixdata == 1){
    //            memset(pixData1, 0, OSD_HALF_SIZE);
    //            ptrPXDWrite = (unsigned short *)pixData1;
    //        }
    //        else if (current_pixdata == 2){
    //            memset(pixData2, 0, OSD_HALF_SIZE);
    //            ptrPXDWrite = (unsigned short *)pixData2;
    //        }
    //        else {
    //            return -1;
    //        }
    //    }
    //    else if (n==2) {        // 2 for even
    //        if (current_pixdata == 1){
    //            memset(pixData1+OSD_HALF_SIZE, 0, OSD_HALF_SIZE);
    //            ptrPXDWrite = (unsigned short *)(pixData1+OSD_HALF_SIZE);
    //        }
    //        else if (current_pixdata == 2){
    //            memset(pixData2+OSD_HALF_SIZE, 0, OSD_HALF_SIZE);
    //            ptrPXDWrite = (unsigned short *)(pixData2+OSD_HALF_SIZE);
    //        }
    //        else {
    //            return -1;
    //        }
    //    }
    //    else {
    //        return -1;
    //    }
    ptrPXDWriteEnd = (unsigned short *)(((char *)ptrPXDWrite) + OSD_HALF_SIZE);
    for (j = 0; j < height / 2; j++)
    {
        while (nDecodedPixNum < rownum)
        {
            nRLData = GetWordFromPixBuffer(PXDBufferBitPos, ptrPXDRead);
            nBits = DecodeRL(nRLData, &nPixelNum, &nPixelData);
            PXDBufferBitPos += nBits;
            if (PXDBufferBitPos >= 16)
            {
                PXDBufferBitPos -= 16;
                ptrPXDRead += 2;
            }
            if (nPixelNum == 0)
            {
                nPixelNum = rownum - nDecodedPixNum % rownum;
            }
            for (i = 0; i < nPixelNum; i++)
            {
                WrOffset -= 2;
                *ptrPXDWrite |= nPixelData << WrOffset;
                //              ASSERT((ptrPXDWrite>=ptrPXDWriteEnd), "AVI: subtitle write pointer out of range\n");
                if (WrOffset == 0)
                {
                    WrOffset = 16;
                    ptrPXDWrite++;
                    // avoid out of range
                    if (ptrPXDWrite >= ptrPXDWriteEnd)
                        ptrPXDWrite = ptrPXDWriteEnd - 1;
                    //              *ptrPXDWrite = bg_data;
                }
            }
            totalBits += nBits;
            nDecodedPixNum += nPixelNum;
        }
        if (PXDBufferBitPos == 4)            //Rule 6
        {
            PXDBufferBitPos = 8;
        }
        else if (PXDBufferBitPos == 12)
        {
            PXDBufferBitPos = 0;
            ptrPXDRead += 2;
        }
        if (WrOffset != 16)
        {
            WrOffset = 16;
            ptrPXDWrite++;
            // avoid out of range
            if (ptrPXDWrite >= ptrPXDWriteEnd)
            {
                ptrPXDWrite = ptrPXDWriteEnd - 1;
            }
        }
        nDecodedPixNum -= rownum;
    }
    if (totalBits == field_offset)
    {
        return 1;
    }
    return 0;
}





int fill_resize_data(amsub_para_s *amsub_p, int *dst_data, int *src_data)
{
    if (amsub_p->resize_size == get_amsub_size(amsub_p))
    {
        //LOGI("-[vob]--fill_resize_data,resize_size==height*width-\n");
        memcpy(dst_data, src_data, amsub_p->resize_size * 4);
        return 0;
    }
    int y_start = amsub_p->resize_ystart;
    int x_start = amsub_p->resize_xstart;
    int y_end = y_start + amsub_p->resize_height;
    int resize_width = amsub_p->resize_width;
    int buffer_width = amsub_p->spu_width;
    int buffer_height = amsub_p->spu_height;
    int buffer_width_size = (buffer_width + 63) & 0xffffffc0;
    int *resize_src_data = src_data + buffer_width_size * y_start;
    int i = y_start;
    for (; i < y_end; i++)
    {
        memcpy(dst_data + (resize_width * (i - y_start)),
               resize_src_data + (buffer_width_size * (i - y_start)) + x_start,
               resize_width * 4);
    }
    return 0;
}

int *parser_inter_spu(amsub_para_t *amsub_para, int *buffer)
{
    LOGI("enter parser_inter_sup \n\n");
    unsigned short i = 0, j = 0;
    unsigned char *data = NULL, *data2 = NULL;
    unsigned char color = 0;
    unsigned *result_buf = (unsigned *)buffer;
    unsigned index = 0, index1 = 0;
    unsigned char n = 0;
    unsigned short buffer_width, buffer_height;
    int start_height = -1, end_height = 0;
    amsub_para_s *amsub_para_d = amsub_para->amsub_p;
    buffer_width = amsub_para_d->spu_width;
    buffer_height = amsub_para_d->spu_height;
    int resize_width = buffer_width;
    int resize_height = amsub_para_d->spu_height;
    int x_start = buffer_width;
    int x_end = 0;
    unsigned data_byte = (((buffer_width * 2) + 15) >> 4) << 1;
    //LOGI("data_byte is %d\n\n",data_byte);
    int buffer_width_size = (buffer_width + 63) & 0xffffffc0;
    //LOGI("buffer_width is %d\n\n",buffer_width_size);
    unsigned short subtitle_alpha = amsub_para_d->spu_alpha;
    LOGI("subtitle_alpha is %x\n\n", subtitle_alpha);
    unsigned int RGBA_Pal[4];
    RGBA_Pal[0] = RGBA_Pal[1] = RGBA_Pal[2] = RGBA_Pal[3] = 0;
#if 0
    if ((subtitle_alpha == 0xff0))
    {
        RGBA_Pal[2] = 0xffffffff;
        RGBA_Pal[1] = 0xff0000ff;
    }
    else if ((subtitle_alpha == 0xfff0))
    {
        RGBA_Pal[1] = 0xffffffff;
        RGBA_Pal[2] = 0xff000000;
        RGBA_Pal[3] = 0xff000000;
    }
    else if ((subtitle_alpha == 0xf0f0))
    {
        RGBA_Pal[1] = 0xffffffff;
        RGBA_Pal[3] = 0xff000000;
    }
    else if (subtitle_alpha == 0xf0ff)
    {
        RGBA_Pal[1] = 0xffffffff;
        RGBA_Pal[2] = 0xff000000;
        RGBA_Pal[3] = 0xff000000;
    }
    else if ((subtitle_alpha == 0xff00))
    {
        RGBA_Pal[2] = 0xffffffff;
        RGBA_Pal[3] = 0xff000000;
    }
    else if (subtitle_alpha == 0xfe0)
    {
        RGBA_Pal[1] = 0xffffffff;
        RGBA_Pal[2] = 0xff000000;
        RGBA_Pal[3] = 0;
    }
    else
    {
        RGBA_Pal[1] = 0xffffffff;
        RGBA_Pal[3] = 0xff000000;
    }
#else
    if (amsub_para_d->rgba_enable)
    {
        RGBA_Pal[0] = amsub_para_d->rgba_background;
        RGBA_Pal[1] = amsub_para_d->rgba_pattern1;
        RGBA_Pal[2] = amsub_para_d->rgba_pattern2;
        RGBA_Pal[3] = amsub_para_d->rgba_pattern3;
        LOGI(" RGBA_Pal[0] == 0x%x, RGBA_Pal[1] == 0x%x\n", RGBA_Pal[0] , RGBA_Pal[1]);
        LOGI(" RGBA_Pal[2] == 0x%x, RGBA_Pal[3] == 0x%x\n", RGBA_Pal[2] , RGBA_Pal[3]);
    }
    else if (subtitle_alpha & 0xf000 && subtitle_alpha & 0x0f00 && \
             subtitle_alpha & 0x00f0)
    {
        RGBA_Pal[1] = 0xffffffff;
        RGBA_Pal[2] = 0xff000000;
        RGBA_Pal[3] = 0xff000000;
    }
    else if (subtitle_alpha == 0xfe0)
    {
        RGBA_Pal[1] = 0xffffffff;
        RGBA_Pal[2] = 0xff000000;
        RGBA_Pal[3] = 0;
    }
    else
    {
        RGBA_Pal[1] = 0xffffffff;
        RGBA_Pal[3] = 0xff000000;
    }
#endif
    for (i = 0; i < buffer_height; i++)
    {
        if (i & 1)
            data = amsub_para_d->amsub_data + (i >> 1) * data_byte + (VOB_SUB_SIZE / 2);
        else
            data = amsub_para_d->amsub_data + (i >> 1) * data_byte;
        index = 0;
        for (j = 0; j < buffer_width; j++)
        {
            index1 = index % 2 ? index - 1 : index + 1;
            n = data[index1];
            index++;
            if (start_height < 0)
            {
                start_height = i;
                //start_height = (start_height%2)?(start_height-1):start_height;
            }
            end_height = i;
            if (j < x_start)
                x_start = j;
            result_buf[i * (buffer_width_size) + j] = RGBA_Pal[(n >> 6) & 0x3];
            if (++j >= buffer_width)    break;
            result_buf[i * (buffer_width_size) + j] = RGBA_Pal[(n >> 4) & 0x3];
            if (++j >= buffer_width)    break;
            result_buf[i * (buffer_width_size) + j] = RGBA_Pal[(n >> 2) & 0x3];
            if (++j >= buffer_width)    break;
            result_buf[i * (buffer_width_size) + j] = RGBA_Pal[n & 0x3];
            if (j > x_end)
                x_end = j;
        }
    }
    //end_height = (end_height%2)?(((end_height+1)<=buffer_height)?(end_height+1):end_height):end_height;
    amsub_para_d->resize_xstart = x_start;
    amsub_para_d->resize_ystart = start_height;
    amsub_para_d->resize_width = (x_end - x_start + 1 + 63) & 0xffffffc0;
    amsub_para_d->resize_height = end_height - start_height + 1;
    amsub_para_d->resize_size = amsub_para_d->resize_height * amsub_para_d->resize_width;
    LOGI("resize startx is %d\n\n", amsub_para_d->resize_xstart);
    LOGI("resize starty is %d\n\n", amsub_para_d->resize_ystart);
    LOGI("resize height is %d,height=%d\n\n", amsub_para_d->resize_height, amsub_para_d->spu_height);
    LOGI("resize_width is %d,width=%d\n\n", amsub_para_d->resize_width, amsub_para_d->spu_width);
    return (result_buf + start_height * buffer_width_size);
}


char int2byteArray(int num)
{
    char bytes[4] = {0};
    bytes[0] = (char)(0xff & num);
    bytes[1] = (char)((0xff00 & num) >> 8);
    bytes[2] = (char)((0xff0000 & num) >> 16);
    bytes[3] = (char)((0xff000000 & num) >> 24);
    return bytes[0];
}


int amsub_vob_reset_data(amsub_para_t *amsub_para)
{
    char dump_path[128];
    amsub_para_s *amsub_p = amsub_para->amsub_p;
    int i = 0, j = 0;
    int sub_size = get_amsub_size(amsub_p);
    if (sub_size <= 0)
    {
        LOGE("sub_size invalid \n\n");
        return -1;
    }
    LOGE("vob sub_size is %d\n\n", sub_size);
    int *inter_sub_data = NULL;
    inter_sub_data = malloc(sub_size * 4);
    if (inter_sub_data == NULL)
    {
        LOGE("malloc sub_size fail \n\n");
        return -1;
    }
    memset(inter_sub_data, 0x0, sub_size * 4);
    parser_inter_spu(amsub_para, inter_sub_data);
    int *resize_data = malloc(amsub_p->resize_size * 4);
    if (resize_data == NULL)
    {
        free(inter_sub_data);
        return -1;
    }
    fill_resize_data(amsub_p, resize_data, inter_sub_data);
    memset(amsub_p->amsub_data, 0, VOB_SUB_SIZE);
    memcpy(amsub_p->amsub_data, (char *)resize_data, amsub_p->resize_size * 4);
    amsub_p->buffer_size = amsub_p->resize_size * 4;
    amsub_p->spu_width = amsub_p->resize_width;
    amsub_p->spu_height = amsub_p->resize_height;
#if 0
    sprintf(dump_path, "/data/amsub_out_f%d.log", pp++);
    FILE *fp1 = fopen(dump_path, "w");
    if (fp1)
    {
        int flen = fwrite(amsub_p->amsub_data, 1, amsub_p->buffer_size, fp1);
        LOGI("-pp =%d,flen = %d---outlen=%d ", pp, flen, amsub_p->buffer_size);
        fclose(fp1);
    }
    else
    {
        LOGI("could not open file:amsub_out");
    }
#endif
    if (inter_sub_data)
    {
        free(inter_sub_data);
        inter_sub_data = NULL;
    }
    if (resize_data)
    {
        free(resize_data);
        resize_data = NULL;
    }
    return 0;
}





int amsub_dec_init(amsub_dec_opt_t *amsubdec_ops)
{
    int ret = 0;
    amsub_dec_t *amsub_dec = NULL;
    LOGI("amsub_dec_init, subtitle avi/xsub !\n");
    amsub_dec = amsubdec_ops->priv_data;
    amsubdec_ops->amsub_handle = open_sub_device();
    if (amsubdec_ops->amsub_handle < 0)
    {
        LOGI("open amstream_sub device failed !\n");
        return -1;
    }
    ret = amsub_dec_out_init(amsub_dec->amsub_para);
    if (ret != 0)
    {
        LOGI("amsub decoder output init failed\n");
        amsub_dec->mstate = STOPPED;
        return ret;
    }
    return 0;
}


int amsub_dec_output(amsub_dec_opt_t *amsubdec_ops)
{
    return 0;
}


int amsub_dec_release(amsub_dec_opt_t *amsubdec_ops)
{
    amsub_dec_t *amsub_dec;
    LOGI("amsub_dec_release,avi/xsub subtitle\n");
    if (amsubdec_ops)
    {
        amsub_dec = amsubdec_ops->priv_data;
    }
    else
    {
        return -1;
    }
    if (amsub_dec->amsub_para)
    {
        amsub_dec_out_close(amsub_dec->amsub_para);
    }
    if (amsub_dec->amsub_para)
    {
        free(amsub_dec->amsub_para);
        amsub_dec->amsub_para = NULL;
    }
    if (amsubdec_ops->amsub_handle)
    {
        close(amsubdec_ops->amsub_handle);
    }
    LOGI("amsub_dec_release,over\n");
    return 0;
}

int amsub_dec_decode(amsub_dec_opt_t *amsubdec_ops)
{
    int ret = 0;
    int read_handle;
    amsub_dec_t *amsub_dec;
    LOGI("enter amsub_dec_decode,avi/xsub subtitle\n");
    amsub_dec = amsubdec_ops->priv_data;
    read_handle = amsubdec_ops->amsub_handle;
    LOGI("amsub_dec_decode,read_handle=%d\n", read_handle);
    //amsub_dec->amsub_para = (amsub_para_t *)malloc(sizeof(amsub_para_t));
    //memset(amsub_dec->amsub_para,0, sizeof(amsub_para_t));
    amsub_para_s *amsub_para = NULL;
    amsub_para = (amsub_para_s *)malloc(sizeof(amsub_para_s));
    if (amsub_para == NULL)
    {
        LOGI("failed to malloc amsub_para !\n");
        return -1;
    }
    amsub_dec->amsub_para->amsub_p = amsub_para;
    INIT_LIST_HEAD(&amsub_dec->amsub_para->amsub_p->list);
    LOGI("amsub_dec->mstate=%d\n", amsub_dec->mstate);
    while (amsub_dec->mstate == RUNNING)
    {
        //LOGI("--0-amsub_dec->mstate=%d--\n",amsub_dec->mstate);
        memset(amsub_para, 0, sizeof(amsub_para_s));
        amsub_para->sync_bytes = 0x414d4c55;
        usleep(200 * 1000);
        if (amsub_dec->mstate == RUNNING)
        {
            ret = subtitle_poll_sub_fd(read_handle, 10);
            if (ret == 0)
            {
                LOGI("---xxxx--codec_poll_sub_fd fail \n");
                continue;
            }
            if (amsub_dec->amsub_para->amsub_data_l->sub_num >= bitmap_sub_num)
            {
                LOGI("The xsub sub num reached to max sub num\n");
                continue;
            }
            xsub_subtitle_decode(amsub_dec, read_handle);
        }
        else
        {
            break;
        }
    }
    //LOGI("--1-amsub_dec->mstate=%d--\n",amsub_dec->mstate);
    if (amsub_para)
    {
        free(amsub_para);
        amsub_para = NULL;
    }
    if (restbuf)
    {
        free(restbuf);
        restbuf = NULL;
    }
    amsub_dec->mstate = TERMINATED;
    return 0;
    /*
    error:
        if (amsub_dec->amsub_para) {
            free(amsub_dec->amsub_para);
            amsub_dec->amsub_para = NULL;
        }
        if (amsub_para) {
            free(amsub_para);
            amsub_para = NULL;
        }
        return ret;
        */
}



int xsub_subtitle_decode(amsub_dec_t *amsub_dec, int read_handle)
{
    int ret, rd_oft, wr_oft, size;
    char *spu_buf = NULL;
    unsigned int current_pts, duration_pts;
    unsigned int current_length, current_type;
    unsigned short *ptrPXDWrite = 0, *ptrPXDRead = 0;
    DivXSubPictHdr *avihandle = NULL;
    DivXSubPictHdr_HD *avihandle_hd = NULL;
    amsub_para_s *amsub_ps = amsub_dec->amsub_para->amsub_p;
    char dump_path[128];
#if 1
    size = subtitle_get_sub_size_fd(read_handle);
    if (size <= 0)
    {
        ret = -1;
        LOGI("\n player get sub size less than zero \n\n");
        goto error;
    }
    else
    {
        size += restlen;
        current_type = 0;
        spu_buf = malloc(size);
        LOGI("\n malloc subtitle size %d, restlen=%d, spu_buf=%x, \n\n", size, restlen, spu_buf);
    }
    int sizeflag = size;
    char *spu_buf_tmp = spu_buf;
    char *spu_buf_piece = spu_buf_piece;
#if 0
    FILE *fp1 = fopen("/data/amsub_out", "a+");
    if (fp1)
    {
        int flen = fwrite(spu_buf, 1, size, fp1);
        // LOGI("flen = %d---outlen=%d ", flen, VOB_SUB_SIZE);
        fclose(fp1);
    }
    else
    {
        LOGI("could not open file:amsub_out");
    }
#endif
    while (sizeflag > 30)
    {
        LOGI("\n sizeflag =%u  restlen=%d, \n\n", sizeflag, restlen);
        if (sizeflag <= 16)
        {
            ret = -1;
            LOGI("\n sizeflag is too little \n\n");
            goto error;
        }
        char *spu_buf_piece = spu_buf_tmp;
        // judge amsub decode state
        if (amsub_dec->mstate != RUNNING)
        {
            LOGI("[%s,%d], amsub_dec->mstate=%d--\n", __FUNCTION__, __LINE__, amsub_dec->mstate);
            break;
        }
        if (restlen)
            memcpy(spu_buf_piece, restbuf, restlen);
        if ((current_type == 0x17000 || current_type == 0x1700a) && restlen > 0)
        {
            LOGI("decode rest data!\n");
        }
        else
        {
            ret = subtitle_read_sub_data_fd(read_handle, spu_buf_piece + restlen, 16);
            sizeflag -= 16; spu_buf_tmp += 16;
        }
        rd_oft = 0;
        if ((spu_buf_piece[rd_oft++] != 0x41) || (spu_buf_piece[rd_oft++] != 0x4d) ||
                (spu_buf_piece[rd_oft++] != 0x4c) || (spu_buf_piece[rd_oft++] != 0x55) || (spu_buf_piece[rd_oft++] != 0xaa))
        {
            LOGI("\n wrong subtitle header :%x %x %x %x    %x %x %x %x    %x %x %x %x \n", spu_buf_piece[0], spu_buf_piece[1], spu_buf_piece[2], spu_buf_piece[3], spu_buf_piece[4], spu_buf_piece[5],
                 spu_buf_piece[6], spu_buf_piece[7], spu_buf_piece[8], spu_buf_piece[9], spu_buf_piece[10], spu_buf_piece[11]);
            ret = subtitle_read_sub_data_fd(read_handle, spu_buf_piece, sizeflag);
            sizeflag = 0;
            LOGI("\n\n ******* find wrong subtitle header!! ******\n\n");
            ret = -1;
            goto error;         // wrong head
        }
        LOGI("\n\n ******* find correct subtitle header ******\n\n");
        current_type = spu_buf_piece[rd_oft++] << 16;
        current_type |= spu_buf_piece[rd_oft++] << 8;
        current_type |= spu_buf_piece[rd_oft++];
        current_length = spu_buf_piece[rd_oft++] << 24;
        current_length |= spu_buf_piece[rd_oft++] << 16;
        current_length |= spu_buf_piece[rd_oft++] << 8;
        current_length |= spu_buf_piece[rd_oft++];
        current_pts = spu_buf_piece[rd_oft++] << 24;
        current_pts |= spu_buf_piece[rd_oft++] << 16;
        current_pts |= spu_buf_piece[rd_oft++] << 8;
        current_pts |= spu_buf_piece[rd_oft++];
        LOGI("sizeflag=%u, current_type:%x, current_pts is 0x%x, current_length is %d, \n", sizeflag, current_type, current_pts, current_length);
        if (current_length > sizeflag)
        {
            LOGI("current_length > size");
            ret = subtitle_read_sub_data_fd(read_handle, spu_buf_piece, sizeflag);
            sizeflag = 0;
            ret = -1;
            goto error;
        }
        ret = subtitle_read_sub_data_fd(read_handle, spu_buf_piece + 16, current_length + 4);
        sizeflag -= (current_length + 4);
        spu_buf_tmp += (current_length + 4);
        restlen = 0;
        // judge amsub decode state
        if (amsub_dec->mstate != RUNNING)
        {
            LOGI("[%s,%d], amsub_dec->mstate=%d--\n", __FUNCTION__, __LINE__, amsub_dec->mstate);
            goto error;
        }
        switch (current_type)
        {
            case 0x17003:   //XSUB
                duration_pts = spu_buf_piece[rd_oft++] << 24;
                duration_pts |= spu_buf_piece[rd_oft++] << 16;
                duration_pts |= spu_buf_piece[rd_oft++] << 8;
                duration_pts |= spu_buf_piece[rd_oft++];
                LOGI("duration_pts is %d, current_length=%d  ,rd_oft is %d\n", duration_pts, current_length, rd_oft);
                avihandle = (DivXSubPictHdr *)(spu_buf_piece + rd_oft);
                amsub_ps->amsub_data = malloc(VOB_SUB_SIZE);
                memset(amsub_ps->amsub_data, 0, VOB_SUB_SIZE);
                //sublen = 50;
                amsub_ps->sub_type = SUBTITLE_VOB;
                amsub_ps->buffer_size  = VOB_SUB_SIZE;
                {
                    unsigned char  *s = &(avihandle->duration[0]);
                    amsub_ps->pts = str2ms(s) * 90;
                    s = &(avihandle->duration[13]);
                    amsub_ps->m_delay = str2ms(s) * 90;
                }
                amsub_ps->spu_width = avihandle->width;
                amsub_ps->spu_height = avihandle->height;
                LOGI(" spu->pts:%x, spu->spu_width is 0x%x,  spu->spu_height=0x%x\n  spu->spu_width is %u,  spu->spu_height=%u\n", amsub_ps->pts, avihandle->width, avihandle->height, amsub_ps->spu_width, amsub_ps->spu_height);
                amsub_ps->rgba_enable = 1;  // XSUB
                //FFT:The background pixels are 100% transparent
                amsub_ps->rgba_background = (unsigned)avihandle->background.red << 16 | (unsigned)avihandle->background.green << 8 | (unsigned)avihandle->background.blue | 0 << 24;
                amsub_ps->rgba_pattern1 = (unsigned)avihandle->pattern1.red << 16 | (unsigned)avihandle->pattern1.green << 8 | (unsigned)avihandle->pattern1.blue | 0xff << 24;
                amsub_ps->rgba_pattern2 = (unsigned)avihandle->pattern2.red << 16 | (unsigned)avihandle->pattern2.green << 8 | (unsigned)avihandle->pattern2.blue | 0xff << 24;
                amsub_ps->rgba_pattern3 = (unsigned)avihandle->pattern3.red << 16 | (unsigned)avihandle->pattern3.green << 8 | (unsigned)avihandle->pattern3.blue | 0xff << 24;
                LOGI(" spu->rgba_background == 0x%x,  spu->rgba_pattern1 == 0x%x\n", amsub_ps->rgba_background, amsub_ps->rgba_pattern1);
                LOGI(" spu->rgba_pattern2 == 0x%x,  spu->rgba_pattern3 == 0x%x\n", amsub_ps->rgba_pattern2, amsub_ps->rgba_pattern3);
                // judge amsub decode state
                if (amsub_dec->mstate != RUNNING)
                {
                    LOGI("[%s,%d], amsub_dec->mstate=%d--\n", __FUNCTION__, __LINE__, amsub_dec->mstate);
                    if (amsub_ps->amsub_data)
                    {
                        free(amsub_ps->amsub_data);
                        amsub_ps->amsub_data = NULL;
                    }
                    goto error;
                }
                ptrPXDRead = (unsigned short *) & (avihandle->rleData);
                FillPixel(ptrPXDRead, amsub_ps->amsub_data, 1, amsub_ps, avihandle->field_offset);
                ptrPXDRead = (unsigned short *)((unsigned long)(&avihandle->rleData) + (unsigned long)(avihandle->field_offset));
                FillPixel(ptrPXDRead, amsub_ps->amsub_data + VOB_SUB_SIZE / 2, 2, amsub_ps, avihandle->field_offset);
                ret = 0;
                break;
            case 0x17008:   //XSUB HD
            case 0x17009:   //XSUB+ (XSUA HD)
                duration_pts = spu_buf_piece[rd_oft++] << 24;
                duration_pts |= spu_buf_piece[rd_oft++] << 16;
                duration_pts |= spu_buf_piece[rd_oft++] << 8;
                duration_pts |= spu_buf_piece[rd_oft++];
                LOGI("duration_pts is %d, current_length=%d  ,rd_oft is %d\n", duration_pts, current_length, rd_oft);
                avihandle_hd = (DivXSubPictHdr_HD *)(spu_buf_piece + rd_oft);
                amsub_ps->amsub_data = malloc(VOB_SUB_SIZE);
                memset(amsub_ps->amsub_data, 0, VOB_SUB_SIZE);
                //sublen = 50;
                amsub_ps->sub_type = SUBTITLE_VOB;
                amsub_ps->buffer_size  = VOB_SUB_SIZE;
                {
                    unsigned char  *s = &(avihandle_hd->duration[0]);
                    amsub_ps->pts = str2ms(s) * 90;
                    s = &(avihandle_hd->duration[13]);
                    amsub_ps->m_delay = str2ms(s) * 90;
                }
                amsub_ps->spu_width = avihandle_hd->width;
                amsub_ps->spu_height = avihandle_hd->height;
                LOGI(" spu->spu_width is 0x%x,  spu->spu_height=0x%x\n  spu->spu_width is %u,  spu->spu_height=%u\n", avihandle_hd->width, avihandle_hd->height, amsub_ps->spu_width, amsub_ps->spu_height);
                // judge amsub decode state
                if (amsub_dec->mstate != RUNNING)
                {
                    LOGI("[%s,%d], amsub_dec->mstate=%d--\n", __FUNCTION__, __LINE__, amsub_dec->mstate);
                    if (amsub_ps->amsub_data)
                    {
                        free(amsub_ps->amsub_data);
                        amsub_ps->amsub_data = NULL;
                    }
                    goto error;
                }
                amsub_ps->rgba_enable = 1;  // XSUB
                amsub_ps->rgba_background = (unsigned)avihandle_hd->background.red << 16 | (unsigned)avihandle_hd->background.green << 8 | (unsigned)avihandle_hd->background.blue | avihandle_hd->background_transparency << 24;
                amsub_ps->rgba_pattern1 = (unsigned)avihandle_hd->pattern1.red << 16 | (unsigned)avihandle_hd->pattern1.green << 8 | (unsigned)avihandle_hd->pattern1.blue | avihandle_hd->pattern1_transparency << 24;
                amsub_ps->rgba_pattern2 = (unsigned)avihandle_hd->pattern2.red << 16 | (unsigned)avihandle_hd->pattern2.green << 8 | (unsigned)avihandle_hd->pattern2.blue | avihandle_hd->pattern2_transparency << 24;
                amsub_ps->rgba_pattern3 = (unsigned)avihandle_hd->pattern3.red << 16 | (unsigned)avihandle_hd->pattern3.green << 8 | (unsigned)avihandle_hd->pattern3.blue | avihandle_hd->pattern3_transparency << 24;
                LOGI(" avihandle_hd->background.red == 0x%x,  avihandle_hd->background.green == 0x%x\n", avihandle_hd->background.red, avihandle_hd->background.green);
                LOGI(" avihandle_hd->background.blue == 0x%x,  avihandle_hd->background_transparency == 0x%x\n\n", avihandle_hd->background.blue, avihandle_hd->background_transparency);
                LOGI(" avihandle_hd->pattern1.red == 0x%x,  avihandle_hd->pattern1.green == 0x%x\n", avihandle_hd->pattern1.red, avihandle_hd->pattern1.green);
                LOGI(" avihandle_hd->pattern1.blue == 0x%x, avihandle_hd->pattern1_transparency == 0x%x\n\n", avihandle_hd->pattern1.blue, avihandle_hd->pattern1_transparency);
                LOGI(" avihandle_hd->pattern2.red == 0x%x,  avihandle_hd->pattern2.green == 0x%x\n", avihandle_hd->pattern2.red, avihandle_hd->pattern2.green);
                LOGI(" avihandle_hd->pattern2.blue == 0x%x, avihandle_hd->pattern@_transparency == 0x%x\n\n", avihandle_hd->pattern2.blue, avihandle_hd->pattern2_transparency);
                LOGI(" avihandle_hd->pattern3.red == 0x%x,  avihandle_hd->pattern3.green == 0x%x\n", avihandle_hd->pattern3.red, avihandle_hd->pattern3.green);
                LOGI(" avihandle_hd->pattern3.blue == 0x%x, avihandle_hd->pattern3_transparency == 0x%x\n\n", avihandle_hd->pattern3.blue, avihandle_hd->pattern3_transparency);
                LOGI(" spu->rgba_background == 0x%x,  spu->rgba_pattern1 == 0x%x\n", amsub_ps->rgba_background, amsub_ps->rgba_pattern1);
                LOGI(" spu->rgba_pattern2 == 0x%x,  spu->rgba_pattern3 == 0x%x\n", amsub_ps->rgba_pattern2, amsub_ps->rgba_pattern3);
                ptrPXDRead = (unsigned short *) & (avihandle_hd->rleData);
                FillPixel(ptrPXDRead, amsub_ps->amsub_data, 1, amsub_ps, avihandle_hd->field_offset);
                ptrPXDRead = (unsigned short *)((unsigned long)(&avihandle_hd->rleData) + (unsigned long)(avihandle_hd->field_offset));
                FillPixel(ptrPXDRead, amsub_ps->amsub_data + VOB_SUB_SIZE / 2, 2, amsub_ps, avihandle_hd->field_offset);
                ret = 0;
                break;
            default:
                ret = -1;
                break;
        }
        if (ret < 0)
            goto error;
        // judge amsub decode state
        if (amsub_dec->mstate != RUNNING)
        {
            LOGI("[%s,%d], amsub_dec->mstate=%d--\n", __FUNCTION__, __LINE__, amsub_dec->mstate);
            if (amsub_ps->amsub_data)
            {
                free(amsub_ps->amsub_data);
                amsub_ps->amsub_data = NULL;
            }
            goto error;
        }
        amsub_vob_reset_data(amsub_dec->amsub_para);
        //write_subtitle_file(spu);
        amsub_dec_out_add(amsub_dec->amsub_para);
#if 0
        sprintf(dump_path, "/data/amsub_out_f%d.log", pp++);
        FILE *fp1 = fopen(dump_path, "w");
        if (fp1)
        {
            int flen = fwrite(amsub_para_d->amsub_data, 1, VOB_SUB_SIZE, fp1);
            LOGI("-pp =%d,flen = %d---outlen=%d ", pp, flen, VOB_SUB_SIZE);
            fclose(fp1);
        }
        else
        {
            LOGI("could not open file:amsub_out");
        }
#endif
    }
error:
    LOGI("[%s::%d] spu_buf=%x, \n", __FUNCTION__, __LINE__, spu_buf);
    if (spu_buf)
    {
        free(spu_buf);
        spu_buf = NULL;
    }
    return ret;
#endif
}
