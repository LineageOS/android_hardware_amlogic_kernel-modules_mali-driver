#
# Copyright (C) 2021-2023 The LineageOS Project
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

ifeq ($(TARGET_PREBUILT_KERNEL),)
MALI_GPU_VARIANT ?= utgard
#MALI_DRV_VERSION ?= r10p0
MALI_DRV_VERSION ?= r8p0
#MALI_PATH        := $(abspath $(call my-dir))
GPU_PATH         := $(MALI_GPU_VARIANT)/$(MALI_DRV_VERSION)
GPU_CONFIGS      := CONFIG_MALI400=m  CONFIG_MALI450=m CONFIG_AM_VDEC_H264_4K2K=y

include $(CLEAR_VARS)

LOCAL_MODULE        := mali
LOCAL_MODULE_SUFFIX := .ko
LOCAL_MODULE_CLASS  := ETC
LOCAL_MODULE_PATH   := $(TARGET_OUT_VENDOR)/lib/modules
LOCAL_CFLAGS        := -Wno-error -Wno-pointer-sign -Wno-error=frame-larger-than=
LOCAL_C_INCLUDES    := \
    $(MALI_PATH)/$(GPU_PATH)/backend/gpu \
    $(MALI_PATH)/$(GPU_PATH)/platform/devicetree \
    $(MALI_PATH)/$(GPU_PATH)/ipa \
    $(MALI_PATH)/$(GPU_PATH) \
    $(MALI_PATH)/dvalin/kernel/include

_mali_intermediates := $(call intermediates-dir-for,$(LOCAL_MODULE_CLASS),$(LOCAL_MODULE))
_mali_ko := $(_mali_intermediates)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
GPU_CFLAGS := $(LOCAL_CFLAGS) $(addprefix -I,$(LOCAL_C_INCLUDES))  -DCONFIG_MALI400=m -DCONFIG_MALI450=m

$(_mali_ko): $(KERNEL_OUT)/arch/$(KERNEL_ARCH)/boot/$(BOARD_KERNEL_IMAGE_NAME)
	@mkdir -p $(dir $@)
	@cp -R $(MALI_PATH)/* $(dir $@)/
	@cp -R $(MALI_PATH)/$(MALI_GPU_VARIANT)/platform $(dir $@)/$(GPU_PATH)
	$(hide) +$(KERNEL_MAKE_CMD) $(PATH_OVERRIDE) $(KERNEL_MAKE_FLAGS) -C $(KERNEL_OUT) M=$(abspath $(_mali_intermediates))/$(GPU_PATH) ARCH=$(TARGET_KERNEL_ARCH) $(KERNEL_CROSS_COMPILE) EXTRA_CFLAGS="$(GPU_CFLAGS)" $(GPU_CONFIGS) modules
	$(TARGET_KERNEL_CLANG_PATH)/bin/llvm-strip --strip-unneeded $(dir $@)/$(GPU_PATH)/mali.ko; \
	cp $(dir $@)/$(GPU_PATH)/mali.ko $@;

include $(BUILD_SYSTEM)/base_rules.mk
endif
