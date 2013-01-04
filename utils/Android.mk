LOCAL_PATH := $(call my-dir)

# common variables
##################

utils_exported_includes_folder := audio_hal_utils
utils_exported_includes_files := \
    SyncSemaphore.h \
    SyncSemaphoreList.h

utils_src_files := \
    SyncSemaphore.cpp \
    SyncSemaphoreList.cpp


# build for target
##################
include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := $(utils_exported_includes_folder)
LOCAL_COPY_HEADERS := $(utils_exported_includes_files)

LOCAL_CFLAGS := -DDEBUG

LOCAL_SRC_FILES := $(utils_src_files)

LOCAL_C_INCLUDES += \
    external/stlport/stlport \
    bionic

LOCAL_SHARED_LIBRARIES := libstlport

LOCAL_MODULE := libaudiohalutils
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
