LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := hardware/intel/include
LOCAL_CFLAGS += -g -Wall
LOCAL_SRC_FILES:= dummy-stmd-daemon.c
LOCAL_MODULE :=  dummy-stmd
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := libcutils libc
LOCAL_LDLIBS += -lpthread
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := hardware/intel/include
LOCAL_CFLAGS += -g -Wall
LOCAL_SRC_FILES:= stmd-dummy-app.c
LOCAL_MODULE :=  stmd-dummy
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := libcutils libc
LOCAL_LDLIBS += -lpthread
include $(BUILD_EXECUTABLE)
