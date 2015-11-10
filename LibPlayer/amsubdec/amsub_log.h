#ifndef AMSUB_LOG_PRINT_H
#define AMSUB_LOG_PRINT_H

#ifdef ANDROID
#include <android/log.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define  LOG_TAG    "amsub_dec"
#define amsub_print(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#else
#define amsub_print(f,s...) fprintf(stderr,f,##s)
#endif


#endif
