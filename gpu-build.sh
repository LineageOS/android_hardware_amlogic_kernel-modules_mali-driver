#!/bin/sh

GPU_TYPE=$1
echo "GPU_TYPE is ${GPU_TYPE}"
PRODUCT_OUT=${OUT}
MESON_GPU_DIR=./
PREFIX_CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
KERNEL_ARCH=arm64
GPU_MODS_OUT=obj/lib_vendor/

if [ x$2 = x32 ]; then
	KERNEL_ARCH=arm
	PREFIX_CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
fi
echo "KERNEL_ARCH=${KERNEL_ARCH}"

SOURCE_CODE=$3
if [ x${SOURCE_CODE} = x ]; then
	if [ x${GPU_TYPE} = xmali ]; then
		SOURCE_CODE=utgard/r10p0
	elif [ x${GPU_TYPE} = xbif ]; then
		SOURCE_CODE=bifrost/r21p0
	fi
fi

KDIR=$4
if [ x${KDIR} = x ];then
	if [ x${PRODUCT_OUT} = x ]; then
		echo "shall set KDIR in 4th para"
		exit
	fi
	KDIR=${PRODUCT_OUT}/obj/KERNEL_OBJ
fi

if [ x${PRODUCT_OUT} = x ]; then
	if [ x$5 = x ]; then
		PRODUCT_OUT=${KDIR}/../..
	else
		PRODUCT_OUT=$5
	fi
	mkdir -p ${PRODUCT_OUT}
fi

echo "module in ${SOURCE_CODE}, KDIR=${KDIR} building"

PATH=${TARGET_HOST_TOOL_PATH}:$PATH

usage()
{
	echo "$0 gpu_type [[[[arch] source] KDIR] output]"
	echo "gpu_type: mali bif"
	echo "arch:     32 64"
	echo "source:   mali driver path, like bifrost/r21p0"
	echo "KDIR:     linux kernel dir"
	echo "output:   kernel building path"
}
utgard_build()
{
	rm ${PRODUCT_OUT}/obj/mali -rf
	mkdir -p ${PRODUCT_OUT}/obj/mali
	cp ${SOURCE_CODE}/*  ${PRODUCT_OUT}/obj/mali -airf
	cp ${MESON_GPU_DIR}/utgard/platform  ${PRODUCT_OUT}/obj/mali/ -airf
	echo "make mali module MALI_OUT is ${PRODUCT_OUT}/obj/mali ${MALI_OUT}"
	make -C ${KDIR} M=${PRODUCT_OUT}/obj/mali  \
		ARCH=${KERNEL_ARCH} CROSS_COMPILE=${PREFIX_CROSS_COMPILE} CONFIG_MALI400=m  CONFIG_MALI450=m	\
		EXTRA_CFLAGS="-DCONFIG_MALI400=m -DCONFIG_MALI450=m" \
		EXTRA_LDFLAGS+="--strip-debug" \
		CONFIG_AM_VDEC_H264_4K2K=y 2>&1 | tee mali.txt

	echo "GPU_MODS_OUT is ${GPU_MODS_OUT}"
	mkdir -p ${PRODUCT_OUT}/${GPU_MODS_OUT}
	cp  ${PRODUCT_OUT}/obj/mali/mali.ko ${PRODUCT_OUT}/${GPU_MODS_OUT}/mali.ko

	cp  ${PRODUCT_OUT}/${GPU_MODS_OUT}/mali.ko ${PRODUCT_OUT}/obj/lib_vendor/mali.ko
	echo "${GPU_ARCH}.ko build finished"
}

bifrost_build()
{

	rm ${PRODUCT_OUT}/obj/bifrost -rf
	mkdir -p ${PRODUCT_OUT}/obj/bifrost
	cp ${SOURCE_CODE}/*  ${PRODUCT_OUT}/obj/bifrost -airf
	make -C ${KDIR} M=${PRODUCT_OUT}/obj/bifrost/kernel/drivers/gpu/arm/midgard \
		ARCH=${KERNEL_ARCH} CROSS_COMPILE=${PREFIX_CROSS_COMPILE} \
		EXTRA_CFLAGS="-DCONFIG_MALI_PLATFORM_DEVICETREE -DCONFIG_MALI_MIDGARD_DVFS -DCONFIG_MALI_BACKEND=gpu " \
		EXTRA_CFLAGS+="-I${PRODUCT_OUT}/obj/bifrost/kernel/include " \
		EXTRA_CFLAGS+="-Wno-error=larger-than=16384  -DCONFIG_MALI_DMA_BUF_MAP_ON_DEMAND=1 -DCONFIG_MALI_DMA_BUF_LEGACY_COMPAT=0" \
		EXTRA_LDFLAGS+="--strip-debug" \
		CONFIG_MALI_MIDGARD=m CONFIG_MALI_PLATFORM_DEVICETREE=y CONFIG_MALI_MIDGARD_DVFS=y CONFIG_MALI_BACKEND=gpu

	mkdir -p ${PRODUCT_OUT}/${GPU_MODS_OUT}
	echo "GPU_MODS_OUT is ${GPU_MODS_OUT}"
	cp  ${PRODUCT_OUT}/obj/bifrost/kernel/drivers/gpu/arm/midgard/mali_kbase.ko ${PRODUCT_OUT}/${GPU_MODS_OUT}/mali.ko
}

echo "args is $#"
if [ $# -lt 2 ]; then
	usage
fi

if [ x${GPU_TYPE} = xmali ]; then
	utgard_build
elif [ x${GPU_TYPE} = xbif ]; then
	bifrost_build
fi

exit
