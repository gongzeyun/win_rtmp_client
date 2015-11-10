#ifndef AMSUB_VOB_SUB_H
#define AMSUB_VOB_SUB_H

#include "amsub_dec.h"

int get_vob_spu(char *input_buf, int *buf_size, unsigned length, amsub_para_s *spu);
void vobsub_init_decoder(void);

typedef enum
{
    FSTA_DSP = 0,
    STA_DSP = 1,
    STP_DSP  = 2,
    SET_COLOR = 3,
    SET_CONTR = 4,
    SET_DAREA = 5,
    SET_DSPXA = 6,
    CHG_COLCON = 7,
    CMD_END = 0xFF,
} CommandID;

#endif
