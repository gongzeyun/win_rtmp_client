LOCAL_PATH := $(call my-dir)

###################################################
## build libsub_dvb

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/ \
                    $(LOCAL_PATH)/amsub_codec \
                    $(LOCAL_PATH)/amsub_codec/libsub_dvb/ \
                    $(LOCAL_PATH)/../amavutils/include


LOCAL_SRC_FILES :=  amsub_codec/libsub_dvb/amsub_dvb_dec.c \
            amsub_codec/amsub_io_ctrl.c \
            amsub_codec/amsub_dec_output.c


LOCAL_MODULE := libsub_dvb
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES += libutils libmedia libz libbinder libdl libcutils libc libamavutils

include $(BUILD_SHARED_LIBRARY)


###################################################

## build libsub_pgs

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/ \
                    $(LOCAL_PATH)/amsub_codec \
                    $(LOCAL_PATH)/amsub_codec/libsub_pgs/ \
                    $(LOCAL_PATH)/../amavutils/include


LOCAL_SRC_FILES :=  amsub_codec/libsub_pgs/amsub_pgs_dec.c \
            amsub_codec/amsub_io_ctrl.c \
            amsub_codec/amsub_dec_output.c


LOCAL_MODULE := libsub_pgs
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES += libutils libmedia libz libbinder libdl libcutils libc libamavutils

include $(BUILD_SHARED_LIBRARY)


###################################################

## build libsub_vob

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/ \
                    $(LOCAL_PATH)/amsub_codec \
                    $(LOCAL_PATH)/amsub_codec/libsub_vob/ \
                    $(LOCAL_PATH)/../amavutils/include


LOCAL_SRC_FILES :=  amsub_codec/libsub_vob/amsub_vob_dec.c \
            amsub_codec/amsub_io_ctrl.c \
            amsub_codec/amsub_dec_output.c


LOCAL_MODULE := libsub_vob
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES += libutils libmedia libz libbinder libdl libcutils libc libamavutils

include $(BUILD_SHARED_LIBRARY)


###################################################

## build libsub_ass

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/ \
                    $(LOCAL_PATH)/amsub_codec \
                    $(LOCAL_PATH)/amsub_codec/libsub_ass/ \
                    $(LOCAL_PATH)/../amavutils/include


LOCAL_SRC_FILES :=  amsub_codec/libsub_ass/amsub_ass_dec.c \
            amsub_codec/amsub_io_ctrl.c \
            amsub_codec/amsub_dec_output.c


LOCAL_MODULE := libsub_ass
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES += libutils libmedia libz libbinder libdl libcutils libc libamavutils

include $(BUILD_SHARED_LIBRARY)


###################################################

## build libsub_xsub

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/ \
                    $(LOCAL_PATH)/amsub_codec \
                    $(LOCAL_PATH)/amsub_codec/libsub_avi/ \
                    $(LOCAL_PATH)/../amavutils/include


LOCAL_SRC_FILES :=  amsub_codec/libsub_avi/amsub_avi_dec.c \
            amsub_codec/amsub_io_ctrl.c \
            amsub_codec/amsub_dec_output.c


LOCAL_MODULE := libsub_xsub
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES += libutils libmedia libz libbinder libdl libcutils libc libamavutils

include $(BUILD_SHARED_LIBRARY)

###################################################

## build libsub_text

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/ \
                    $(LOCAL_PATH)/amsub_codec \
                    $(LOCAL_PATH)/amsub_codec/libsub_text/ \
                    $(LOCAL_PATH)/../amavutils/include


LOCAL_SRC_FILES :=  amsub_codec/libsub_text/amsub_text_dec.c \
            amsub_codec/amsub_io_ctrl.c \
            amsub_codec/amsub_dec_output.c


LOCAL_MODULE := libsub_text
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES += libutils libmedia libz libbinder libdl libcutils libc libamavutils

include $(BUILD_SHARED_LIBRARY)

###################################################

## build libsub_idxsub

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/ \
                    $(LOCAL_PATH)/amsub_codec \
                    $(LOCAL_PATH)/amsub_codec/libsub_idxsub/ \
                    $(LOCAL_PATH)/../amavutils/include


LOCAL_SRC_FILES :=  amsub_codec/libsub_idxsub/amsub_idxsub_dec.c \
            amsub_codec/amsub_io_ctrl.c \
            amsub_codec/amsub_dec_output.c


LOCAL_MODULE := libsub_idxsub
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES += libutils libmedia libz libbinder libdl libcutils libc libamavutils

include $(BUILD_SHARED_LIBRARY)

###################################################

## build libamsubdec

include $(CLEAR_VARS)

LOCAL_SRC_FILES := amsub_dec.c \
                   amsub_internal_ctrl.c

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
            $(LOCAL_PATH)/amsub_codec/ \
            $(LOCAL_PATH)/../amavutils/include



LOCAL_SHARED_LIBRARIES += libutils libmedia libz libbinder libdl libcutils libc libamavutils libsub_dvb libsub_pgs libsub_vob libsub_ass libsub_xsub libsub_text libsub_idxsub

LOCAL_ARM_MODE := arm
LOCAL_MODULE:= libamsubdec
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

###################################################


