LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := kplayer
LOCAL_MODULE_TAGS := samples
LOCAL_SRC_FILES := kplayer.c
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../amplayer/player/include \
    $(LOCAL_PATH)/../amcodec/include \
    $(LOCAL_PATH)/../amadec/include \
    $(LOCAL_PATH)/../amffmpeg \
    $(JNI_H_INCLUDE) \
    $(LOCAL_PATH)/../streamsource \

LOCAL_STATIC_LIBRARIES := libamplayer libamplayer libamcodec libavformat librtmp libavcodec libavutil libamadec libamavutils libamstreaming libiconv
LOCAL_SHARED_LIBRARIES += libutils libmedia libbinder libz libdl libcutils libssl libcrypto libamsubdec

LOCAL_SHARED_LIBRARIES += libbinder 

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_SHARED_LIBRARIES += libsystemcontrolservice
else
LOCAL_SHARED_LIBRARIES += libsystemwriteservice
endif


include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE    := simple_player
LOCAL_MODULE_TAGS := samples
LOCAL_SRC_FILES := simple_player.c
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../amplayer/player/include \
    $(LOCAL_PATH)/../amcodec/include \
    $(LOCAL_PATH)/../amadec/include \
    $(LOCAL_PATH)/../amffmpeg \
    $(JNI_H_INCLUDE) 

LOCAL_STATIC_LIBRARIES := libamplayer libamplayer libamcodec libavformat librtmp libavcodec libavutil libamadec libamavutils libiconv
LOCAL_SHARED_LIBRARIES += libutils libmedia libbinder libz libdl libcutils libssl libcrypto libamsubdec

LOCAL_SHARED_LIBRARIES += libbinder 


ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_SHARED_LIBRARIES += libsystemcontrolservice
else
LOCAL_SHARED_LIBRARIES += libsystemwriteservice
endif

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE    := esplayer
LOCAL_MODULE_TAGS := samples
LOCAL_SRC_FILES := esplayer.c
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../amplayer/player/include \
    $(LOCAL_PATH)/../amcodec/include \
    $(LOCAL_PATH)/../amadec/include \
    $(LOCAL_PATH)/../amffmpeg \
    $(JNI_H_INCLUDE)

LOCAL_STATIC_LIBRARIES := libamplayer libamplayer libamcodec libavformat librtmp libavcodec libavutil libamadec libamavutils
LOCAL_SHARED_LIBRARIES += libutils libmedia libbinder libz libdl libcutils libssl libcrypto libamsubdec

LOCAL_SHARED_LIBRARIES += libbinder

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_SHARED_LIBRARIES += libsystemcontrolservice
else
LOCAL_SHARED_LIBRARIES += libsystemwriteservice
endif

include $(BUILD_EXECUTABLE)

ifeq ($(BUILD_AMVIDEO_CAPTURE_TEST),true)
include $(CLEAR_VARS)
LOCAL_MODULE    := amvideocaptest
LOCAL_MODULE_TAGS := samples
LOCAL_SRC_FILES := amvideocaptest.c
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../amplayer/player/include \
    $(LOCAL_PATH)/../amavutils/include \
    $(JNI_H_INCLUDE)

LOCAL_STATIC_LIBRARIES := libavutil libamadec libamavutils
LOCAL_SHARED_LIBRARIES += libutils libmedia libbinder libz libdl libcutils libssl libcrypto libamsubdec

LOCAL_SHARED_LIBRARIES += libbinder 

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_SHARED_LIBRARIES += libsystemcontrolservice
else
LOCAL_SHARED_LIBRARIES += libsystemwriteservice
endif

include $(BUILD_EXECUTABLE)
endif
