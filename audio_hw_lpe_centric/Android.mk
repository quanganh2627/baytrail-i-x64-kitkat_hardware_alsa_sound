ifeq ($(BOARD_USES_ALSA_AUDIO),true)

#AUDIO_POLICY_TEST := true
#ENABLE_AUDIO_DUMP := true

LOCAL_PATH := $(call my-dir)
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
    audio_route_manager/AudioRouteFactory.cpp \
    audio_route_manager/AudioStreamRoute.cpp \
    audio_route_manager/AudioStreamRouteMedia.cpp \
    audio_route_manager/AudioStreamRouteVoice.cpp \
    audio_route_manager/AudioExternalRoute.cpp \
    audio_route_manager/AudioExternalRouteModemIA.cpp \
    audio_route_manager/AudioExternalRouteBtIA.cpp \
    audio_route_manager/AudioExternalRouteFMIA.cpp \
    audio_route_manager/AudioExternalRouteHwCodec0IA.cpp \
    audio_route_manager/AudioExternalRouteHwCodec1IA.cpp \
    audio_route_manager/VolumeKeys.cpp

LOCAL_CFLAGS := -D_POSIX_SOURCE

ifeq ($(BOARD_HAVE_AUDIENCE),true)
    LOCAL_CFLAGS += -DCUSTOM_BOARD_WITH_AUDIENCE
endif

ifeq ($(ENABLE_AUDIO_DUMP),true)
  LOCAL_CFLAGS += -DENABLE_AUDIO_DUMP
  LOCAL_SRC_FILES += AudioDumpInterface.cpp
endif

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

ifeq ($(BUILD_FM_RADIO),true)
  LOCAL_CFLAGS += -DWITH_FM_SUPPORT
endif

ifeq ($(FM_RADIO_RX_ANALOG),true)
  LOCAL_CFLAGS += -DFM_RX_ANALOG
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

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
#  LOCAL_SHARED_LIBRARIES += liba2dp
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
