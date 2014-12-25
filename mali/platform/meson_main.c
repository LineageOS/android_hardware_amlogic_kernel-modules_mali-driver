/*
 * Copyright (C) 2010, 2012-2013 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <mach/register.h>
#include <mach/irqs.h>
#include <mach/io.h>
#include <asm/io.h>

#include "meson_main.h"
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include "common/mali_pmu.h"
#include "common/mali_osk_profiling.h"

int mali_pm_statue = 0;
u32 mali_gp_reset_fail = 0;
module_param(mali_gp_reset_fail, int, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH); /* rw-rw-r-- */
MODULE_PARM_DESC(mali_gp_reset_fail, "times of failed to reset GP");
u32 mali_core_timeout = 0;
module_param(mali_core_timeout, int, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH); /* rw-rw-r-- */
MODULE_PARM_DESC(mali_core_timeout, "times of failed to reset GP");

int mali_pdev_pre_init(struct platform_device* ptr_plt_dev)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));
	return mali_meson_init_start(ptr_plt_dev);
}

void mali_pdev_post_init(struct platform_device* pdev)
{
	mali_gp_reset_fail = 0;
	mali_core_timeout = 0;
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	pm_runtime_set_autosuspend_delay(&(pdev->dev), 1000);
	pm_runtime_use_autosuspend(&(pdev->dev));
#endif
	pm_runtime_enable(&(pdev->dev));
#endif
	mali_meson_init_finish(pdev);
}

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data);
static struct mali_gpu_device_data mali_gpu_data =
{
	.shared_mem_size = 1024 * 1024 * 1024,
	.max_job_runtime = 60000, /* 60 seconds */
	.pmu_switch_delay = 0xFFFF, /* do not have to be this high on FPGA, but it is good for testing to have a delay */

	/* the following is for dvfs or utilization. */
	.control_interval = 300, /* 1000ms */
	.get_clock_info = NULL,
	.get_freq = NULL,
	.set_freq = NULL,
	.utilization_callback = mali_gpu_utilization_callback,
};

#ifndef CONFIG_MALI_DT
static void mali_platform_device_release(struct device *device);
static struct platform_device mali_gpu_device =
{
	.name = MALI_GPU_NAME_UTGARD,
	.id = 0,
	.dev.release = mali_platform_device_release,
	.dev.dma_mask = &mali_gpu_device.dev.coherent_dma_mask,
	.dev.coherent_dma_mask = DMA_BIT_MASK(32),
	.dev.platform_data = &mali_gpu_data,
	.dev.type = &mali_pm_device, /* We should probably use the pm_domain instead of type on newer kernels */
};

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
	platform_device_unregister(&mali_gpu_device);
	platform_device_put(&mali_gpu_device);
}

static void mali_platform_device_release(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}

#else /* CONFIG_MALI_DT */

int mali_platform_device_init(struct platform_device *device)
{
	int err = 0;

	ret = mali_pdev_pre_init(&mali_gpu_data);
	if (ret < 0) goto exit;
	err = platform_device_add_data(device, &mali_gpu_data, sizeof(mali_gpu_data));
	if (ret < 0) goto exit;

	mali_pdev_post_init(&mali_gpu_data);
exit:
	return err;
}

int mali_platform_device_deinit(struct platform_device *device)
{
	MALI_IGNORE(device);

	MALI_DEBUG_PRINT(4, ("mali_platform_device_deinit() called\n"));

	mali_core_scaling_term();

	return 0;
}

#endif /* CONFIG_MALI_DT */

