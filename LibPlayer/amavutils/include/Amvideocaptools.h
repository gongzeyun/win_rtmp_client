#ifndef AMVIDEOCAP_TOOLS_HEAD
#define AMVIDEOCAP_TOOLS_HEAD
#include "Amvideocap.h"
//fmt ignored,always RGB888 now
int amvideocap_capframe(char *buf, int size, int *w, int *h, int fmt_ignored, int at_end, int* ret_size, int fmt);
int amvideocap_capframe_with_rect(char *buf, int size, int src_rect_x, int src_rect_y, int *w, int *h, int fmt_ignored, int at_end, int* ret_size);
#endif

