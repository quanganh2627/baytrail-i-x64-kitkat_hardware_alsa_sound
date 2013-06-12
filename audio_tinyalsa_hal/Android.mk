ifeq ($(BOARD_USES_ALSA_AUDIO),true)
ifeq ($(BOARD_USES_AUDIO_HAL_CONFIGURABLE),false)

#ENABLE_AUDIO_DUMP := true
LOCAL_PATH := $(call my-dir)

#######################################################################
# Common variables

audio_tinyalsa_configurable_src_files :=  \
    audio_hw.c \
    audio_route.c


audio_tinyalsa_configurable_includes_dir := \
    $(LOCAL_PATH) \
    frameworks/av/include/media \
    external/expat/lib \
    external/tinyalsa/include \
    system/media/audio_utils/include \
    system/media/audio_effects/include

audio_tinyalsa_configurable_includes_dir_host := \
    $(audio_tinyalsa_configurable_includes_dir) \
    $(HOST_OUT_HEADERS)/property

audio_tinyalsa_configurable_includes_dir_target := \
    $(audio_tinyalsa_configurable_includes_dir) \
    $(TARGET_OUT_HEADERS)/property \
    external/stlport/stlport \
    external/expat/lib \
    bionic

audio_tinyalsa_configurable_header_files :=  \
    audio_route.h


audio_tinyalsa_configurable_cflags := -Werror

#######################################################################
# Phony package definition

include $(CLEAR_VARS)
LOCAL_MODULE := audio_hal_configurable
LOCAL_MODULE_TAGS := optional

LOCAL_REQUIRED_MODULES := \
    audio.primary.$(TARGET_DEVICE) \
    audio_policy.$(TARGET_DEVICE) \
    libaudiohalutils

include $(BUILD_PHONY_PACKAGE)

#######################################################################
# Build for target audio.primary

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
    $(audio_tinyalsa_configurable_includes_dir) \
    $(TARGET_OUT_HEADERS)/property \
    external/stlport/stlport \
    bionic


LOCAL_SRC_FILES := $(audio_tinyalsa_configurable_src_files)
LOCAL_CFLAGS := $(audio_tinyalsa_configurable_cflags)


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
    libstlport \
    libexpat \
    libaudioutils

include $(BUILD_SHARED_LIBRARY)

endif
endif
