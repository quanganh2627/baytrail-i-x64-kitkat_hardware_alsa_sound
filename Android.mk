#AUDIO_POLICY_TEST := true
#ENABLE_AUDIO_DUMP := true

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/parameter \
    $(TARGET_OUT_HEADERS)/hw \
    external/alsa-lib/include \
    external/stlport/stlport/ \
    bionic/libstdc++ \
    bionic/

LOCAL_SRC_FILES := \
    AudioHardwareInterface.cpp \
    audio_hw_hal.cpp \
    AudioHardwareALSA.cpp \
    AudioStreamOutALSA.cpp \
    AudioStreamInALSA.cpp \
    ALSAStreamOps.cpp \
    ALSAMixer.cpp \
    ALSAControl.cpp \
    AudioRouteManager.cpp \
    AudioRoute.cpp \
    AudioRouteMSICVoice.cpp \
    AudioRouteBT.cpp \
    AudioRouteMM.cpp \
    AudioRouteVoiceRec.cpp

LOCAL_CFLAGS := -D_POSIX_SOURCE

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

LOCAL_MODULE := audio.primary.$(TARGET_DEVICE)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional
TARGET_ERROR_FLAGS += -Wno-non-virtual-dtor
LOCAL_STATIC_LIBRARIES := libmedia_helper
LOCAL_SHARED_LIBRARIES := \
    libasound \
    libcutils \
    libutils \
    libmedia \
    libhardware \
    libhardware_legacy \
    libxmlserializer \
    libparameter \
    libstlport \
    libicuuc

ifeq ($(USE_INTEL_SRC),true)
  LOCAL_CFLAGS += -DUSE_INTEL_SRC
  LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libaudioresample
  LOCAL_SRC_FILES += AudioResamplerALSA.cpp
  LOCAL_SHARED_LIBRARIES += libaudioresample
endif

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
#  LOCAL_SHARED_LIBRARIES += liba2dp
endif

include $(BUILD_SHARED_LIBRARY)
