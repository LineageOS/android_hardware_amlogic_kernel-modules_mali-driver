/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <asm/io.h>
#include <mach/am_regs.h>
#include "mali_platform.h"

static void mali_platform_device_release(struct device *device);

#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TV

#define INT_MALI_GP      (48+32)
#define INT_MALI_GP_MMU  (49+32)
#define INT_MALI_PP      (50+32)
#define INT_MALI_PP2     (58+32)
#define INT_MALI_PP3     (60+32)
#define INT_MALI_PP4     (62+32)
#define INT_MALI_PP_MMU  (51+32)
#define INT_MALI_PP2_MMU (59+32)
#define INT_MALI_PP3_MMU (61+32)
#define INT_MALI_PP4_MMU (63+32)

#ifndef CONFIG_MALI400_4_PP
static struct resource meson_mali_resources[] =
{
	MALI_GPU_RESOURCES_MALI400_MP2(0xd0060000, 
			INT_MALI_GP, INT_MALI_GP_MMU, 
			INT_MALI_PP, INT_MALI_PP_MMU, 
			INT_MALI_PP2, INT_MALI_PP2_MMU)
};
#else
static struct resource meson_mali_resources[] =
{
	MALI_GPU_RESOURCES_MALI400_MP4(0xd0060000, 
			INT_MALI_GP, INT_MALI_GP_MMU, 
			INT_MALI_PP, INT_MALI_PP_MMU, 
			INT_MALI_PP2, INT_MALI_PP2_MMU,
			INT_MALI_PP3, INT_MALI_PP3_MMU,
			INT_MALI_PP4, INT_MALI_PP4_MMU
			)
};
#endif

#elif MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6

int static_pp_mmu_cnt;

#define INT_MALI_GP      (48+32)
#define INT_MALI_GP_MMU  (49+32)
#define INT_MALI_PP      (50+32)
#define INT_MALI_PP_MMU  (51+32)
#define INT_MALI_PP2_MMU ( 6+32)

static struct resource meson_mali_resources[] =
{
	MALI_GPU_RESOURCES_MALI400_MP2(0xd0060000, 
			INT_MALI_GP, INT_MALI_GP_MMU, 
			INT_MALI_PP, INT_MALI_PP2_MMU, 
			INT_MALI_PP_MMU, INT_MALI_PP2_MMU)
};

#else

#define INT_MALI_GP	48
#define INT_MALI_GP_MMU 49
#define INT_MALI_PP	50
#define INT_MALI_PP_MMU 51

static struct resource meson_mali_resources[] =
{
	MALI_GPU_RESOURCES_MALI400_MP1(0xd0060000, 
			INT_MALI_GP, INT_MALI_GP_MMU, INT_MALI_PP, INT_MALI_PP_MMU)
};
#endif

void mali_utilization_handler(unsigned int utilization_num)
{

}

static struct mali_gpu_device_data mali_gpu_data =
{
	.shared_mem_size = 512 * 1024 * 1024,
	.fb_start = 0,
	.fb_size = 0,
#ifdef CONFIG_MALI400_UTILIZATION
    .utilization_interval = 1000,
    .utilization_callback = mali_utilization_handler,
#endif
    .max_job_runtime = 60000, /* 60 seconds */
};

extern struct device_type mali_gpu_device_device_type;
static struct platform_device mali_gpu_device =
{
	.name = MALI_GPU_NAME_UTGARD,
	.id = 0,
	.dev.release = mali_platform_device_release,
	.dev.coherent_dma_mask = DMA_BIT_MASK(32),
	.dev.platform_data = &mali_gpu_data,
	.dev.type = &mali_gpu_device_device_type, /* We should probably use the pm_domain instead of type on newer kernels */
};

int mali_pdev_pre_init(struct platform_device* ptr_plt_dev)
{
	if (mali_gpu_data.shared_mem_size < 10) {
		MALI_DEBUG_PRINT(2, ("mali os memory didn't configered, set to default(512M)\n"));
		mali_gpu_data.shared_mem_size = 1024 * 1024 *1024;
	}

	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));
	ptr_plt_dev->num_resources = ARRAY_SIZE(meson_mali_resources);
	ptr_plt_dev->resource = &meson_mali_resources;

	return  0;
}

void mali_pdev_post_init(struct platform_device* pdev)
{
	mali_platform_init();
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	pm_runtime_set_autosuspend_delay(&(pdev->dev), 1000);
	pm_runtime_use_autosuspend(&(pdev->dev));
#endif
	pm_runtime_enable(&(pdev->dev));
#endif
}

int mali_pdev_dts_init(struct platform_device* mali_gpu_device)
{
	struct device_node     *cfg_node = mali_gpu_device->dev.of_node;
    struct device_node     *child;
    u32 prop_value;
    int err;
	struct resource *res;
    u64 *base;
    u64 size;
    
    res = platform_get_resource(mali_gpu_device, IORESOURCE_MEM, 0);
    if (res) {
		base = (u64*)res->start;
		size = res->end - res->start + 1;
		if (size > 0) {
			MALI_DEBUG_PRINT(2, ("dedicated memory size:%dM\n", size / (1024*1024)));
			mali_gpu_data.dedicated_mem_start = base;
    		mali_gpu_data.dedicated_mem_size = size;
		}
	}
	
    for_each_child_of_node(cfg_node, child) {
    	err = of_property_read_u32(child, "shared_memory", &prop_value);
    	if (err == 0) {
    		MALI_DEBUG_PRINT(2, ("shared_memory configurate  %d\n", prop_value));
    		mali_gpu_data.shared_mem_size = prop_value * 1024 * 1024;
    	}
    }

def_start:
	err = mali_pdev_pre_init(mali_gpu_device);
	if (err == 0)
		mali_pdev_post_init(mali_gpu_device);
	return err;
}

int mali_platform_device_register(void)
{
	int err = -1;

#	if MESON_CPU_TYPE == MESON_CPU_TYPE_MESON6
	static_pp_mmu_cnt = 1;
#	endif

	err = mali_pdev_pre_init(&mali_gpu_device);
	if (err == 0) {
		err = platform_device_register(&mali_gpu_device);
		if (0 == err)
			mali_pdev_post_init(&mali_gpu_device);
	}
	return err;
}

void mali_platform_device_unregister(void)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));

	mali_platform_deinit();

	platform_device_unregister(&mali_gpu_device);
}

static void mali_platform_device_release(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}


