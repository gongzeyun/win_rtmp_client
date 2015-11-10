#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "amstream.h"

#include "amsub_log.h"
#include "amsub_dec.h"
#include "amsub_io_ctrl.h"


/* --------------------------------------------------------------------------*/
/**
* @ function   aml_sub_start
* @ brief        subtitle decode start,init subtitle decode.
*/
/* --------------------------------------------------------------------------*/

void aml_sub_start(void **amsub_handle, amsub_info_t *amsub_info)
{
    int ret = 0;
    amsub_dec_t *amsub_dec;
    amsub_dec = (amsub_dec_t *)malloc(sizeof(amsub_dec_t));
    if (amsub_dec == NULL)
    {
        amsub_print("malloc amsub_dec failed !\n");
        return;
    }
    memset(amsub_dec, 0, sizeof(amsub_dec_t));
    amsub_dec->mstate = INITING;
    amsub_dec->sub_type = amsub_info->sub_type;
    amsub_dec->sub_pid = amsub_info->sub_pid;
    amsub_dec->stream_type = amsub_info->stream_type;
    amsub_dec->sub_filename = amsub_info->sub_filename;
    amsub_print("aml_sub_start,sub_type=0x%x,sub_pid=%d,stream_type=%d,mstate=%d \n",
                amsub_dec->sub_type, amsub_dec->sub_pid, amsub_dec->stream_type, amsub_dec->mstate);
    amsub_dec->need_stop = 0;
    ret = amsub_decode_init(amsub_dec);
    if (ret)
    {
        amsub_print("amsub_decode_init failed!");
        goto error;
    }
    *amsub_handle = (void *)amsub_dec;
    amsub_print("aml_sub_start ok, amsub_dec=%p \n", amsub_dec);
    return NULL;
error:
    if (amsub_dec)
    {
        free(amsub_dec);
        amsub_dec = NULL;
    }
    return;
}

/* --------------------------------------------------------------------------*/
/**
* @ function  aml_sub_stop
* @ brief       subtitle decode stop ,stop subtitle decode.
*/
/* --------------------------------------------------------------------------*/

int aml_sub_stop(void *priv)
{
    int ret = 0;
    amsub_dec_t *amsub_dec = (amsub_dec_t *)priv;
    if (amsub_dec)
    {
        amsub_dec->mstate = STOPPED;
        ret = amsub_decode_stop(amsub_dec);
    }
    return ret;
}

/* --------------------------------------------------------------------------*/
/**
* @ function  aml_sub_release
* @ brief       subtitle decode release ,release subtitle decode. kill subtitle decode thread
*/
/* --------------------------------------------------------------------------*/

int aml_sub_release(void **amsub_handle)
{
    int ret = 0;
    amsub_dec_t *amsub_dec = (amsub_dec_t *)*amsub_handle;
    ret = amthreadpool_pthread_join(amsub_dec->amsub_dec_ctrl_tid, NULL);
    if (amsub_dec)
    {
        free(amsub_dec);
        amsub_dec = NULL;
    }
    amsub_print("aml_sub_release over\n");
    return ret;
}

/* --------------------------------------------------------------------------*/
/**
* @ function  aml_sub_read_odata
* @ brief       read subtitle data from subtitle output data list
*/
/* --------------------------------------------------------------------------*/

int aml_sub_read_odata(void **amsub_handle, amsub_info_t *amsub_info)
{
    int ret = 0;
    amsub_dec_t *amsub_dec = (amsub_dec_t *)*amsub_handle;
    //amsub_print("---amsub_dec=%p-amsub_dec->sub_filename=%s--\n",amsub_dec,amsub_dec->sub_filename);
    if (amsub_dec && amsub_dec->sub_filename && (amsub_dec->mstate == RUNNING))
    {
        amsub_dec_opt_t *amsubdec_ops = amsub_dec->amsub_ops;
        ret = amsubdec_ops->getinfo(amsubdec_ops, amsub_info);
        if (ret != 0)
        {
            amsub_print("aml_sub_read_odata,failed\n");
            return -1;
        }
    }
    else if (amsub_dec && amsub_dec->amsub_para)
    {
        //amsub_print("---amsub_dec->amsub_para=%p---\n",amsub_dec->amsub_para);
        amsub_para_t *amsub_para = amsub_dec->amsub_para;
        if (amsub_dec->mstate != RUNNING)
        {
            amsub_print("aml_sub_read_odata,failed,not in running !\n");
            return -1;
        }
        ret = amsub_read_sub_data(amsub_para, amsub_info);
        if (ret != 0)
        {
            amsub_print("aml_sub_read_odata,failed\n");
            return -1;
        }
    }
    else
    {
        amsub_print("aml_sub_read_odata,can not get amsub_para handle\n");
        return -1;
    }
    return 0;
}


#if 0
int amsub_esdata_write(void *buffer, int len)
{
}

int amsub_esdata_read(void *buffer, int len)
{
}
#endif
