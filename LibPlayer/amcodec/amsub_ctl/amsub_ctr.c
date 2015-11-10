#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "amsub_internal_ctrl.h"


/* --------------------------------------------------------------------------*/
/**
* @brief  amsub_start   subtitle decode start
*/
/* --------------------------------------------------------------------------*/

void amsub_start(void **priv, amsub_info_t *amsub_info)
{
    aml_sub_start(priv, amsub_info);
    return;
}


/* --------------------------------------------------------------------------*/
/**
* @brief  amsub_stop   subtitle decode stop
*/
/* --------------------------------------------------------------------------*/
void amsub_stop(void **priv)
{
    aml_sub_stop(*priv);
    aml_sub_release(priv);
    return;
}

int amsub_esdata_write(void *buffer, int len)
{
    return 0;
}

int amsub_esdata_read(void *buffer, int len)
{
    return 0;
}

/* --------------------------------------------------------------------------*/
/**
* @brief  amsub_outdata_read    get subtitle data from subtitle decode
*/
/* --------------------------------------------------------------------------*/
int amsub_outdata_read(void **priv, amsub_info_t *amsub_info)
{
    int ret = 0;
    ret = aml_sub_read_odata(priv, amsub_info);
    if (ret != 0)
    {
        return -1;
    }
    return 0;
}
