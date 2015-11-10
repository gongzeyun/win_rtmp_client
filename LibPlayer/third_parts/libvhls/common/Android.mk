LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional

ifeq ($(LIVEPLAY_SEEK), true)
 LOCAL_CFLAGS += -DLIVEPLAY_SEEK
endif

LIBPLAYER_PATH=$(LOCAL_PATH)/../../../
LOCAL_SRC_FILES := \
	hls_utils.c\
	hls_rand.c
	
LOCAL_C_INCLUDES := \
    $(LIBPLAYER_PATH)/amavutils/include \
    $(TOP)/external/openssl/include\
    $(LOCAL_PATH)./

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

LOCAL_MODULE := libhls_common

include $(BUILD_STATIC_LIBRARY)
