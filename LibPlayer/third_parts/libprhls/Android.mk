LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false


LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 		

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include \
        $(LOCAL_PATH)/../../amavutils/include \
	$(LOCAL_PATH)/../../amutils/include \
        $(LOCAL_PATH)/../../amffmpeg

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -Wno-multichar


LOCAL_SHARED_LIBRARIES :=libamplayer libcutils libssl libamavutils




LOCAL_SHARED_LIBRARIES +=libdl 


LOCAL_MODULE := libprhls_mod
LOCAL_MODULE_PATH:=$(TARGET_OUT_SHARED_LIBRARIES)/amplayer
include $(BUILD_SHARED_LIBRARY)
