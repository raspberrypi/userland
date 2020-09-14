LOCAL_PATH := $(call my-dir)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libmmal
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := build/lib/libmmal.so
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libmmal_core
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := build/lib/libmmal_core.so
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libmmal_util
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := build/lib/libmmal_util.so
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libmmal_vc_client
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := build/lib/libmmal_vc_client.so
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libmmal_components
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := build/lib/libmmal_components.so
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libvchiq_arm
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := build/lib/libvchiq_arm.so
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libvcos
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := build/lib/libvcos.so
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := raspistill
LOCAL_SRC_FILES := build/bin/raspistill
LOCAL_MODULE_CLASS := EXECUTABLES
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := raspivid
LOCAL_SRC_FILES := build/bin/raspivid
LOCAL_MODULE_CLASS := EXECUTABLES
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := mmal_vc_diag
LOCAL_SRC_FILES := build/bin/mmal_vc_diag
LOCAL_MODULE_CLASS := EXECUTABLES
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libopenmaxil
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := build/lib/libopenmaxil.so
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libbcm_host
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := build/lib/libbcm_host.a
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libvchostif
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := build/lib/libvchostif.a
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libvcilcs
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := build/lib/libvcilcs.a
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libvcsm
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := build/lib/libvcsm.a
include $(BUILD_PREBUILT)

#######################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libilclient
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := build/lib/libilclient.a
include $(BUILD_PREBUILT)
