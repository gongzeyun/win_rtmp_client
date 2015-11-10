LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE := libdash_mod.so
LOCAL_MODULE_TAGS := optional
LOCAL_IS_HOST_MODULE := true

LOCAL_MODULE_VERSION := $(shell expr substr "$(PLATFORM_VERSION)" 1 3)
ifneq ($(wildcard $(LOCAL_PATH)/libdash_mod_$(LOCAL_MODULE_VERSION).so),)
LOCAL_SRC_FILES := libdash_mod_$(LOCAL_MODULE_VERSION).so
else
LOCAL_SRC_FILES := libdash_mod.so
endif
#LOCAL_SRC_FILES := $(wildcard libdash_mod_$(LOCAL_MODULE_VERSION).so )
LOCAL_MODULE_PATH:=$(TARGET_OUT)/lib/amplayer

include $(BUILD_PREBUILT) 
