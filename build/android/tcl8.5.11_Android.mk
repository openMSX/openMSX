LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := tcl8.5

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

#ifneq ($(NDK_R5_TOOLCHAIN),)
LOCAL_SRC_FILES := lib/$(TARGET_ARCH_ABI)/lib$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)
#else
#LOCAL_SRC_FILES := dummy.c
#include $(BUILD_SHARED_LIBRARY)
#$(abspath $(LOCAL_PATH)/../../obj/local/armeabi/lib$(LOCAL_MODULE).so): $(LOCAL_PATH)/lib/armeabi/lib$(LOCAL_MODULE).so OVERRIDE_CUSTOM_LIB
#	cp -f $< $@
#$(abspath $(LOCAL_PATH)/../../obj/local/armeabi-v7a/lib$(LOCAL_MODULE).so): $(LOCAL_PATH)/lib/armeabi-v7a/lib$(LOCAL_MODULE).so OVERRIDE_CUSTOM_LIB
#	cp -f $< $@
#.PHONY: OVERRIDE_CUSTOM_LIB
#OVERRIDE_CUSTOM_LIB:
#endif
