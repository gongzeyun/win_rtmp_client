#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include "amsub_dec.h"
#include "amsub_log.h"
#include "amsub_dec_output.h"
#include <amthreadpool.h>



void *amsub_dec_loop_ctrl(void *args);
//void *amsub_decode_loop(void *args);
void amsub_decode_loop(amsub_dec_t *amsub_dec);



/*audio decoder list structure*/
typedef struct
{
    int sub_type;
    char    name[64];
} amsub_lib_t;


amsub_lib_t amsub_lib_list[] =
{
    {CODEC_ID_DVD_SUBTITLE, "libsub_vob.so"},       // case 0x17000: vob internal image;
    {CODEC_ID_DVB_SUBTITLE, "libsub_dvb.so"},       // case 0x17001: dvb internal image;
    {CODEC_ID_TEXT, "libsub_ass.so"},               // case 0x17002:text,mkv internel utf-8;
    {CODEC_ID_XSUB, "libsub_xsub.so"},              // case 0x17003:xsub image;
    {CODEC_ID_SSA, "libsub_ass.so"},                // case 0x17004:text,mkv internel ssa;
    {CODEC_ID_MOV_TEXT, "libsub_text.so"},          // case 0x17005; text, 3gpp,eg;
    {CODEC_ID_HDMV_PGS_SUBTITLE, "libsub_pgs.so"},  // case 0x17006:pgs image;
    {CODEC_ID_SRT, "libsub_xsub.so"},               // case 0x17008:xsub HD  image;
    {CODEC_ID_MICRODVD, "libsub_xsub.so"},          // case 0x17009:XSUB+ (XSUA HD) image;
    {CODEC_ID_IDX_SUB, "libsub_idxsub.so"},         // case 0x17010: idx + sub;

    NULL
} ;

int find_amsub_lib(amsub_dec_t *amsub_dec)
{
    int i;
    int num;
    amsub_lib_t *f;
    int fd = 0;
    num = ARRAY_SIZE(amsub_lib_list);
    amsub_dec_opt_t *amsubdec_ops = amsub_dec->amsub_ops;
    for (i = 0; i < num; i++)
    {
        f = &amsub_lib_list[i];
        amsub_print("find_amsub_lib,amsub_lib_list[i].name=%s,sub_type=0x%x,amsub_dec->sub_type=0x%x\n",
                    amsub_lib_list[i].name, f->sub_type, amsub_dec->sub_type);
        if (f->sub_type == amsub_dec->sub_type)
        {
            fd = dlopen(amsub_lib_list[i].name, RTLD_NOW);
            if (fd != 0)
            {
                amsubdec_ops->init    = dlsym(fd, "amsub_dec_init");
                amsubdec_ops->decode  = dlsym(fd, "amsub_dec_decode");
                amsubdec_ops->release = dlsym(fd, "amsub_dec_release");
                amsubdec_ops->getinfo = dlsym(fd, "amsub_dec_getinfo");
            }
            else
            {
                amsub_print("cant find decoder lib,%s\n", dlerror());
                return -1;
            }
            return 0;
        }
    }
    return -1;
}


int amsub_register_dec(amsub_dec_t *amsub_dec)
{
    int ret = 0;
    amsub_print("amsub_register_dec\n");
    if (amsub_dec->sub_type < CODEC_ID_DVD_SUBTITLE)
    {
        if (amsub_dec->sub_filename)
        {
            amsub_dec->sub_type = CODEC_ID_IDX_SUB;
        }
    }
    ret = find_amsub_lib(amsub_dec);
    if (ret != 0)
    {
        amsub_print("amsub register decoder failed\n");
        return -1;
    }
    amsub_dec->amsub_ops->priv_data = amsub_dec;
    return ret;
}

int amsub_decode_init(amsub_dec_t *amsub_dec)
{
    int ret = 0;
    pthread_t  tid;
    amsub_dec_opt_t *amsubdec_ops;
    amsub_dec->amsub_ops = (amsub_dec_opt_t *)malloc(sizeof(amsub_dec_opt_t));
    memset(amsub_dec->amsub_ops, 0, sizeof(amsub_dec_opt_t));
    amsubdec_ops = amsub_dec->amsub_ops;
    amsub_dec->amsub_para = (amsub_para_t *)malloc(sizeof(amsub_para_t));
    memset(amsub_dec->amsub_para, 0, sizeof(amsub_para_t));
    //amsub_print("---amsub_dec_decode,amsub_dec->amsub_para=%p--\n",amsub_dec->amsub_para);
    ret = amsub_register_dec(amsub_dec);
    if (ret != 0)
    {
        amsub_print("[%s],this subtitle not support !\n", __FUNCTION__);
        goto error1;
    }
    ret = amsubdec_ops->init(amsubdec_ops);
    if (ret != 0)
    {
        amsub_print("amsub decode init failed\n");
        goto error1;
    }
    amsub_dec->mstate = INITTED;
    //amsub_print("amsub_decode_init-amsub_dec->mstate=%d\n",amsub_dec->mstate);
    {
        //create amsub_dec_loop_ctrl
        ret = amthreadpool_pthread_create(&tid, NULL, (void *)amsub_dec_loop_ctrl, (void *)amsub_dec);
        //pthread_mutex_init(&subdec->amsub_thread.pthread_mutex, NULL);
        //pthread_cond_init(&subdec->amsub_thread.pthread_cond, NULL);   // not understand
        if (ret != 0)
        {
            amsub_print("Create amsub decode loop thread failed!\n");
            goto error2;
        }
        amsub_dec->amsub_dec_ctrl_tid = tid;
        pthread_setname_np(amsub_dec->amsub_dec_ctrl_tid, "AmsubdecLP");   // not understand
    }
    return ret;
error2:
    if (amsub_dec->amsub_para->amsub_data_l)
    {
        free(amsub_dec->amsub_para->amsub_data_l);
        amsub_dec->amsub_para->amsub_data_l = NULL;
    }
error1:
    if (amsub_dec->amsub_para)
    {
        free(amsub_dec->amsub_para);
        amsub_dec->amsub_para = NULL;
    }
    if (amsubdec_ops)
    {
        free(amsubdec_ops);
        amsubdec_ops = NULL;
    }
    return ret;
}


int amsub_decode_stop(amsub_dec_t *amsub_dec)
{
    int ret = 0;
    amsub_dec_opt_t *amsubdec_ops = amsub_dec->amsub_ops;
    amsub_print("amsub_decode_stop !\n");
    while (1)
    {
        if (amsub_dec->mstate == TERMINATED)
        {
            amsubdec_ops->release(amsubdec_ops);
            break;
        }
        else
        {
            amthreadpool_thread_usleep(10 * 1000);
        }
    }
    amsub_print("amsub_decode_stop,amsubdec_ops=%p\n", amsubdec_ops);
    if (amsubdec_ops)
    {
        free(amsubdec_ops);
        amsubdec_ops = NULL;
    }
    //ret = amthreadpool_pthread_join(amsub_dec->amsub_dec_tid, NULL);
    amsub_print("[%s]amsub_decode_loop thread exit success\n", __FUNCTION__);
    //amsub_dec->mstate = TERMINATED;
    return ret;
}


int amsub_dec_start_thread(amsub_dec_t *amsub_dec)
{
    int ret = 0;
    pthread_t  tid;
    amsub_print("amsub_dec_start_thread-amsub_dec->mstate=%d\n", amsub_dec->mstate);
    {
        //create amsub_dec_loop
        ret = amthreadpool_pthread_create(&tid, NULL, (void *)amsub_decode_loop, (void *)amsub_dec);
        //pthread_mutex_init(&subdec->amsub_thread.pthread_mutex, NULL);
        //pthread_cond_init(&subdec->amsub_thread.pthread_cond, NULL);   // not understand
        if (ret != 0)
        {
            amsub_print("Create amsub decode loop thread failed!\n");
            return ret;
        }
        amsub_dec->amsub_dec_tid = tid;
        pthread_setname_np(amsub_dec->amsub_dec_tid, "amsub_decode_loop");   // not understand
    }
    return 0;
}


void *amsub_dec_loop_ctrl(void *args)
{
    int ret;
    amsub_dec_t *amsub_dec;
    amsub_dec = (amsub_dec_t *)args;
    amsub_print("amsub_dec_loop_ctrl thread-amsub_dec->mstate=%d\n", amsub_dec->mstate);
    //start decode thread
    //amsub_dec_start_thread(amsub_dec);
#if 1
    do
    {
        if (amsub_dec->mstate == INITTED)
        {
            amsub_dec->mstate = STARTED;
            // amsub_dec_start_thread(amsub_dec);
            amsub_decode_loop(amsub_dec);
        }
        else
        {
            if (amsub_dec->need_stop)
            {
                break;
            }
            amthreadpool_thread_usleep(10 * 1000);
        }
    }
    while (1);
#endif
    amsub_print("Exit amsub_dec_loop_ctrl Thread!");
    pthread_exit(NULL);
    return NULL;
}


int amsub_get_data_buffer(amsub_dec_t *amsub_dec, char *sub_data)
{
    return 0;
}

int amsub_get_data_amstrem(amsub_dec_t *amsub_dec, char *sub_data)
{
    return 0;
}



int amsub_get_esdata(amsub_dec_t *amsub_dec, char *sub_data)
{
    int ret = 0;
    if (amsub_dec->stream_type == STREAM_TYPE_ES_SUB)
    {
        amsub_get_data_buffer(amsub_dec, sub_data);
    }
    else
    {
        amsub_get_data_amstrem(amsub_dec, sub_data);
    }
    return 0;
}
#if 0
void *amsub_decode_loop(void *args)
{
    int ret;
    amsub_dec_t *amsub_dec;
    amsub_print("---amsub_decode_loop--\n");
    amsub_dec = (amsub_dec_t *)args;
    amsub_dec_opt_t *amsubdec_ops = amsub_dec->amsub_ops;
#if 1
    do
    {
        if (amsub_dec->mstate == STARTED)
        {
            amsub_dec->mstate = RUNNING;
            amsubdec_ops->decode(amsubdec_ops);
        }
        else
        {
            amthreadpool_thread_usleep(10 * 1000);
        }
    }
    while (amsub_dec->mstate != TERMINATED);
#endif
    amsub_print("exit amsub_decode_loop Thread finished!");
    pthread_exit(NULL);
    return NULL;
}
#endif

void amsub_decode_loop(amsub_dec_t *amsub_dec)
{
    int ret;
    amsub_print("amsub_decode_loop\n");
    amsub_dec_opt_t *amsubdec_ops = amsub_dec->amsub_ops;
#if 1
    do
    {
        if (amsub_dec->mstate == STARTED)
        {
            amsub_dec->mstate = RUNNING;
            //amsub_print("-[%s]--amsub_dec->mstate=%d--\n",__FUNCTION__,amsub_dec->mstate);
            amsubdec_ops->decode(amsubdec_ops);
        }
        else
        {
            usleep(5 * 1000);
        }
    }
    while (amsub_dec->mstate != TERMINATED);
#endif
    amsub_dec->need_stop = 1;
    amsub_print("exit amsub_decode_loop finished!");
    return NULL;
}
