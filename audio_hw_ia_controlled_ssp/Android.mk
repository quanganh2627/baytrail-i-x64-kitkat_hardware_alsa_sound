ifeq ($(BOARD_USES_ALSA_AUDIO),true)
ifeq ($(BOARD_USES_AUDIO_HAL_IA_CONTROLLED_SSP),true)

#ENABLE_AUDIO_DUMP := true
LOCAL_PATH := $(call my-dir)

#PHONY PACKAGE DEFINITION#############################################

include $(CLEAR_VARS)
LOCAL_MODULE := audio_hal_ia_controlled_ssp
LOCAL_MODULE_TAGS := optional

LOCAL_REQUIRED_MODULES := \
    audio.primary.$(TARGET_DEVICE) \
    audio_policy.$(TARGET_DEVICE) \
    libaudiohalutils

include $(BUILD_PHONY_PACKAGE)

#######################################################################

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
    $(TARGET_OUT_HEADERS)/libaudioresample \
    $(TARGET_OUT_HEADERS)/event-listener \
    $(TARGET_OUT_HEADERS)/mamgr-interface \
    $(TARGET_OUT_HEADERS)/mamgr-core \
    $(TARGET_OUT_HEADERS)/interface-provider \
    $(TARGET_OUT_HEADERS)/interface-provider-lib \
    $(TARGET_OUT_HEADERS)/property \
    $(TARGET_OUT_HEADERS)/audiocomms-include \
    $(TARGET_OUT_HEADERS)/audio_hal_utils

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
    audio_route_manager/AudioPort.cpp \
    audio_route_manager/AudioPortGroup.cpp \
    audio_route_manager/AudioStreamRoute.cpp \
    audio_route_manager/AudioExternalRoute.cpp \
    audio_route_manager/AudioCompressedStreamRoute.cpp \
    audio_route_manager/AudioPlatformHardware_$(TARGET_DEVICE).cpp \
    audio_route_manager/AudioParameterHandler.cpp \
    audio_route_manager/VolumeKeys.cpp

LOCAL_CFLAGS := -D_POSIX_SOURCE -Wall -Werror

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
    libevent-listener \
    libaudioresample \
    libaudioutils \
    libproperty \
    libaudiohalutils \
    libinterface-provider-lib

# gcov build
ifeq ($($(LOCAL_MODULE).gcov),true)
  LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/gcov_flush_with_prop
  LOCAL_CFLAGS += -O0 -fprofile-arcs -ftest-coverage -include GcovFlushWithProp.h
  LOCAL_LDFLAGS += -fprofile-arcs -lgcov
  LOCAL_STATIC_LIBRARIES += gcov_flush_with_prop
endif

include $(BUILD_SHARED_LIBRARY)

endif
endif
