LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	logwatch.c \
	configure.c \

LOCAL_MODULE := logwatch

##########################
# enable it open debug
##########################
#LOCAL_CFLAGS += -g -DDEBUG

LOCAL_C_INCLUDES += $(KERNEL_HEADERS)
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
