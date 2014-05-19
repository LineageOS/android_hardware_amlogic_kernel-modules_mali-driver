/*
 * platform.c
 *
 * clock source setting and resource config
 *
 *  Created on: Dec 4, 2013
 *      Author: amlogic
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
#include <linux/mali/mali_utgard.h>

#include <common/mali_kernel_common.h>
#include <common/mali_osk_profiling.h>
#include <common/mali_pmu.h>

#include "meson_main.h"

/**
 *    For Meson 8.
 *
 */
 
u32 mali_clock_turbo_index = 4;
u32 mali_default_clock_idx = 3;
u32 mali_up_clock_idx = 3;

/* fclk is 2550Mhz. */
#define FCLK_DEV3 (6 << 9)		/*	850   Mhz  */
#define FCLK_DEV4 (5 << 9)		/*	637.5 Mhz  */
#define FCLK_DEV5 (7 << 9)		/*	510   Mhz  */
#define FCLK_DEV7 (4 << 9)		/*	364.3 Mhz  */

u32 mali_dvfs_clk[] = {
	FCLK_DEV7 | 1,     /* 182.1 Mhz */
	FCLK_DEV4 | 1,     /* 318.7 Mhz */
	FCLK_DEV3 | 1,     /* 425 Mhz */
	FCLK_DEV5 | 0,     /* 510 Mhz */
	FCLK_DEV4 | 0,     /* 637.5 Mhz */
};

u32 mali_dvfs_clk_sample[] = {
	182,     /* 182.1 Mhz */
	319,     /* 318.7 Mhz */
	425,     /* 425 Mhz */
	510,     /* 510 Mhz */
	637,     /* 637.5 Mhz */
};

#define MALI_PP_NUMBER 2

static struct resource mali_gpu_resources[] =
{
	MALI_GPU_RESOURCES_MALI450_MP2_PMU(IO_MALI_APB_PHY_BASE, INT_MALI_GP, INT_MALI_GP_MMU, 
				INT_MALI_PP0, INT_MALI_PP0_MMU,
				INT_MALI_PP1, INT_MALI_PP1_MMU,
				INT_MALI_PP)
};

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data);
int mali_meson_init_start(struct platform_device* ptr_plt_dev)
{

	struct mali_gpu_device_data* pdev = ptr_plt_dev->dev.platform_data;

	/* for mali platform data. */
	pdev->utilization_interval = 500,
	pdev->utilization_callback = mali_gpu_utilization_callback,

	/* for resource data. */
	ptr_plt_dev->num_resources = ARRAY_SIZE(mali_gpu_resources);
	ptr_plt_dev->resource = mali_gpu_resources;
	return mali_clock_init(mali_default_clock_idx);
}

int mali_meson_init_finish(struct platform_device* ptr_plt_dev)
{
	mali_core_scaling_init(MALI_PP_NUMBER, mali_default_clock_idx);
	return 0;
}

int mali_meson_uninit(struct platform_device* ptr_plt_dev)
{
	return 0;
}

static int mali_cri_pmu_on_off(size_t param)
{
	struct mali_pmu_core *pmu;

	MALI_DEBUG_PRINT(4, ("mali_os_suspend() called\n"));
	pmu = mali_pmu_get_global_pmu_core();
	if (param == 0)
		mali_pmu_power_down_all(pmu);
	else
		mali_pmu_power_up_all(pmu);
	return 0;
}

int mali_light_suspend(struct device *device)
{
	int ret = 0;
#ifdef CONFIG_MALI400_PROFILING
	_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
					MALI_PROFILING_EVENT_CHANNEL_GPU |
					MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
					0, 0,	0,	0,	0);
#endif
	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_suspend)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->runtime_suspend(device);
	}

	/* clock scaling. Kasin..*/
	mali_clock_critical(mali_cri_pmu_on_off, 0);
	//disable_clock();
	return ret;
}

int mali_light_resume(struct device *device)
{
	int ret = 0;
	/* clock scaling. Kasin..*/
	//enable_clock();

	mali_clock_critical(mali_cri_pmu_on_off, 1);
#ifdef CONFIG_MALI400_PROFILING
	_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
					MALI_PROFILING_EVENT_CHANNEL_GPU |
					MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
					get_current_frequency(), 0,	0,	0,	0);
#endif

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_resume)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->runtime_resume(device);
	}
	return ret;
}

int mali_deep_suspend(struct device *device)
{
	int ret = 0;
	enable_clock();
	flush_scaling_job();
	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->suspend)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->suspend(device);
	}

	/* clock scaling off. Kasin... */
	mali_clock_critical(mali_cri_pmu_on_off, 0);
	disable_clock();
	return ret;
}

int mali_deep_resume(struct device *device)
{
	int ret = 0;
	/* clock scaling up. Kasin.. */
	enable_clock();
	mali_clock_critical(mali_cri_pmu_on_off, 1);
	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->resume)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->resume(device);
	}
	return ret;

}

