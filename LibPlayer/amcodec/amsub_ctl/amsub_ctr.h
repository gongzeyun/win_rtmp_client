#ifndef CODEC_AMSUB_CTRL_H_
#define CODEC_AMSUB_CTRL_H_

void amsub_start(void **priv, amsub_info_t *amsub_info);
void amsub_stop(void **priv);
int amsub_outdata_read(void **priv, amsub_info_t *amsub_info);

#endif
