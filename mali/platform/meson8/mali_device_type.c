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
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif

#include <common/mali_kernel_common.h>
#include <common/mali_osk_profiling.h>
#include <common/mali_pmu.h>

#include "mali_scaling.h"
#include "mali_clock.h"

int mali_cri_pmu_on_off(u64 param)
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


static int mali_os_suspend(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_suspend() called\n"));

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

static int mali_os_resume(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_resume() called\n"));

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

static int mali_os_freeze(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_freeze() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->freeze)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->freeze(device);
	}

	return ret;
}

static int mali_os_thaw(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_thaw() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->thaw)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->thaw(device);
	}

	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int mali_runtime_suspend(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_runtime_suspend() called\n"));

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

static int mali_runtime_resume(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_run	time_resume() called\n"));

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

static int mali_runtime_idle(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_runtime_idle() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_idle)
	{
		/* Need to notify Mali driver about this event */
		int ret = device->driver->pm->runtime_idle(device);
		if (0 != ret)
		{
			return ret;
		}
	}

	pm_runtime_suspend(device);

	return 0;
}
#endif

static struct dev_pm_ops mali_gpu_device_type_pm_ops =
{
	.suspend = mali_os_suspend,
	.resume = mali_os_resume,
	.freeze = mali_os_freeze,
	.thaw = mali_os_thaw,
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = mali_runtime_suspend,
	.runtime_resume = mali_runtime_resume,
	.runtime_idle = mali_runtime_idle,
#endif
};

struct device_type mali_gpu_device_device_type =
{
	.pm = &mali_gpu_device_type_pm_ops,
};
