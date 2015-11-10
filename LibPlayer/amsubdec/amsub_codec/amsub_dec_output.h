#ifndef AMSUB_DEC_OUTPUT_H_
#define AMSUB_DEC_OUTPUT_H_

#include "amsub_dec.h"

#define text_sub_num 60
#define bitmap_sub_num 30
int amsub_dec_out_init(amsub_para_t *amsub_para);

int amsub_dec_out_add(amsub_para_t *amsub_para);

int amsub_dec_out_get(amsub_para_t *amsub_para, amsub_info_t *amsub_info);

int amsub_dec_out_close(amsub_para_t *amsub_para);

int get_amsub_size(amsub_para_s *amsub_p);

int amsub_dec_out_ctrl();

#endif
