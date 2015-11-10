#ifndef IONVDEC_PRIV_HEADER__
#define IONVDEC_PRIV_HEADER__

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <videodev2.h>
#include <android/log.h>

#define  TAG        "ionvdec"
#define  XLOG(V,T,...)  __android_log_print(V,T,__VA_ARGS__)
#define  LOGI(...)   XLOG(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define  LOGE(...)   XLOG(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

#endif
