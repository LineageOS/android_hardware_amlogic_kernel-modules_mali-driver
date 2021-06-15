#
# Copyright (C) 2015 The Android Open Source Project
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


GPU_TYPE:=mali450
GPU_ARCH:=utgard
GPU_DRV_VERSION?=r10p1

EXTRA_INCLUDE := -I$(KERNEL_SRC)/$(M)/../utgard/$(GPU_DRV_VERSION)/include \
		 -I$(KERNEL_SRC)/$(M)/../utgard/$(GPU_DRV_VERSION)/linux/license/gpl \
		 -I$(KERNEL_SRC)/$(M)/../utgard/$(GPU_DRV_VERSION)/common \
		 -I$(KERNEL_SRC)/$(M)/../utgard/$(GPU_DRV_VERSION)/linux \
		 -I$(KERNEL_SRC)/$(M)/../utgard/platform/meson_bu

KBUILD_CFLAGS_MODULE += $(GKI_EXT_MODULE_PREDEFINE)

modules:
	cd $(M)/$(GPU_DRV_VERSION); \
	ln -s $(M)/platform platform; \
	$(MAKE) -C $(KERNEL_SRC) M=$(M)/$(GPU_DRV_VERSION)  \
	EXTRA_CFLAGS="-DCONFIG_MALI400=m -DCONFIG_MALI450=m" \
	EXTRA_CFLAGS+="-Wno-error -Wno-pointer-sign -Wno-error=frame-larger-than= $(EXTRA_INCLUDE) $(KBUILD_CFLAGS_MODULE)" \
	EXTRA_LDFLAGS+="--strip-debug" \
	CONFIG_MALI400=m  CONFIG_MALI450=m CONFIG_AM_VDEC_H264_4K2K=y modules

modules_install:
	@$(MAKE) INSTALL_MOD_STRIP=1 M=$(M)/$(GPU_DRV_VERSION) -C $(KERNEL_SRC) modules_install
	mkdir -p ${OUT_DIR}/../vendor_lib/modules
	cd ${OUT_DIR}/$(M)/; find -name "*.ko" -exec cp {} ${OUT_DIR}/../vendor_lib/modules/mali.ko \;

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M)/$(GPU_DRV_VERSION) clean
