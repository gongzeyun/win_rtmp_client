
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifneq ($(BOARD_VOUT_USES_FREESCALE),false)
LOCAL_CFLAGS += -DENABLE_FREE_SCALE
endif

ifdef DOLBY_DAP
LOCAL_CFLAGS += -DDOLBY_DAP_EN
endif

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 4.3))
    ifneq ($(TARGET_BOARD_PLATFORM),meson6)
        LOCAL_CFLAGS +=   -DUSE_ARM_AUDIO_DEC
    endif 
endif

LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 												

LOCAL_SRC_FILES +=system/android.c system/systemsetting.c

$(shell cd $(LOCAL_PATH);touch version.c)
LIBPLAYER_GIT_VERSION="$(shell cd $(LOCAL_PATH);git log | grep commit -m 1 | cut -d' ' -f 2)"
LIBPLAYER_GIT_UNCOMMIT_FILE_NUM=$(shell cd $(LOCAL_PATH);git diff | grep +++ -c)
LIBPLAYER_LAST_CHANGED="$(shell cd $(LOCAL_PATH);git log | grep Date -m 1)"
LIBPLAYER_BUILD_TIME=" $(shell date)"
LIBPLAYER_BUILD_NAME=" $(shell echo ${LOGNAME})"

LOCAL_CFLAGS+=-DHAVE_VERSION_INFO
LOCAL_CFLAGS+=-DLIBPLAYER_GIT_VERSION=\"${LIBPLAYER_GIT_VERSION}${LIBPLAYER_GIT_DIRTY}\"
LOCAL_CFLAGS+=-DLIBPLAYER_LAST_CHANGED=\"${LIBPLAYER_LAST_CHANGED}\"
LOCAL_CFLAGS+=-DLIBPLAYER_BUILD_TIME=\"${LIBPLAYER_BUILD_TIME}\"
LOCAL_CFLAGS+=-DLIBPLAYER_BUILD_NAME=\"${LIBPLAYER_BUILD_NAME}\"
LOCAL_CFLAGS+=-DLIBPLAYER_GIT_UNCOMMIT_FILE_NUM=${LIBPLAYER_GIT_UNCOMMIT_FILE_NUM}

LOCAL_CFLAGS += -DANDROID_PLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

FFCFLAGS += -fPIC
ifeq ($(TARGET_ARCH),arm)
LOCAL_LDFLAGS := -Wl,--no-warn-shared-textrel
endif
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../../amcodec/include \
	$(LOCAL_PATH)/../common\
	$(LOCAL_PATH)/../../amadec/include \
	$(LOCAL_PATH)/../../amffmpeg\
	$(LOCAL_PATH)/../../amavutils/include \
	$(LOCAL_PATH)/../../third_parts/libiconv-1.12/include

LOCAL_MODULE := libamplayer

LOCAL_ARM_MODE := arm

include $(BUILD_STATIC_LIBRARY)

####################################################################33
include $(CLEAR_VARS)

ifneq ($(BOARD_VOUT_USES_FREESCALE),false)
LOCAL_CFLAGS += -DENABLE_FREE_SCALE
endif

ifdef DOLBY_DAP
LOCAL_CFLAGS += -DDOLBY_DAP_EN
endif

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 4.3))
    ifneq ($(TARGET_BOARD_PLATFORM),meson6)
        LOCAL_CFLAGS +=   -DUSE_ARM_AUDIO_DEC
    endif 
endif



LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 									

LOCAL_SRC_FILES +=system/android.c system/systemsetting.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
        $(LOCAL_PATH)/../../amcodec/include \
        $(LOCAL_PATH)/../../amadec/include \
	$(LOCAL_PATH)/../common\
        $(LOCAL_PATH)/../../amffmpeg\
        $(LOCAL_PATH)/../../amavutils/include \
		$(LOCAL_PATH)/../../third_parts/libiconv-1.12/include

LOCAL_CFLAGS+=-DHAVE_VERSION_INFO
LOCAL_CFLAGS+=-DLIBPLAYER_GIT_VERSION=\"${LIBPLAYER_GIT_VERSION}${LIBPLAYER_GIT_DIRTY}\"
LOCAL_CFLAGS+=-DLIBPLAYER_LAST_CHANGED=\"${LIBPLAYER_LAST_CHANGED}\"
LOCAL_CFLAGS+=-DLIBPLAYER_BUILD_TIME=\"${LIBPLAYER_BUILD_TIME}\"
LOCAL_CFLAGS+=-DLIBPLAYER_BUILD_NAME=\"${LIBPLAYER_BUILD_NAME}\"
LOCAL_CFLAGS+=-DLIBPLAYER_GIT_UNCOMMIT_FILE_NUM=${LIBPLAYER_GIT_UNCOMMIT_FILE_NUM}

LOCAL_CFLAGS += -DANDROID_PLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

FFCFLAGS += -fPIC 
ifeq ($(TARGET_ARCH),arm)
LOCAL_LDFLAGS := -Wl,--no-warn-shared-textrel
endif
LOCAL_STATIC_LIBRARIES := libamcodec libavformat librtmp libswscale libavcodec libavutil libamadec libiconv
LOCAL_SHARED_LIBRARIES += libutils libmedia libz libbinder libdl libcutils libc libamavutils libssl libcrypto libamsubdec

LOCAL_MODULE := libamplayer
LOCAL_MODULE_TAGS := optional

LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

##$(shell cd $(LOCAL_PATH);rm ${VERSION_FILE_NAME})

