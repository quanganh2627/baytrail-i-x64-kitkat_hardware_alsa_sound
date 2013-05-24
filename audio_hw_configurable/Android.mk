ifeq ($(BOARD_USES_ALSA_AUDIO),true)
ifeq ($(BOARD_USES_AUDIO_HAL_CONFIGURABLE),true)

#ENABLE_AUDIO_DUMP := true
LOCAL_PATH := $(call my-dir)

#######################################################################
# Common variables

audio_hw_configurable_src_files :=  \
    ALSAStreamOps.cpp \
    audio_hw_hal.cpp \
    AudioAutoRoutingLock.cpp \
    AudioConversion.cpp \
    AudioConverter.cpp \
    AudioHardwareALSA.cpp \
    AudioHardwareInterface.cpp \
    AudioReformatter.cpp \
    AudioRemapper.cpp \
    AudioResampler.cpp \
    AudioStreamInALSA.cpp \
    AudioStreamOutALSA.cpp \
    AudioUtils.cpp \
    Resampler.cpp \
    SampleSpec.cpp

audio_hw_configurable_src_files +=  \
    audio_route_manager/AudioCompressedStreamRoute.cpp \
    audio_route_manager/AudioExternalRoute.cpp \
    audio_route_manager/AudioParameterHandler.cpp \
    audio_route_manager/AudioPlatformHardware_$(REF_PRODUCT_NAME).cpp \
    audio_route_manager/AudioPlatformState.cpp \
    audio_route_manager/AudioPort.cpp \
    audio_route_manager/AudioPortGroup.cpp \
    audio_route_manager/AudioRoute.cpp \
    audio_route_manager/AudioRouteManager.cpp \
    audio_route_manager/AudioStreamRoute.cpp \
    audio_route_manager/VolumeKeys.cpp

audio_hw_configurable_includes_dir := \
    $(LOCAL_PATH)/audio_route_manager \
    $(TARGET_OUT_HEADERS)/libaudioresample \
    $(TARGET_OUT_HEADERS)/event-listener \
    $(TARGET_OUT_HEADERS)/mamgr-interface \
    $(TARGET_OUT_HEADERS)/mamgr-core \
    $(TARGET_OUT_HEADERS)/interface-provider \
    $(TARGET_OUT_HEADERS)/interface-provider-lib \
    $(TARGET_OUT_HEADERS)/audiocomms-include \
    $(TARGET_OUT_HEADERS)/audio_hal_utils \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    frameworks/av/include/media \
    external/tinyalsa/include \
    system/media/audio_utils/include \
    system/media/audio_effects/include

audio_hw_configurable_includes_dir_host := \
    $(audio_hw_configurable_includes_dir) \
    $(HOST_OUT_HEADERS)/property

audio_hw_configurable_includes_dir_target := \
    $(audio_hw_configurable_includes_dir) \
    $(TARGET_OUT_HEADERS)/property \
    external/stlport/stlport \
    bionic

audio_hw_configurable_header_files :=  \
    ALSAStreamOps.h \
    AudioAutoRoutingLock.h \
    AudioConversion.h \
    AudioConverter.h \
    AudioDumpInterface.h \
    AudioHardwareALSA.h \
    AudioReformatter.h \
    AudioRemapper.h \
    AudioResampler.h \
    audio_route_manager/AudioCompressedStreamRoute.h \
    audio_route_manager/AudioExternalRoute.h \
    audio_route_manager/AudioParameterHandler.h \
    audio_route_manager/AudioPlatformHardware.h \
    audio_route_manager/AudioPlatformState.h \
    audio_route_manager/AudioPortGroup.h \
    audio_route_manager/AudioPort.h \
    audio_route_manager/AudioRoute.h \
    audio_route_manager/AudioRouteManager.h \
    audio_route_manager/AudioStreamRoute.h \
    audio_route_manager/VolumeKeys.h \
    AudioStreamInALSA.h \
    AudioStreamOutALSA.h \
    AudioUtils.h \
    Resampler.h \
    SampleSpec.h

audio_hw_configurable_header_copy_folder_unit_test := \
    audio_hw_configurable_unit_test

audio_hw_configurable_cflags := -Wall -Werror

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
    $(audio_hw_configurable_includes_dir) \
    $(TARGET_OUT_HEADERS)/property \
    external/stlport/stlport \
    bionic

# for testing with dummy-stmd daemon, comment previous include
# path and uncomment the following one
#LOCAL_C_INCLUDES += \
#        hardware/alsa_sound/test-app/
#

LOCAL_SRC_FILES := $(audio_hw_configurable_src_files)
LOCAL_CFLAGS := $(audio_hw_configurable_cflags)


ifeq ($(TARGET_DEVICE),saltbay)
    LOCAL_CFLAGS += -DOPEN_ROUTES_BEFORE_CONFIG
endif

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

#######################################################################
# Build for test with and without gcov for host and target

# Compile macro
define make_audio_hw_configurable_test_lib
$( \
    $(eval LOCAL_COPY_HEADERS_TO := $(audio_hw_configurable_header_copy_folder_unit_test)) \
    $(eval LOCAL_COPY_HEADERS := $(audio_hw_configurable_header_files)) \
    $(eval LOCAL_SRC_FILES += $(audio_hw_configurable_src_files)) \
    $(eval LOCAL_CFLAGS += $(audio_hw_configurable_cflags)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
)
endef
define add_gcov_audio_hw_configurable_test_lib
$( \
    $(eval LOCAL_CFLAGS += -O0 -fprofile-arcs -ftest-coverage) \
    $(eval LOCAL_LDFLAGS += -lgcov) \
)
endef

# Build for host test with gcov
ifeq ($(audiocomms_test_gcov_host),true)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(audio_hw_configurable_includes_dir_host)
$(call make_audio_hw_configurable_test_lib)
$(call add_gcov_audio_hw_configurable_test_lib)
LOCAL_MODULE := libaudio_hw_configurable_static_gcov_host
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target test with gcov
ifeq ($(audiocomms_test_gcov),true)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(audio_hw_configurable_includes_dir_target)
$(call make_audio_hw_configurable_test_lib)
$(call add_gcov_audio_hw_configurable_test_lib)
LOCAL_MODULE := libaudio_hw_configurable_static_gcov
include $(BUILD_STATIC_LIBRARY)

endif

# Build for host test
ifeq ($(audiocomms_test_host),true)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(audio_hw_configurable_includes_dir_host)
$(call make_audio_hw_configurable_test_lib)
LOCAL_MODULE := libaudio_hw_configurable_static_host
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target test
ifeq ($(audiocomms_test_target),true)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(audio_hw_configurable_includes_dir_target)
LOCAL_MODULE := libaudio_hw_configurable_static
$(call make_audio_hw_configurable_test_lib)
include $(BUILD_STATIC_LIBRARY)

endif


endif
endif
