ifeq ($(BOARD_USES_ALSA_AUDIO),true)

LOCAL_PATH := $(call my-dir)

  include $(CLEAR_VARS)

  LOCAL_PRELINK_MODULE := false

  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

  LOCAL_CFLAGS := -D_POSIX_SOURCE -Wall -Werror

  LOCAL_C_INCLUDES += \
        external/tinyalsa/include \
        $(TARGET_OUT_HEADERS)/alsa-sound \
        $(TARGET_OUT_HEADERS)/hw

  LOCAL_SRC_FILES:= \
        tinyalsawrapper.cpp \

  LOCAL_SHARED_LIBRARIES := \
        libtinyalsa \
        liblog


  LOCAL_MODULE:= tinyalsa.$(TARGET_DEVICE)
  LOCAL_MODULE_TAGS := optional

  include $(BUILD_SHARED_LIBRARY)
endif
