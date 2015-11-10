#ifndef AMSUB_INTERNAL_H_
#define AMSUB_INTERNAL_H_

#include "amsub_dec.h"


void aml_sub_start(void **amsub_handle, amsub_info_t *amsub_info);

int aml_sub_stop(void *priv);

int aml_sub_release(void **amsub_handle);

int aml_sub_read_odata(void **amsub_handle, amsub_info_t *amsub_info);

#endif
