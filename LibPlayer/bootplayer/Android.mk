LOCAL_PATH := $(call my-dir)

ifeq ($(BUILD_WITH_BOOT_PLAYER),true)

include $(CLEAR_VARS)
LOCAL_MODULE    := bootplayer
LOCAL_MODULE_TAGS := samples
LOCAL_SRC_FILES := bootplayer.c
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../amplayer/player/include \
    $(LOCAL_PATH)/../amcodec/include \
    $(LOCAL_PATH)/../amadec/include \
    $(LOCAL_PATH)/../amffmpeg \
    $(JNI_H_INCLUDE) \
    $(LOCAL_PATH)/../streamsource \
    $(LOCAL_PATH)/../amavutils/include \

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_STATIC_LIBRARIES := libamplayer libamplayer libamcodec libavformat librtmp libavcodec libavutil libamadec_alsa libamstreaming libiconv
LOCAL_SHARED_LIBRARIES += libutils libmedia libbinder libz libdl libcutils libssl libcrypto libasound libamavutils libamsubdec
else
LOCAL_STATIC_LIBRARIES := libamplayer libamplayer libamcodec libavformat librtmp libavcodec libavutil libamadec_alsa libamavutils_alsa libamstreaming libiconv
LOCAL_SHARED_LIBRARIES += libutils libmedia libbinder libz libdl libcutils libssl libcrypto libasound libamsubdec
endif

include $(BUILD_EXECUTABLE)

endif

