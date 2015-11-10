/************************************************
 * name :vob_sub.c
 * function :decoder relative functions
 * data     :2010.8.10
 * author       :FFT
 * version  :1.0.0
 *************************************************/
//header file
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/ioctl.h>

#include <android/log.h>

#include "amsub_vob_dec.h"
#include "amsub_dec_output.h"

#define  LOG_TAG    "amsub_vob_dec"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
static uint32_t palette[16];
static int has_palette = 0;

char *restbuf = NULL;
int restlen = 0;

int pp = 0;



unsigned short doDCSQC(unsigned char *pdata, unsigned char *pend)
{
    unsigned short cmdDelay, cmdDelaynew;
    unsigned short temp;
    unsigned short cmdAddress;
    int Done, stoped;
    cmdDelay = *pdata++;
    cmdDelay <<= 8;
    cmdDelay += *pdata++;
    cmdAddress = *pdata++;
    cmdAddress <<= 8;
    cmdAddress += *pdata++;
    cmdDelaynew = 0;
    Done = 0;
    stoped = 0;
    while (!Done)
    {
        switch (*pdata)
        {
            case FSTA_DSP:
                pdata++;
                break;
            case STA_DSP:
                pdata++;
                break;
            case STP_DSP:
                pdata++;
                stoped = 1;
                break;
            case SET_COLOR:
                pdata += 3;
                break;
            case SET_CONTR:
                pdata += 3;
                break;
            case SET_DAREA:
                pdata += 7;
                break;
            case SET_DSPXA:
                pdata += 7;
                break;
            case CHG_COLCON:
                temp = *pdata++;
                temp = temp << 8;
                temp += *pdata++;
                pdata += temp;
                break;
            case CMD_END:
                pdata++;
                Done = 1;
                break;
            default:
                pdata = pend;
                Done = 1;
                break;
        }
    }
    if ((pdata < pend) && (stoped == 0))
        cmdDelaynew = doDCSQC(pdata, pend);
    return cmdDelaynew > cmdDelay ? cmdDelaynew : cmdDelay;
}

static int get_spu_cmd(amsub_para_s *sub_frame)
{
    unsigned short temp;
    unsigned char *pCmdData;
    unsigned char *pCmdEnd;
    unsigned char data_byte0, data_byte1;
    unsigned char spu_cmd;
    if (sub_frame->cmd_offset >= sub_frame->length)
    {
        LOGI("cmd_offset bigger than frame_size\n\n");
        return -1;      //cmd offset > frame size
    }
    pCmdData = (unsigned char *)(sub_frame->amsub_data);
    pCmdEnd = pCmdData + sub_frame->length;
    pCmdData += sub_frame->cmd_offset;
    pCmdData += 4;
    while (pCmdData < pCmdEnd)
    {
        spu_cmd = *pCmdData++;
        switch (spu_cmd)
        {
            case FSTA_DSP:
                sub_frame->display_pending = 2;
                break;
            case STA_DSP:
                sub_frame->display_pending = 1;
                break;
            case STP_DSP:
                sub_frame->display_pending = 0;
                break;
            case SET_COLOR:
                temp = *pCmdData++;
                sub_frame->spu_color = temp << 8;
                temp = *pCmdData++;
                sub_frame->spu_color += temp;
                break;
            case SET_CONTR:
                temp = *pCmdData++;
                sub_frame->spu_alpha = temp << 8;
                temp = *pCmdData++;
                sub_frame->spu_alpha += temp;
                break;
            case SET_DAREA:
                data_byte0 = *pCmdData++;
                data_byte1 = *pCmdData++;
                sub_frame->spu_start_x = ((data_byte0 & 0x3f) << 4) | (data_byte1 >> 4);
                data_byte0 = *pCmdData++;
                sub_frame->spu_width = ((data_byte1 & 0x0f) << 8) | (data_byte0);
                sub_frame->spu_width = sub_frame->spu_width - sub_frame->spu_start_x + 1;
                data_byte0 = *pCmdData++;
                data_byte1 = *pCmdData++;
                sub_frame->spu_start_y = ((data_byte0 & 0x3f) << 4) | (data_byte1 >> 4);
                data_byte0 = *pCmdData++;
                sub_frame->spu_height = ((data_byte1 & 0x0f) << 8) | (data_byte0);
                sub_frame->spu_height = sub_frame->spu_height - sub_frame->spu_start_y + 1;
                if (sub_frame->spu_width > VOB_SUB_WIDTH)
                {
                    sub_frame->spu_width = VOB_SUB_WIDTH;
                }
                if (sub_frame->spu_height > VOB_SUB_HEIGHT)
                {
                    sub_frame->spu_height = VOB_SUB_HEIGHT;
                }
                LOGI("sub_frame->spu_width = %d, sub_frame->spu_height = %d \n", sub_frame->spu_width, sub_frame->spu_height);
                break;
            case SET_DSPXA:
                temp = *pCmdData++;
                sub_frame->top_pxd_addr = temp << 8;
                temp = *pCmdData++;
                sub_frame->top_pxd_addr += temp;
                temp = *pCmdData++;
                sub_frame->bottom_pxd_addr = temp << 8;
                temp = *pCmdData++;
                sub_frame->bottom_pxd_addr += temp;
                break;
            case CHG_COLCON:
                temp = *pCmdData++;
                temp = temp << 8;
                temp += *pCmdData++;
                pCmdData += temp;
                /*
                        uVobSPU.disp_colcon_addr = uVobSPU.point + uVobSPU.point_offset;
                        uVobSPU.colcon_addr_valid = 1;
                        temp = uVobSPU.disp_colcon_addr + temp - 2;

                        uSPU.point = temp & 0x1fffc;
                        uSPU.point_offset = temp & 3;
                */
                break;
            case CMD_END:
                if (pCmdData <= (pCmdEnd - 6))
                {
                    if ((sub_frame->m_delay = doDCSQC(pCmdData, pCmdEnd - 6)) > 0)
                        sub_frame->m_delay = sub_frame->m_delay * 1024 + sub_frame->pts;
                }
                // LOGI("get_spu_cmd parser to the end\n\n");
                return 0;
                break;
            default:
                return -1;
        }
    }
    LOGI("get_spu_cmd can not parser complete\n\n");
    return -1;
}

/**
 * Locale-independent conversion of ASCII isspace.
 */
static int av_isspace(int c)
{
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

static void parse_palette(char *p)
{
    int i;
    has_palette = 1;
    for (i = 0; i < 16; i++)
    {
        palette[i] = strtoul(p, &p, 16);
        while (*p == ',' || av_isspace(*p))
            p++;
    }
}

static int dvdsub_parse_extradata(char *extradata, int extradata_size)
{
    char *dataorig, *data;
    if (!extradata || !extradata_size)
        return 1;
    extradata += 5;
    extradata_size -= 5;
    dataorig = data = malloc(extradata_size + 1);
    if (!data)
        return 1;
    memcpy(data, extradata, extradata_size);
    data[extradata_size] = '\0';
    for (;;)
    {
        int pos = strcspn(data, "\n\r");
        if (pos == 0 && *data == 0)
            break;
        if (strncmp("palette:", data, 8) == 0)
        {
            parse_palette(data + 8);
        }
        data += pos;
        data += strspn(data, "\n\r");
    }
    free(dataorig);
    return 1;
}


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


unsigned char spu_fill_pixel(unsigned short *pixelIn, char *pixelOut, amsub_para_s *sub_frame, int n)
{
    unsigned short nPixelNum = 0, nPixelData = 0;
    unsigned short nRLData, nBits;
    unsigned short nDecodedPixNum = 0;
    unsigned short i, j;
    unsigned short PXDBufferBitPos  = 0, WrOffset = 16;
    unsigned short change_data = 0;
    unsigned short PixelDatas[4] = {0, 1, 2, 3};
    unsigned short rownum = sub_frame->spu_width;
    unsigned short height = sub_frame->spu_height;
    unsigned short _alpha = sub_frame->spu_alpha;
    static unsigned short *ptrPXDWrite;
    memset(pixelOut, 0, VOB_SUB_SIZE / 2);
    ptrPXDWrite = (unsigned short *)pixelOut;
    if (_alpha & 0xF)
    {
        _alpha = _alpha >> 4;
        change_data++;
        while (_alpha & 0xF)
        {
            change_data++;
            _alpha = _alpha >> 4;
        }
        PixelDatas[0] = change_data;
        PixelDatas[change_data] = 0;
        if (n == 2)
            sub_frame->spu_alpha = (sub_frame->spu_alpha & 0xFFF0) | (0x000F << (change_data << 2));
    }
    for (j = 0; j < height / 2; j++)
    {
        while (nDecodedPixNum < rownum)
        {
            nRLData = GetWordFromPixBuffer(PXDBufferBitPos, pixelIn);
            nBits = DecodeRL(nRLData, &nPixelNum, &nPixelData);
            PXDBufferBitPos += nBits;
            if (PXDBufferBitPos >= 16)
            {
                PXDBufferBitPos -= 16;
                pixelIn++;
            }
            if (nPixelNum == 0)
            {
                nPixelNum = rownum - nDecodedPixNum % rownum;
            }
            if (change_data)
            {
                nPixelData = PixelDatas[nPixelData];
            }
            for (i = 0; i < nPixelNum; i++)
            {
                WrOffset -= 2;
                *ptrPXDWrite |= nPixelData << WrOffset;
                if (WrOffset == 0)
                {
                    WrOffset = 16;
                    ptrPXDWrite++;
                }
            }
            nDecodedPixNum += nPixelNum;
        }
        if (PXDBufferBitPos == 4)            //Rule 6
        {
            PXDBufferBitPos = 8;
        }
        else if (PXDBufferBitPos == 12)
        {
            PXDBufferBitPos = 0;
            pixelIn++;
        }
        if (WrOffset != 16)
        {
            WrOffset = 16;
            ptrPXDWrite++;
        }
        nDecodedPixNum -= rownum;
    }
    return 0;
}



int fill_resize_data(amsub_para_s *amsub_p, int *dst_data, int *src_data)
{
    if (amsub_p->resize_size == get_amsub_size(amsub_p))
    {
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
    amsub_para_s *amsub_ps = amsub_para->amsub_p;
    buffer_width = amsub_ps->spu_width;
    buffer_height = amsub_ps->spu_height;
    int resize_width = buffer_width;
    int resize_height = amsub_ps->spu_height;
    int x_start = buffer_width;
    int x_end = 0;
    unsigned data_byte = (((buffer_width * 2) + 15) >> 4) << 1;
    //LOGI("data_byte is %d\n\n",data_byte);
    int buffer_width_size = (buffer_width + 63) & 0xffffffc0;
    //LOGI("buffer_width is %d\n\n",buffer_width_size);
    unsigned short subtitle_alpha = amsub_ps->spu_alpha;
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
    if (amsub_ps->rgba_enable)
    {
        RGBA_Pal[0] = amsub_ps->rgba_background;
        RGBA_Pal[1] = amsub_ps->rgba_pattern1;
        RGBA_Pal[2] = amsub_ps->rgba_pattern2;
        RGBA_Pal[3] = amsub_ps->rgba_pattern3;
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
            data = amsub_ps->amsub_data + (i >> 1) * data_byte + (VOB_SUB_SIZE / 2);
        else
            data = amsub_ps->amsub_data + (i >> 1) * data_byte;
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
    amsub_ps->resize_xstart = x_start;
    amsub_ps->resize_ystart = start_height;
    amsub_ps->resize_width = (x_end - x_start + 1 + 63) & 0xffffffc0;
    amsub_ps->resize_height = end_height - start_height + 1;
    amsub_ps->resize_size = amsub_ps->resize_height * amsub_ps->resize_width;
    LOGI("resize startx is %d\n\n", amsub_ps->resize_xstart);
    LOGI("resize starty is %d\n\n", amsub_ps->resize_ystart);
    LOGI("resize height is %d,height=%d\n\n", amsub_ps->resize_height, amsub_ps->spu_height);
    LOGI("resize_width is %d,width=%d\n\n", amsub_ps->resize_width, amsub_ps->spu_width);
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
    LOGI("amsub_dec_init,subtitle vob\n");
    amsub_dec = amsubdec_ops->priv_data;
    amsubdec_ops->amsub_handle = open_sub_device();
    if (amsubdec_ops->amsub_handle < 0)
    {
        LOGI("open amstream_sub device failed\n");
        return -1;
    }
    ret = amsub_dec_out_init(amsub_dec->amsub_para);
    if (ret != 0)
    {
        LOGI("amsub decoder output init failed\n");
        amsub_dec->mstate = STOPPED;
        return ret;
    }
    restlen = 0;
    pp = 0;
    vobsub_init_decoder();
    return 0;
}


int amsub_dec_output(amsub_dec_opt_t *amsubdec_ops)
{
    return 0;
}


int amsub_dec_release(amsub_dec_opt_t *amsubdec_ops)
{
    amsub_dec_t *amsub_dec;
    LOGI("amsub_dec_release,vob subtitle\n");
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
    restlen = 0;
    pp = 0;
    LOGI("amsub_dec_release,over\n");
    return 0;
}

int amsub_dec_decode(amsub_dec_opt_t *amsubdec_ops)
{
    int ret = 0;
    int read_handle;
    amsub_dec_t *amsub_dec;
    LOGI("enter amsub_dec_decode,vob subtitle\n");
    amsub_dec = amsubdec_ops->priv_data;
    read_handle = amsubdec_ops->amsub_handle;
    //LOGI("---amsub_dec_decode,read_handle=%d--\n",read_handle);
    //amsub_dec->amsub_para = (amsub_para_t *)malloc(sizeof(amsub_para_t));
    //memset(amsub_dec->amsub_para,0, sizeof(amsub_para_t));
    amsub_para_s *amsub_para = NULL;
    amsub_para = (amsub_para_s *)malloc(sizeof(amsub_para_s));
    if (amsub_para == NULL)
    {
        LOGI("failed to malloc amsub_para,,return\n");
        return -1;
    }
    amsub_dec->amsub_para->amsub_p = amsub_para;
    INIT_LIST_HEAD(&amsub_dec->amsub_para->amsub_p->list);
    //LOGI("---amsub_dec->mstate=%d--\n",amsub_dec->mstate);
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
                LOGI("The vob sub num reached to max sub num,num=%d\n", amsub_dec->amsub_para->amsub_data_l->sub_num);
                continue;
            }
            vob_subtitle_decode(amsub_dec, read_handle);
        }
        else
        {
            break;
        }
    }
    LOGI("%s,amsub_dec->mstate=%d,\n", __FUNCTION__, amsub_dec->mstate);
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



int vob_subtitle_decode(amsub_dec_t *amsub_dec, int read_handle)
{
    int ret, rd_oft, wr_oft, size;
    char *spu_buf = NULL;
    unsigned int current_pts, duration_pts;
    unsigned int current_length, current_type;
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
    // judge amsub decode state
    if (amsub_dec->mstate != RUNNING)
    {
        LOGI("[%s,%d], amsub_dec->mstate=%d--\n", __FUNCTION__, __LINE__, amsub_dec->mstate);
        goto error;
    }
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
        // judge amsub decode state
        if (amsub_dec->mstate != RUNNING)
        {
            LOGI("[%s,%d], amsub_dec->mstate=%d--\n", __FUNCTION__, __LINE__, amsub_dec->mstate);
            goto error;
        }
        if (current_length > sizeflag)
        {
            LOGI("current_length > size");
            ret = subtitle_read_sub_data_fd(read_handle, spu_buf_piece, sizeflag);
            sizeflag = 0;
            ret = -1;
            goto error;
        }
        ret = subtitle_read_sub_data_fd(read_handle, spu_buf_piece + restlen + 16, sizeflag - restlen);
        restlen = sizeflag;
        sizeflag = 0;
        spu_buf_tmp += current_length;
        LOGI("current_type=0x17000 or 0x1700a! restlen=%d, sizeflag=%d,\n", restlen, sizeflag);
        //  sublen = 50;
        amsub_ps->sub_type = SUBTITLE_VOB;
        amsub_ps->buffer_size  = VOB_SUB_SIZE;
        amsub_ps->amsub_data = malloc(VOB_SUB_SIZE);
        memset(amsub_ps->amsub_data, 0, VOB_SUB_SIZE);
        amsub_ps->pts = current_pts;
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
        ret = get_vob_spu(spu_buf_piece + rd_oft, &restlen, current_length, amsub_ps);
        LOGI("## ret=%d, restlen=%d, sizeflag=%d, restbuf=%x,%x, ---\n", ret, restlen, sizeflag, restbuf, restbuf ? restbuf[0] : 0);
        if (restlen < 0)
        {
            LOGI("Warning restlen <0, set to 0\n");
            restlen = 0;
        }
        if (restlen)
        {
            if (restbuf)
            {
                free(restbuf);
                restbuf = NULL;
            }
            restbuf = malloc(restlen);
            memcpy(restbuf, spu_buf_piece + rd_oft + ret, restlen);
            //LOGI("## %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,-----\n",
            //    restbuf[0],restbuf[1],restbuf[2],restbuf[3],restbuf[4],restbuf[5],restbuf[6],restbuf[7],
            //    restbuf[8],restbuf[9],restbuf[10],restbuf[11],restbuf[12],restbuf[13],restbuf[14],restbuf[15],
            //    restbuf[16],restbuf[17],restbuf[18],restbuf[19],restbuf[20],restbuf[21],restbuf[22],restbuf[23]);
            if ((restbuf[0] == 0x41) && (restbuf[1] == 0x4d) &&
                    (restbuf[2] == 0x4c) && (restbuf[3] == 0x55) && (restbuf[4] == 0xaa))
            {
                LOGI("## sub header found ! restbuf=%x,%x, ---\n", restbuf, restbuf[0]);
                sizeflag = restlen;
            }
            else
            {
                LOGI("## no header found, free restbuf! ---\n");
                free(restbuf);
                restbuf = NULL;
                restlen = sizeflag = 0;
            }
        }
        if (ret < 0)
        {
            if (amsub_ps->amsub_data)
            {
                free(amsub_ps->amsub_data);
                amsub_ps->amsub_data = NULL;
            }
            goto error;
        }
        // judge amsub decode state
        if (amsub_dec->mstate != RUNNING)
        {
            LOGI("[%s,%d], amsub_dec->mstate=%d--\n", __FUNCTION__, __LINE__, amsub_dec->mstate);
            if (amsub_ps->amsub_data)
            {
                free(amsub_ps->amsub_data);
                amsub_ps->amsub_data = NULL;
            }
            if (restbuf)
            {
                free(restbuf);
                restbuf = NULL;
            }
            goto error;
        }
        ret = amsub_vob_reset_data(amsub_dec->amsub_para);
        if (ret != 0)
        {
            LOGI("wrong vob subtitle parament\n");
            if (amsub_ps->amsub_data)
            {
                free(amsub_ps->amsub_data);
                amsub_ps->amsub_data = NULL;
            }
            goto error;
        }
        //write_subtitle_file(spu);
        amsub_dec_out_add(amsub_dec->amsub_para);
#if 0
        sprintf(dump_path, "/data/amsub_out_f%d.log", pp++);
        FILE *fp1 = fopen(dump_path, "w");
        if (fp1)
        {
            int flen = fwrite(amsub_ps->amsub_data, 1, VOB_SUB_SIZE, fp1);
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
    //LOGI("[%s::%d]  amsub_data=%x, \n",__FUNCTION__,__LINE__, amsub_ps->amsub_data);
    //if(amsub_ps->amsub_data){
    //  free(amsub_ps->amsub_data);
    //  amsub_ps->amsub_data = NULL;
    //}
    return ret;
#endif
}

int get_vob_spu(char *spu_buf, int *bufsize, unsigned length, amsub_para_s *spu)
{
    //  LOGI("spubuf  %x %x %x %x %x %x %x %x   %x %x %x %x %x %x %x %x  \n %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
    //      spu_buf[0], spu_buf[1],spu_buf[2],spu_buf[3],spu_buf[4],spu_buf[5],spu_buf[6],spu_buf[7],
    //      spu_buf[8],spu_buf[9],spu_buf[10],spu_buf[11],spu_buf[12],spu_buf[13],spu_buf[14],spu_buf[15],
    //      spu_buf[16],spu_buf[17],spu_buf[18],spu_buf[19],spu_buf[20],spu_buf[21],spu_buf[22],spu_buf[23],
    //      spu_buf[24],spu_buf[25],spu_buf[26],spu_buf[27],spu_buf[28],spu_buf[29],spu_buf[30],spu_buf[31] );
    int rd_oft, wr_oft, i;
    unsigned current_length = length;
    int ret = -1;
    char *pixDataOdd = NULL;
    char *pixDataEven = NULL;
    unsigned short *ptrPXDRead;
    rd_oft = 0;
    if (spu_buf[0] == 'E' && spu_buf[1] == 'X' && spu_buf[2] == 'T' && spu_buf[3] == 'R' && spu_buf[4] == 'A')
    {
        LOGI("## extradata %x,%x,%x,%x,%x,%x,%x,%x, x,%x,%x,%x,%x,%x,%x,%x, ----------\n",
             spu_buf[0], spu_buf[1], spu_buf[2], spu_buf[3], spu_buf[4], spu_buf[5], spu_buf[6], spu_buf[7],
             spu_buf[8], spu_buf[9], spu_buf[10], spu_buf[11], spu_buf[12], spu_buf[13], spu_buf[14], spu_buf[15]);
        dvdsub_parse_extradata(spu_buf, current_length);
        rd_oft += current_length;
        current_length = 0;
        *bufsize -= rd_oft;
        return 0;
    }
    spu->length = spu_buf[0] << 8;
    spu->length |= spu_buf[1];
    spu->cmd_offset = spu_buf[2] << 8;
    spu->cmd_offset |= spu_buf[3];
    memset(spu->amsub_data, 0, VOB_SUB_SIZE);
    wr_oft = 0;
    while (spu->length - wr_oft > 0)
    {
        if (!current_length)
        {
            LOGI("current_length is zero\n\n");
            if ((spu_buf[rd_oft++] != 0x41) || (spu_buf[rd_oft++] != 0x4d) ||
                    (spu_buf[rd_oft++] != 0x4c) || (spu_buf[rd_oft++] != 0x55) || (spu_buf[rd_oft++] != 0xaa))
            {
                LOGI("## goto error ---------\n");
                goto error;         // wrong head
            }
            rd_oft += 3;            // 3 bytes for type
            current_length = spu_buf[rd_oft++] << 24;
            current_length |= spu_buf[rd_oft++] << 16;
            current_length |= spu_buf[rd_oft++] << 8;
            current_length |= spu_buf[rd_oft++];
            rd_oft += 4;            // 4 bytes for pts
        }
        if ((wr_oft + current_length) <= spu->length)
        {
            memcpy(spu->amsub_data + wr_oft, spu_buf + rd_oft, current_length);
            rd_oft += current_length;
            wr_oft += current_length;
            current_length = 0;
        }
        if (wr_oft == spu->length)
        {
            get_spu_cmd(spu);
            spu->frame_rdy = 1;
            break;
        }
    }
    *bufsize -= rd_oft;
    if (has_palette)
    {
        spu->rgba_background = (palette[(spu->spu_color) & 0x0f] & 0x00ffffff)
                               | ((((spu->spu_alpha) & 0x0f) * 17U) << 24);
        spu->rgba_pattern1 = (palette[(spu->spu_color >> 4) & 0x0f] & 0x00ffffff)
                             | ((((spu->spu_alpha >> 4) & 0x0f) * 17U) << 24);
        spu->rgba_pattern2 = (palette[(spu->spu_color >> 8) & 0x0f] & 0x00ffffff)
                             | ((((spu->spu_alpha >> 8) & 0x0f) * 17U) << 24);
        spu->rgba_pattern3 = (palette[(spu->spu_color >> 12) & 0x0f] & 0x00ffffff)
                             | ((((spu->spu_alpha >> 12) & 0x0f) * 17U) << 24);
        spu->rgba_enable = 1;
    }
    // if one frame data is ready, decode it.
    LOGI("spu->frame_rdy is %d, restlen=%d, rd_oft=%d, \n\n", spu->frame_rdy, *bufsize, rd_oft);
    if (spu->frame_rdy == 1)
    {
        pixDataOdd = malloc(VOB_SUB_SIZE / 2);
        LOGI("pixDataOdd is %x\n\n", pixDataOdd);
        if (!pixDataOdd)
        {
            LOGI("pixDataOdd malloc fail\n\n");
            goto error;         //not enough memory
        }
        ptrPXDRead = (unsigned short *)(spu->amsub_data + spu->top_pxd_addr);
        spu_fill_pixel(ptrPXDRead, pixDataOdd, spu, 1);
        pixDataEven = malloc(VOB_SUB_SIZE / 2);
        LOGI("pixDataEven is %x\n\n", pixDataEven);
        if (!pixDataEven)
        {
            LOGI("pixDataEven malloc fail\n\n");
            goto error;         //not enough memory
        }
        ptrPXDRead = (unsigned short *)(spu->amsub_data + spu->bottom_pxd_addr);
        spu_fill_pixel(ptrPXDRead, pixDataEven, spu, 2);
        memset(spu->amsub_data, 0, VOB_SUB_SIZE);
#if 0
        for (i = 0; i < VOB_SUB_SIZE; i += spu->spu_width / 2)
        {
            memcpy(spu->amsub_data + i, pixDataOdd + i / 2, spu->spu_width / 4);
            memcpy(spu->amsub_data + i + spu->spu_width / 4, pixDataEven + i / 2, spu->spu_width / 4);
        }
#else
        memcpy(spu->amsub_data, pixDataOdd, VOB_SUB_SIZE / 2);
        memcpy(spu->amsub_data + VOB_SUB_SIZE / 2, pixDataEven, VOB_SUB_SIZE / 2);
#endif
        ret = 0;
    }
error:
    if (pixDataOdd)
    {
        LOGI("start free pixDataOdd\n\n");
        free(pixDataOdd);
        LOGI("end free pixDataOdd\n\n");
    }
    if (pixDataEven)
    {
        LOGI("start free pixDataEven\n\n");
        free(pixDataEven);
        LOGI("end free pixDataEven\n\n");
    }
    return rd_oft;
}

void vobsub_init_decoder(void)
{
    int i = 0;
    has_palette = 0;
    for (i = 0; i < 16; i++)
    {
        palette[i] = 0;
    }
}
