#
# INTEL CONFIDENTIAL
# Copyright © 2013 Intel
# Corporation All Rights Reserved.
#
# The source code contained or described herein and all documents related to
# the source code ("Material") are owned by Intel Corporation or its suppliers
# or licensors. Title to the Material remains with Intel Corporation or its
# suppliers and licensors. The Material contains trade secrets and proprietary
# and confidential information of Intel or its suppliers and licensors. The
# Material is protected by worldwide copyright and trade secret laws and
# treaty provisions. No part of the Material may be used, copied, reproduced,
# modified, published, uploaded, posted, transmitted, distributed, or
# disclosed in any way without Intel’s prior express written permission.
#
# No license under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or delivery
# of the Materials, either expressly, by implication, inducement, estoppel or
# otherwise. Any license under such intellectual property rights must be
# express and approved by Intel in writing.
#

ifeq ($(BOARD_USES_ALSA_AUDIO),true)

LOCAL_PATH := $(call my-dir)

#######################################################################
# Common variables

sample_specifications_src_files :=  \
    AudioUtils.cpp \
    SampleSpec.cpp

sample_specifications_includes_dir := \
    $(LOCAL_PATH)/include \
    frameworks/av/include \
    external/tinyalsa/include

sample_specifications_includes_dir_host := \
    $(sample_specifications_includes_dir) \
    bionic/libc/kernel/common

sample_specifications_includes_dir_target := \
    $(sample_specifications_includes_dir) \
    external/stlport/stlport \
    bionic

sample_specifications_cflags := -Wall -Werror

#######################################################################
# Build for libsamplespec with and without gcov for host and target

# Compile macro
# $(1): "_target" or "_host"
define make_sample_specifications_lib
$( \
    $(eval LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include) \
    $(eval LOCAL_C_INCLUDES := $(sample_specifications_includes_dir_$(1))) \
    $(eval LOCAL_SRC_FILES := $(sample_specifications_src_files)) \
    $(eval LOCAL_CFLAGS := $(sample_specifications_cflags)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
)
endef
define add_gcov
$( \
    $(eval LOCAL_CFLAGS += -O0 --coverage) \
    $(eval LOCAL_LDFLAGS += --coverage) \
)
endef

# Build for host test with gcov
ifeq ($(audiocomms_test_gcov_host),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libsamplespec_static_gcov_host
$(call make_sample_specifications_lib,host)
$(call add_gcov)
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target test with gcov
ifeq ($(audiocomms_test_gcov_target),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libsamplespec_static_gcov
$(call make_sample_specifications_lib,target)
$(call add_gcov)
include $(BUILD_STATIC_LIBRARY)

endif

# Build for host test
ifeq ($(audiocomms_test_host),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libsamplespec_static_host
$(call make_sample_specifications_lib,host)
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target (inconditionnal)

include $(CLEAR_VARS)
LOCAL_MODULE := libsamplespec_static
$(call make_sample_specifications_lib,target)
include $(BUILD_STATIC_LIBRARY)


endif #ifeq ($(BOARD_USES_ALSA_AUDIO),true)
