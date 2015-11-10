#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

//#include "list.h"
#include "amsub_dec.h"
#include "amsub_log.h"




int amsub_dec_out_init(amsub_para_t *amsub_para)
{
    // init subdata to store output subtitle paraments and data
    amsub_para->amsub_data_l = (subdata_t *)malloc(sizeof(subdata_t));
    if (!amsub_para->amsub_data_l)
    {
        amsub_print("%s,amsub_para->amsub_data_l malloc failed !\n", __FUNCTION__);
        return -1;
    }
    memset(amsub_para->amsub_data_l, 0, sizeof(subdata_t));
    //amsub_print("----amsub_para->amsub_data_l=%p---\n",amsub_para->amsub_data_l);
    INIT_LIST_HEAD(&amsub_para->amsub_data_l->list);
    //INIT_LIST_HEAD(&amsub_para->amsub_p->list);
    amsub_para->amsub_data_l->sub_num = 0;
    lp_lock_init(&(amsub_para->amsub_data_l->amsub_lock), NULL);
    amsub_print("amsub_dec_out_init ok !\n");
    return 0;
}

int amsub_dec_out_add(amsub_para_t *amsub_para)
{
    subdata_t *subdata = NULL;
    amsub_para_s *amsub_ps = NULL;
    lp_lock(&(amsub_para->amsub_data_l->amsub_lock));
    subdata = amsub_para->amsub_data_l;
    amsub_ps = (amsub_para_s *)malloc(sizeof(amsub_para_s));
    memset(amsub_ps, 0, sizeof(amsub_para_s));
    memcpy(amsub_ps, amsub_para->amsub_p, sizeof(amsub_para_s));
    //amsub_print("-amsub_dec_out_add--subdata=%p,amsub_ps->amsub_data=%p,amsub_ps=%p",subdata,amsub_ps->amsub_data,amsub_ps);
    //amsub_print("-amsub_dec_out_add--amsub_para->list=%p,%p",amsub_para->list,&amsub_para->list);
    //amsub_print("-amsub_dec_out_add--subdata->list=%p,",subdata->list);
    list_add_tail(&amsub_ps->list, &subdata->list);
    //amsub_print("\n---XXXXXXXXXXXXXXXXXXXXXX------0--entry=%p-----\n",&amsub_ps->list);
    dump_amsub_data_info(amsub_ps);
    //amsub_print("---XXXXXXXXXXXXXXXXXXXXXX------1----amsub_ps=%p---\n",amsub_ps);
    subdata->sub_num++;
    amsub_print("amsub output subtitle num=%d\n", subdata->sub_num);
    lp_unlock(&(amsub_para->amsub_data_l->amsub_lock));
    return 0;
}

void dump_amsub_data_info(amsub_para_s *amsub_para)
{
    //int size = amsub_para->spu_width * amsub_para->spu_height;
    amsub_print("subtitle data info: \n\n");
    amsub_print("sub_type       :  %d--\n", amsub_para->sub_type);
    amsub_print("sub_pts        :  %u--\n", amsub_para->pts);
    amsub_print("sub_delay      :  %u--\n", amsub_para->m_delay);
    amsub_print("sub_width      :  %d--\n", amsub_para->spu_width);
    amsub_print("sub_height     :  %d--\n", amsub_para->spu_height);
    amsub_print("sub_buf_size   :  %d--\n", amsub_para->buffer_size);
    amsub_print("sub_amsub_data :  %p--\n", amsub_para->amsub_data);
    return NULL;
}

void amsub_get_odata_info(amsub_para_s *amsub_para, amsub_info_t *amsub_info)
{
    amsub_info->sub_type = amsub_para->sub_type;
    amsub_info->pts = amsub_para->pts;
    amsub_info->m_delay = amsub_para->m_delay;
    amsub_info->sub_width = amsub_para->spu_width;
    amsub_info->sub_height = amsub_para->spu_height;
    amsub_info->buffer_size = amsub_para->buffer_size;
    amsub_info->odata = amsub_para->amsub_data;
    amsub_print("amsub_get_odata_info over !\n");
    return NULL;
}


int amsub_dec_out_get(amsub_para_t *amsub_para, amsub_info_t *amsub_info)
{
    subdata_t *subdata = NULL;
    lp_lock(&(amsub_para->amsub_data_l->amsub_lock));
    subdata = amsub_para->amsub_data_l;
    //amsub_print("subtitle list have sub num : %d,subdata=%p\n",subdata->sub_num,subdata);
    if (list_empty(&subdata->list))
    {
        amsub_print("subtitle list empty, retry next times !\n");
        lp_unlock(&(amsub_para->amsub_data_l->amsub_lock));
        return -1;
    }
    amsub_para_s *sub_ps = NULL;
    sub_ps = list_first_entry(&subdata->list, amsub_para_s, list);
    amsub_get_odata_info(sub_ps, amsub_info);
    //dump_amsub_data_info(sub_ps);
    //amsub_print("--amsub_dec_out_get-subdata->sub_num=%d,amsub_ps=%p,amsub_ps->list=%p-\n",
    //subdata->sub_num,sub_ps,&sub_ps->list);
    list_del(&sub_ps->list);
    if (sub_ps)
    {
        free(sub_ps);
        sub_ps = NULL;
    }
    if (subdata->sub_num > 0)
        subdata->sub_num--;
    amsub_print("amsub_dec_out_get ok\n");
    lp_unlock(&(amsub_para->amsub_data_l->amsub_lock));
    return 0;
}

/*
int amsub_dec_data_get(unsigned int curr_timems)
{

    //amsub_idxsub_data_get(curr_timems);
    return 0;

}

*/
#if 1
int amsub_dec_out_close(amsub_para_t *amsub_para)
{
    list_t *entry;
    subdata_t *subdata;
    lp_lock(&(amsub_para->amsub_data_l->amsub_lock));
    subdata = amsub_para->amsub_data_l;
    entry = &subdata->list;
    amsub_print("amsub_dec_out_close,subdata_num=%d \n", subdata->sub_num);
    entry = &subdata->list;
    while (entry != subdata->list.next)
    {
        if (list_empty(entry))
        {
            amsub_print("subtitle list empty, close over\n");
            lp_unlock(&(amsub_para->amsub_data_l->amsub_lock));
            return 0;
        }
        amsub_para_s *amsub_ps = list_first_entry(entry, amsub_para_s, list);
        if (amsub_ps != NULL)
        {
            amsub_print("amsub_dec_out_close,sub_pts=%d \n", amsub_ps->pts);
            //amsub_print("--amsub_dec_out_close--amsub_ps->amsub_data=%p---\n",amsub_ps->amsub_data);
            if (amsub_ps->amsub_data)
            {
                //amsub_print("--amsub_dec_out_close--amsub_ps->amsub_data=%p---\n",amsub_ps->amsub_data);
                free(amsub_ps->amsub_data);
                amsub_ps->amsub_data = NULL;
            }
            list_del(&amsub_ps->list);
            //entry = entry->next;
            free(amsub_ps);
        }
    }
    subdata->sub_num = 0;
    lp_unlock(&(amsub_para->amsub_data_l->amsub_lock));
    if (amsub_para->amsub_data_l)
    {
        free(amsub_para->amsub_data_l);
        amsub_para->amsub_data_l = NULL;
    }
    return 0;
}
#endif

int get_amsub_size(amsub_para_s *amsub_p)
{
    if ((amsub_p->sub_type == SUBTITLE_VOB) || (amsub_p->sub_type == SUBTITLE_DVB))
    {
        int subtitle_width = amsub_p->spu_width;
        int subtitle_height = amsub_p->spu_height;
        if (subtitle_width * subtitle_height == 0)
            return 0;
        int buffer_width = (subtitle_width + 63) & 0xffffffc0;
        //amsub_p->resize_width = buffer_width;
        amsub_print("[vob/dvb] buffer width=%d ,height=%d\n", buffer_width, subtitle_height);
        return buffer_width * subtitle_height;
    }
    else if ((amsub_p->sub_type == SUBTITLE_SSA) || (amsub_p->sub_type == SUBTITLE_TMD_TXT))
    {
        amsub_print("[SSA/TMD_TXT] data_size is %d\n", amsub_p->buffer_size);
        return amsub_p->buffer_size;
    }
    else if (amsub_p->sub_type == SUBTITLE_PGS)
    {
        amsub_print("[SSA] data_size is %d\n", amsub_p->buffer_size);
        return amsub_p->buffer_size / 4;
    }
    return 0;
}

int amsub_dec_out_ctrl()
{
    return 0;
}
