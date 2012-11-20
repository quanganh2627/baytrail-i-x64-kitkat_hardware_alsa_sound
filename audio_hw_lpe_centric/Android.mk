ifeq ($(BOARD_USES_ALSA_AUDIO),true)
ifeq ($(BOARD_USES_AUDIO_HAL_LPE_CENTRIC),true)

#AUDIO_POLICY_TEST := true
#ENABLE_AUDIO_DUMP := true
LOCAL_PATH := $(call my-dir)

#PHONY PACKAGE DEFINITION#############################################

include $(CLEAR_VARS)
LOCAL_MODULE := audio_hal_lpe_centric
LOCAL_MODULE_TAGS := optional

LOCAL_REQUIRED_MODULES := \
    audio.primary.$(TARGET_DEVICE) \
    audio_policy.$(TARGET_DEVICE) \
    tinyalsa.$(TARGET_DEVICE)

include $(BUILD_PHONY_PACKAGE)

#######################################################################

include $(CLEAR_VARS)
LOCAL_COPY_HEADERS_TO := alsa-sound
LOCAL_COPY_HEADERS := \
    AudioHardwareALSACommon.h
include $(BUILD_COPY_HEADERS)


include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/parameter \
    $(TARGET_OUT_HEADERS)/hw \
    external/tinyalsa/include \
    external/stlport/stlport/ \
    bionic/libstdc++ \
    bionic/ \
    system/media/audio_utils/include \
    frameworks/av/include/media

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/audio_route_manager \
    $(TARGET_OUT_HEADERS)/at-manager \
    $(TARGET_OUT_HEADERS)/libaudioresample \
    $(TARGET_OUT_HEADERS)/event-listener \
    $(TARGET_OUT_HEADERS)/modem-audio-manager \
    $(TARGET_OUT_HEADERS)/audio-at-manager \
    $(TARGET_OUT_HEADERS)/event-listener \
    $(TARGET_OUT_HEADERS)/property

# for testing with dummy-stmd daemon, comment previous include
# path and uncomment the following one
#LOCAL_C_INCLUDES += \
#        hardware/alsa_sound/test-app/
#

  LOCAL_SRC_FILES := \
    AudioHardwareInterface.cpp \
    audio_hw_hal.cpp \
    SampleSpec.cpp \
    AudioUtils.cpp \
    AudioHardwareALSA.cpp \
    AudioStreamOutALSA.cpp \
    AudioStreamInALSA.cpp \
    AudioAutoRoutingLock.cpp \
    ALSAStreamOps.cpp \
    AudioConversion.cpp \
    AudioConverter.cpp \
    AudioRemapper.cpp \
    AudioReformatter.cpp \
    Resampler.cpp \
    AudioResampler.cpp \
    audio_route_manager/AudioPlatformState.cpp \
    audio_route_manager/AudioRouteManager.cpp \
    audio_route_manager/AudioRoute.cpp \
    audio_route_manager/AudioStreamRoute.cpp \
    audio_route_manager/AudioExternalRoute.cpp \
    audio_route_manager/AudioPlatformHardware_$(TARGET_DEVICE).cpp \
    audio_route_manager/AudioParameterHandler.cpp \
    audio_route_manager/VolumeKeys.cpp

LOCAL_CFLAGS := -D_POSIX_SOURCE

ifeq ($(ENABLE_AUDIO_DUMP),true)
  LOCAL_CFLAGS += -DENABLE_AUDIO_DUMP
  LOCAL_SRC_FILES += AudioDumpInterface.cpp
endif

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

LOCAL_MODULE := audio.primary.$(TARGET_DEVICE)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional
TARGET_ERROR_FLAGS += -Wno-non-virtual-dtor
LOCAL_STATIC_LIBRARIES := libmedia_helper
LOCAL_SHARED_LIBRARIES := \
    libtinyalsa \
    libcutils \
    libutils \
    libmedia \
    libhardware \
    libhardware_legacy \
    libxmlserializer \
    libparameter \
    libstlport \
    libicuuc \
    libmodem-audio-manager \
    libevent-listener \
    libaudioresample \
    libaudioutils \
    libproperty

ifeq ($(BOARD_USES_GTI_FRAMEWORK),true)
LOCAL_C_INCLUDES += \
    hardware/intel/PRIVATE/gti/GtiService \
    hardware/intel/PRIVATE/gti/include \
    hardware/intel/PRIVATE/uta_os/include \
    hardware/intel/PRIVATE/gti/uta_inc

LOCAL_SHARED_LIBRARIES += libgtisrv

LOCAL_CFLAGS += -DUSE_FRAMEWORK_GTI
endif

include $(BUILD_SHARED_LIBRARY)

# This is the ALSA audio policy manager

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/at-manager \
    $(TARGET_OUT_HEADERS)/event-listener \
    $(TARGET_OUT_HEADERS)/libaudioresample \
    $(TARGET_OUT_HEADERS)/property

LOCAL_SRC_FILES := \
    AudioPolicyManagerALSA.cpp

LOCAL_C_INCLUDES += \
    external/stlport/stlport/ \
    bionic/

LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/property

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libstlport \
    libproperty \
    libmedia

LOCAL_STATIC_LIBRARIES := \
    libmedia_helper

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libaudiopolicy_legacy

LOCAL_MODULE := audio_policy.$(TARGET_DEVICE)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

include $(BUILD_SHARED_LIBRARY)

endif
endif
