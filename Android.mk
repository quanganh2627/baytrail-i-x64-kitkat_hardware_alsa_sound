ifeq ($(BOARD_USES_ALSA_AUDIO),true)

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
    bionic/ \
    system/media/audio_utils/include \
    system/media/audio_effects/include

LOCAL_C_INCLUDES += \
    hardware/intel/IFX-modem \
    $(TARGET_OUT_HEADERS)/at-manager \
    $(TARGET_OUT_HEADERS)/libaudioresample \
    $(TARGET_OUT_HEADERS)/event-listener \
    $(TARGET_OUT_HEADERS)/modem-audio-manager \
    $(TARGET_OUT_HEADERS)/audio-at-manager

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
    ALSAStreamOps.cpp \
    AudioConversion.cpp \
    AudioConverter.cpp \
    AudioRemapper.cpp \
    AudioReformatter.cpp \
    AudioResampler.cpp \
    ALSAMixer.cpp \
    ALSAControl.cpp \
    AudioRouteManager.cpp \
    AudioRoute.cpp \
    AudioRouteMSICVoice.cpp \
    AudioRouteBT.cpp \
    AudioRouteMM.cpp \
    AudioRouteVoiceRec.cpp

LOCAL_CFLAGS := -D_POSIX_SOURCE

ifeq ($(BOARD_HAVE_AUDIENCE),true)
    LOCAL_CFLAGS += -DCUSTOM_BOARD_WITH_AUDIENCE
endif

ifneq ($(BOARD_HAVE_MODEM),true)
    LOCAL_CFLAGS += -DCUSTOM_BOARD_WITHOUT_MODEM
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
    libicuuc \
    libat-manager \
    libaudioresample \
    libaudioutils \
    libmodem-audio-manager

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
#  LOCAL_SHARED_LIBRARIES += liba2dp
endif

include $(BUILD_SHARED_LIBRARY)

# This is the ALSA audio policy manager

include $(CLEAR_VARS)

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
