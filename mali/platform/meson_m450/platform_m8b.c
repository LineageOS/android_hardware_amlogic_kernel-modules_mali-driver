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
#include <linux/gpu_cooling.h>
#include <linux/gpucore_cooling.h>
#include "meson_main.h"

/**
 *    For Meson 8.
 *
 */
 
u32 mali_clock_turbo_index = 4;
u32 mali_default_clock_idx = 1;
u32 mali_up_clock_idx = 3;

/* fclk is 2550Mhz. */
#define FCLK_DEV3 (6 << 9)		/*	850   Mhz  */
#define FCLK_DEV4 (5 << 9)		/*	637.5 Mhz  */
#define FCLK_DEV5 (7 << 9)		/*	510   Mhz  */
#define FCLK_DEV7 (4 << 9)		/*	364.3 Mhz  */

u32 mali_dvfs_clk[] = {
	FCLK_DEV5 | 1,     /* 255 Mhz */
	FCLK_DEV7 | 0,     /* 364 Mhz */
	FCLK_DEV3 | 1,     /* 425 Mhz */
	FCLK_DEV5 | 0,     /* 510 Mhz */
	FCLK_DEV4 | 0,     /* 637.5 Mhz */
};

u32 mali_dvfs_clk_sample[] = {
	255,     /* 182.1 Mhz */
	364,     /* 318.7 Mhz */
	425,     /* 425 Mhz */
	510,     /* 510 Mhz */
	637,     /* 637.5 Mhz */
};

u32 get_mali_tbl_size(void)
{
	return sizeof(mali_dvfs_clk) / sizeof(u32);
}

int get_mali_freq_level(int freq)
{
	int i = 0, level = -1;
	if(freq < 0)
		return level;
	int mali_freq_num = sizeof(mali_dvfs_clk_sample) / sizeof(mali_dvfs_clk_sample[0]) - 1;
	if(freq <= mali_dvfs_clk_sample[0])
		level = mali_freq_num-1;
	if(freq >= mali_dvfs_clk_sample[mali_freq_num - 1])
		level = 0;
	for(i=0; i<mali_freq_num - 1 ;i++) {
		if(freq >= mali_dvfs_clk_sample[i] && freq<=mali_dvfs_clk_sample[i+1]) {
			level = i;
			level = mali_freq_num-level-1;
		}
	}
	return level;
}

unsigned int get_mali_max_level(void)
{
	int mali_freq_num = sizeof(mali_dvfs_clk_sample) / sizeof(mali_dvfs_clk_sample[0]);
	return mali_freq_num - 1;
}
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
	pdev->utilization_interval = 200,
	pdev->utilization_callback = mali_gpu_utilization_callback,

	/* for resource data. */
	ptr_plt_dev->num_resources = ARRAY_SIZE(mali_gpu_resources);
	ptr_plt_dev->resource = mali_gpu_resources;
	return mali_clock_init(mali_default_clock_idx);
}

int mali_meson_init_finish(struct platform_device* ptr_plt_dev)
{
	mali_core_scaling_init(MALI_PP_NUMBER, mali_default_clock_idx);
#ifdef CONFIG_GPU_THERMAL
	int err;
	struct gpufreq_cooling_device *gcdev = NULL;
	gcdev = gpufreq_cooling_alloc();
	if(IS_ERR(gcdev))
		printk("malloc gpu cooling buffer error!!\n");
	else if(!gcdev)
		printk("system does not enable thermal driver\n");
	else {
		gcdev->get_gpu_freq_level = get_mali_freq_level;
		gcdev->get_gpu_max_level = get_mali_max_level;
		gcdev->set_gpu_freq_idx = set_max_mali_freq;
		gcdev->get_gpu_current_max_level = get_max_mali_freq;
		err = gpufreq_cooling_register(gcdev);
		if(err < 0)
			printk("register GPU  cooling error\n");
		printk("gpu cooling register okay with err=%d\n",err);
	}
#if 0
	struct gpucore_cooling_device *gccdev=NULL;
	gccdev=gpucore_cooling_alloc();
	if(IS_ERR(gccdev))
		printk("malloc gpu core cooling buffer error!!\n");
	else if(!gccdev)
		printk("system does not enable thermal driver\n");
	else {
		gccdev->max_gpu_core_num=MALI_PP_NUMBER;
		gccdev->set_max_pp_num=set_max_pp_num;
		err=gpucore_cooling_register(gccdev);
		if(err < 0)
			printk("register GPU  cooling error\n");
		printk("gpu core cooling register okay with err=%d\n",err);
	}
#endif
#endif
	return 0;
}

int mali_meson_uninit(struct platform_device* ptr_plt_dev)
{
	return 0;
}

static int mali_cri_light_suspend(size_t param)
{
	int ret;
	struct device *device;
	struct mali_pmu_core *pmu;

	ret = 0;
	mali_pm_statue = 0;
	device = (struct device *)param;
	pmu = mali_pmu_get_global_pmu_core();

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_suspend)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->runtime_suspend(device);
	}
	mali_pmu_power_down_all(pmu);
	return ret;
}

static int mali_cri_light_resume(size_t param)
{
	int ret;
	struct device *device;
	struct mali_pmu_core *pmu;

	ret = 0;
	device = (struct device *)param;
	pmu = mali_pmu_get_global_pmu_core();

	mali_pmu_power_up_all(pmu);
	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_resume)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->runtime_resume(device);
	}
	mali_pm_statue = 1;
	return ret;
}

static int mali_cri_deep_suspend(size_t param)
{
	int ret;
	struct device *device;
	struct mali_pmu_core *pmu;

	ret = 0;
	device = (struct device *)param;
	pmu = mali_pmu_get_global_pmu_core();

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->suspend)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->suspend(device);
	}
	mali_pmu_power_down_all(pmu);
	return ret;
}

static int mali_cri_deep_resume(size_t param)
{
	int ret;
	struct device *device;
	struct mali_pmu_core *pmu;

	ret = 0;
	device = (struct device *)param;
	pmu = mali_pmu_get_global_pmu_core();

	mali_pmu_power_up_all(pmu);
	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->resume)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->resume(device);
	}
	return ret;

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

	/* clock scaling. Kasin..*/
	ret = mali_clock_critical(mali_cri_light_suspend, (size_t)device);
	disable_clock();
	return ret;
}

int mali_light_resume(struct device *device)
{
	int ret = 0;
	enable_clock();
	ret = mali_clock_critical(mali_cri_light_resume, (size_t)device);
#ifdef CONFIG_MALI400_PROFILING
	_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
					MALI_PROFILING_EVENT_CHANNEL_GPU |
					MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
					get_current_frequency(), 0,	0,	0,	0);
#endif
	return ret;
}

int mali_deep_suspend(struct device *device)
{
	int ret = 0;
	enable_clock();
	flush_scaling_job();

	/* clock scaling off. Kasin... */
	ret = mali_clock_critical(mali_cri_deep_suspend, (size_t)device);
	disable_clock();
	return ret;
}

int mali_deep_resume(struct device *device)
{
	int ret = 0;

	/* clock scaling up. Kasin.. */
	enable_clock();
	ret = mali_clock_critical(mali_cri_deep_resume, (size_t)device);
	return ret;

}

