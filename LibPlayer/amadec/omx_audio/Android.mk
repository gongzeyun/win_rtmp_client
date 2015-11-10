LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
        -fPIC -D_POSIX_SOURCE  -DDOLBY_DDPDEC51_MULTICHANNEL_ENDPOINT

LOCAL_C_INCLUDES:= \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../     \
    $(LOCAL_PATH)/../include \
    frameworks/native/include/media/openmax \
    frameworks/av/include/media/stagefright \
    frameworks/native/include/utils

LOCAL_SRC_FILES := \
           adec_omx.cpp audio_mediasource.cpp  
	   #../adec_read.c

LOCAL_MODULE := libamadec_omx_api

LOCAL_ARM_MODE := arm

LOCAL_SHARED_LIBRARIES += libutils libmedia libz libbinder libdl libcutils libc libstagefright \
                          libstagefright_omx  libstagefright_yuv libmedia_native liblog 

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)



