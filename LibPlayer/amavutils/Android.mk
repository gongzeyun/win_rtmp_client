LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
#LOCAL_CFLAGS+=-DNO_USE_SYSWRITE

ifneq ($(NUM_FRAMEBUFFER_SURFACE_BUFFERS),)
  LOCAL_CFLAGS += -DFB_BUFFER_NUM=$(NUM_FRAMEBUFFER_SURFACE_BUFFERS)
endif

ifeq ($(TARGET_EXTERNAL_DISPLAY),true)
ifeq ($(TARGET_SINGLE_EXTERNAL_DISPLAY_USE_FB1),true)
LOCAL_CFLAGS += -DSINGLE_EXTERNAL_DISPLAY_USE_FB1
endif
endif

ifeq ($(TARGET_EXTERNAL_DISPLAY),true)
ifeq ($(TARGET_SINGLE_EXTERNAL_DISPLAY_USE_FB1),true)
LOCAL_CFLAGS += -DSINGLE_EXTERNAL_DISPLAY_USE_FB1
endif
endif

LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 	
LOCAL_SRC_FILES += $(notdir $(wildcard $(LOCAL_PATH)/*.cpp)) 

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
     $(LOCAL_PATH)/../amcodec/include \
	 $(JNI_H_INCLUDE) \
	 $(TOP)/frameworks/native/services \
	 $(TOP)/frameworks/native/include \
	 $(TOP)/vendor/amlogic/frameworks/services \

LOCAL_CFLAGS += -DANDROID_PLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_SHARED_LIBRARIES += libutils
#LOCAL_SHARED_LIBRARIES += libandroid_runtime   libnativehelper
LOCAL_SHARED_LIBRARIES +=  libcutils libc libdl
LOCAL_SHARED_LIBRARIES += libbinder
ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_SHARED_LIBRARIES += libsystemcontrolservice
else
LOCAL_SHARED_LIBRARIES += libsystemwriteservice
endif
LOCAL_SHARED_LIBRARIES += libui libgui 
                          
LOCAL_MODULE := libamavutils
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

#LOCAL_CFLAGS+=-DNO_USE_SYSWRITE

ifneq ($(NUM_FRAMEBUFFER_SURFACE_BUFFERS),)
  LOCAL_CFLAGS += -DFB_BUFFER_NUM=$(NUM_FRAMEBUFFER_SURFACE_BUFFERS)
endif

ifeq ($(TARGET_EXTERNAL_DISPLAY),true)
ifeq ($(TARGET_SINGLE_EXTERNAL_DISPLAY_USE_FB1),true)
LOCAL_CFLAGS += -DSINGLE_EXTERNAL_DISPLAY_USE_FB1
endif
endif

LOCAL_CFLAGS += -DANDROID_PLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 	
LOCAL_SRC_FILES += $(notdir $(wildcard $(LOCAL_PATH)/*.cpp))

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
         $(LOCAL_PATH)/../amcodec/include \
         $(JNI_H_INCLUDE) \
         $(TOP)/frameworks/native/services \
         $(TOP)/frameworks/native/include \
         $(TOP)/vendor/amlogic/frameworks/services \

LOCAL_SHARED_LIBRARIES += libutils
#LOCAL_SHARED_LIBRARIES += libandroid_runtime   libnativehelper
LOCAL_SHARED_LIBRARIES +=  libcutils libc
LOCAL_SHARED_LIBRARIES += libui libgui 
LOCAL_SHARED_LIBRARIES += libbinder 
ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_SHARED_LIBRARIES += libsystemcontrolservice
else
LOCAL_SHARED_LIBRARIES += libsystemwriteservice
endif
LOCAL_MODULE := libamavutils
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)

LOCAL_CFLAGS+=-DNO_USE_SYSWRITE

ifneq ($(NUM_FRAMEBUFFER_SURFACE_BUFFERS),)
  LOCAL_CFLAGS += -DFB_BUFFER_NUM=$(NUM_FRAMEBUFFER_SURFACE_BUFFERS)
endif

ifeq ($(TARGET_EXTERNAL_DISPLAY),true)
ifeq ($(TARGET_SINGLE_EXTERNAL_DISPLAY_USE_FB1),true)
LOCAL_CFLAGS += -DSINGLE_EXTERNAL_DISPLAY_USE_FB1
endif
endif

LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 	
LOCAL_SRC_FILES += $(notdir $(wildcard $(LOCAL_PATH)/*.cpp)) 

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
     $(LOCAL_PATH)/../amcodec/include \
	 $(JNI_H_INCLUDE) \
	 $(TOP)/frameworks/native/services \
	 $(TOP)/frameworks/native/include \
	 $(TOP)/vendor/amlogic/frameworks/services \

LOCAL_SHARED_LIBRARIES += libutils
#LOCAL_SHARED_LIBRARIES += libandroid_runtime   libnativehelper
LOCAL_SHARED_LIBRARIES +=  libcutils libc libdl
LOCAL_SHARED_LIBRARIES += libbinder 
ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_SHARED_LIBRARIES += libsystemcontrolservice
else
LOCAL_SHARED_LIBRARIES += libsystemwriteservice
endif

LOCAL_SHARED_LIBRARIES += libui libgui 


LOCAL_MODULE := libamavutils_alsa
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)



include $(CLEAR_VARS)

LOCAL_CFLAGS+=-DNO_USE_SYSWRITE

ifneq ($(NUM_FRAMEBUFFER_SURFACE_BUFFERS),)
  LOCAL_CFLAGS += -DFB_BUFFER_NUM=$(NUM_FRAMEBUFFER_SURFACE_BUFFERS)
endif

ifeq ($(TARGET_EXTERNAL_DISPLAY),true)
ifeq ($(TARGET_SINGLE_EXTERNAL_DISPLAY_USE_FB1),true)
LOCAL_CFLAGS += -DSINGLE_EXTERNAL_DISPLAY_USE_FB1
endif
endif

LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 	
LOCAL_SRC_FILES += $(notdir $(wildcard $(LOCAL_PATH)/*.cpp))

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
         $(call include-path-for, graphics corecg)    \
         $(LOCAL_PATH)/../amcodec/include \
         $(JNI_H_INCLUDE) \
         $(TOP)/frameworks/native/services \
         $(TOP)/frameworks/native/include \
         $(TOP)/vendor/amlogic/frameworks/services \

LOCAL_SHARED_LIBRARIES += libutils
#LOCAL_SHARED_LIBRARIES += libandroid_runtime   libnativehelper
LOCAL_SHARED_LIBRARIES +=  libcutils libc
LOCAL_SHARED_LIBRARIES += libbinder 
LOCAL_SHARED_LIBRARIES += libui libgui 

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_SHARED_LIBRARIES += libsystemcontrolservice
else
LOCAL_SHARED_LIBRARIES += libsystemwriteservice
endif

LOCAL_MODULE := libamavutils_alsa
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
include $(BUILD_STATIC_LIBRARY)
