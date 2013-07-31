# Android.mk
#
# Copyright 2013 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

# Common variables
##################

hal_dump_exported_includes_folder := hal_audio_dump
hal_dump_exported_includes_files := HALAudioDump.h

hal_dump_src_files := HALAudioDump.cpp

# Build for target
##################
include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := $(hal_dump_exported_includes_folder)
LOCAL_COPY_HEADERS := $(hal_dump_exported_includes_files)

LOCAL_CFLAGS := -DDEBUG -Wall -Werror

LOCAL_SRC_FILES := $(hal_dump_src_files)

LOCAL_C_INCLUDES += \
    external/stlport/stlport \
    bionic

LOCAL_SHARED_LIBRARIES := \
        libstlport \
        libutils \
        libcutils


LOCAL_MODULE := libhalaudiodump
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)