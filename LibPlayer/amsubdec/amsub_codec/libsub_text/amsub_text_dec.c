
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

#include "amsub_text_dec.h"
#include "amsub_dec_output.h"


#define  LOG_TAG    "amsub_text_dec"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

char *restbuf = NULL;
int restlen = 0;

int text_subtitle_decode(amsub_dec_t *amsub_dec, int read_handle);



int amsub_dec_init(amsub_dec_opt_t *amsubdec_ops)
{
    int ret = 0;
    amsub_dec_t *amsub_dec = NULL;
    LOGI("amsub_dec_init, subtitle text !\n");
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
    LOGI("amsub_dec_release,text subtitle\n");
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
    LOGI("enter amsub_dec_decode,text subtitle\n");
    amsub_dec = amsubdec_ops->priv_data;
    read_handle = amsubdec_ops->amsub_handle;
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
    while (amsub_dec->mstate == RUNNING)
    {
        memset(amsub_para, 0, sizeof(amsub_para_s));
        amsub_para->sync_bytes = 0x414d4c55;
        usleep(200 * 1000);
        if (amsub_dec->mstate == RUNNING)
        {
            ret = subtitle_poll_sub_fd(read_handle, 10);
            if (ret == 0)
            {
                LOGI("---xxxx--codec_poll_sub_fd fail,-retry \n");
                continue;
            }
            if (amsub_dec->amsub_para->amsub_data_l->sub_num >= text_sub_num)
            {
                LOGI("The text sub num reached to max sub num !\n");
                continue;
            }
            text_subtitle_decode(amsub_dec, read_handle);
        }
        else
        {
            break;
        }
    }
    LOGI("amsub_dec->mstate=%d\n", amsub_dec->mstate);
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



int text_subtitle_decode(amsub_dec_t *amsub_dec, int read_handle)
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
            goto error;
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
        duration_pts = spu_buf_piece[rd_oft++] << 24;
        duration_pts |= spu_buf_piece[rd_oft++] << 16;
        duration_pts |= spu_buf_piece[rd_oft++] << 8;
        duration_pts |= spu_buf_piece[rd_oft++];
        //  sublen = 1000;
        amsub_ps->sub_type = SUBTITLE_TMD_TXT;
        amsub_ps->buffer_size = current_length + 1;
        amsub_ps->amsub_data = malloc(amsub_ps->buffer_size);
        memset(amsub_ps->amsub_data, 0, amsub_ps->buffer_size);
        amsub_ps->pts = current_pts;
        //amsub_ps->m_delay = duration_pts;
        if (duration_pts != 0)
        {
            amsub_ps->m_delay += current_pts;
        }
        rd_oft += 2;
        current_length -= 2;
        if (current_length == 0)
            ret = -1;
        memcpy(amsub_ps->amsub_data, spu_buf_piece + rd_oft, current_length);
        LOGI("CODEC_ID_TIME_TEXT   size is:    %u ,data is:    %s, current_length=%d\n", amsub_ps->buffer_size, amsub_ps->amsub_data, current_length);
        ret = 0;
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
