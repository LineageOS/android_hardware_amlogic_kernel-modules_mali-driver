/*
 * Copyright (C) 2010, 2012-2013 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/module.h>            /* kernel module definitions */
#include <linux/ioport.h>            /* request_mem_region */
#include <linux/slab.h>
#include <mach/register.h>
#include <mach/irqs.h>
#include <mach/io.h>
#include <asm/io.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>

#include "mali_scaling.h"
#include "mali_clock.h"
#include "common/mali_pmu.h"
#include "common/mali_osk_profiling.h"

/* Configure dvfs mode */
enum mali_scale_mode_t {
	MALI_PP_SCALING,
	MALI_PP_FS_SCALING,
	MALI_SCALING_DISABLE,
	MALI_TURBO_MODE,
	MALI_SCALING_STAGING,
	MALI_SCALING_MODE_MAX
};

static int reseted_for_turbo = 0;
#define MALI_PP_NUMBER 6

static int  scaling_mode = MALI_PP_FS_SCALING;
module_param(scaling_mode, int, 0664);
MODULE_PARM_DESC(scaling_mode, "0 disable, 1 pp, 2 fs, 4 double");

static int  internal_param = 0x300000;
module_param(internal_param, int, 0664);
MODULE_PARM_DESC(internal_param, " team internal.");

static void mali_platform_device_release(struct device *device);
static void mali_platform_device_release(struct device *device);
void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data);

static struct resource mali_gpu_resources_m450[] =
{
#if 1
	MALI_GPU_RESOURCES_MALI450_MP6_PMU(IO_MALI_APB_PHY_BASE, INT_MALI_GP, INT_MALI_GP_MMU, 
				INT_MALI_PP0, INT_MALI_PP0_MMU, 
				INT_MALI_PP1, INT_MALI_PP1_MMU, 
				INT_MALI_PP2, INT_MALI_PP2_MMU, 
				INT_MALI_PP4, INT_MALI_PP4_MMU, 
				INT_MALI_PP5, INT_MALI_PP5_MMU,
				INT_MALI_PP6, INT_MALI_PP6_MMU, 
				INT_MALI_PP)
#else 
	MALI_GPU_RESOURCES_MALI450_MP3_PMU(IO_MALI_APB_PHY_BASE, INT_MALI_GP, INT_MALI_GP_MMU, 
				INT_MALI_PP0, INT_MALI_PP0_MMU, 
				INT_MALI_PP1, INT_MALI_PP1_MMU, 
				INT_MALI_PP2, INT_MALI_PP2_MMU, 
				INT_MALI_PP)
#endif
};

static struct mali_gpu_device_data mali_gpu_data =
{
	.shared_mem_size = 1024 * 1024 * 1024,
	.fb_start = 0,
	.fb_size = 0,
	.max_job_runtime = 60000, /* 60 seconds */
#ifdef CONFIG_MALI400_UTILIZATION
	.utilization_interval = 500,
	.utilization_callback = mali_gpu_utilization_callback,
#endif
	.pmu_switch_delay = 0xFFFF, /* do not have to be this high on FPGA, but it is good for testing to have a delay */
	.pmu_domain_config = {0x1, 0x2, 0x4, 0x4, 0x4, 0x8, 0x8, 0x8, 0x8, 0x1, 0x2, 0x8},
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
	int num_pp_cores = MALI_PP_NUMBER;

	if (mali_gpu_data.shared_mem_size < 10) {
		MALI_DEBUG_PRINT(2, ("mali os memory didn't configered, set to default(512M)\n"));
		mali_gpu_data.shared_mem_size = 1024 * 1024 *1024;
	}

	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));
	ptr_plt_dev->num_resources = ARRAY_SIZE(mali_gpu_resources_m450);
	ptr_plt_dev->resource = &mali_gpu_resources_m450;
	
	return mali_clock_init(MALI_CLOCK_318);
}

void mali_pdev_post_init(struct platform_device* pdev)
{
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	pm_runtime_set_autosuspend_delay(&(pdev->dev), 1000);
	pm_runtime_use_autosuspend(&(pdev->dev));
#endif
	pm_runtime_enable(&(pdev->dev));
#endif
#ifdef CONFIG_MALI400_UTILIZATION
	mali_core_scaling_init();
#endif
}

int mali_pdev_dts_init(struct platform_device* mali_gpu_device)
{
	struct device_node     *cfg_node = mali_gpu_device->dev.of_node;
    struct device_node     *child;
    u32 prop_value;
    int err;
    mali_dvfs_threshold_table_t* tbl = NULL;
    u32 tbl_size;
    u32 tbl_size_in_byte;

    for_each_child_of_node(cfg_node, child) {
    	err = of_property_read_u32(child, "shared_memory", &prop_value);
    	if (err == 0) {
    		MALI_DEBUG_PRINT(2, ("shared_memory configurate  %d\n", prop_value));
    		mali_gpu_data.shared_mem_size = prop_value * 1024 * 1024;
    	}

    	err = of_property_read_u32(child, "dvfs_size", &prop_value);
    	if (err != 0 || prop_value < 0 || prop_value > get_max_dvfs_tbl_size()) {
    		MALI_DEBUG_PRINT(2, ("invalid recorde_number is.\n"));
    		goto def_start;
    	}

    	tbl_size = sizeof(mali_dvfs_threshold_table_t) * prop_value;
    	tbl_size_in_byte = tbl_size / sizeof(u32);
    	tbl = kzalloc(tbl_size, GFP_KERNEL);
		if (!tbl)
			goto def_start;

		err = of_property_read_u32_array(child,
						"dvfs_table",
						(u32*)tbl,
						tbl_size_in_byte);
		if (err == 0) {
			memcpy(get_mali_dvfs_tbl_addr(), tbl, tbl_size);
			set_mali_dvfs_tbl_size(prop_value);
			kfree(tbl);
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
	mali_core_scaling_term();
	mali_clock_term();
	platform_device_unregister(&mali_gpu_device);

	platform_device_put(&mali_gpu_device);
}

static void mali_platform_device_release(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data)
{
	switch (scaling_mode) {
	case MALI_PP_FS_SCALING:
		reseted_for_turbo = 0;
		set_turbo_mode(0);
		mali_pp_fs_scaling_update(data);
		break;
	case MALI_PP_SCALING:
		reseted_for_turbo = 0;
		set_turbo_mode(0);
		mali_pp_scaling_update(data);
		break;
    case MALI_TURBO_MODE:
		set_turbo_mode(1);
		//mali_pp_fs_scaling_update(data);
		if (reseted_for_turbo == 0) {
				reseted_for_turbo = 1;
				reset_mali_scaling_stat();
		}
		break;

	case MALI_SCALING_STAGING:
		set_turbo_mode(0);
		mali_pp_scaling_staging(data);
		break;
	default:
		set_turbo_mode(0);
		reseted_for_turbo = 0;
	}
}

u32 get_mali_schel_mode(void)
{
	return scaling_mode;
}
void set_mali_schel_mode(u32 mode)
{
	MALI_DEBUG_ASSERT(mode < MALI_SCALING_MODE_MAX);
	if (mode >= MALI_SCALING_MODE_MAX)return;
	scaling_mode = mode;
	reset_mali_scaling_stat();
}

