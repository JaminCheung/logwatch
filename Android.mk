 #
 #  Copyright (C) 2013, Zhang YanMing <jamincheung@126.com>
 #
 #  ingenic log record tool
 #
 #  This program is free software; you can redistribute it and/or modify it
 #  under  the terms of the GNU General  Public License as published by the
 #  Free Software Foundation;  either version 2 of the License, or (at your
 #  option) any later version.
 #
 #  You should have received a copy of the GNU General Public License along
 #  with this program; if not, write to the Free Software Foundation, Inc.,
 #  675 Mass Ave, Cambridge, MA 02139, USA.
 #
 #

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	logwatch.c \
	configure.c \

LOCAL_MODULE := logwatch

##########################
# enable it open debug
##########################
LOCAL_CFLAGS += -g -DDEBUG

##########################
# enable it color debug log
##########################
LOCAL_CFLAGS += -DCOLOR

LOCAL_C_INCLUDES += $(KERNEL_HEADERS)
LOCAL_SHARED_LIBRARIES += libcutils liblog
LOCAL_LDLIBS := -llog
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
