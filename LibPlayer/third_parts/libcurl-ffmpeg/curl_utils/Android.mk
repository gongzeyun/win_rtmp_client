LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := ca-certificates.crt
LOCAL_SRC_FILES := ca-certificates.crt
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/curl/cacerts
include $(BUILD_PREBUILT)