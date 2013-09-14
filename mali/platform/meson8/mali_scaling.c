/*
 * Copyright (C) 2013 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file arm_core_scaling.c
 * Example core scaling policy.
 */
#include <linux/workqueue.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include "mali_scaling.h"

#include "mali_clock.h"

static int num_cores_total;
static int num_cores_enabled;
static int currentStep;
static int lastStep;
static struct work_struct wq_work;
unsigned int utilization;


unsigned int mali_dvfs_clk[] = {
//                FCLK_DEV7 | 3,     /* 91  Mhz */
                FCLK_DEV7 | 1,     /* 182.1 Mhz */
                FCLK_DEV4 | 1,     /* 318.7 Mhz */
                FCLK_DEV3 | 1,     /* 425 Mhz */
                FCLK_DEV4 | 0,     /* 637.5 Mhz */
};

static mali_dvfs_threshold_table mali_dvfs_threshold[]={
        { 0  		    		  , (43 * 256) / 100.0 + 0.5}, 
        { (40 * 256) / 100.0 + 0.5, (53 * 256) / 100.0 + 0.5},
        { (50 * 256) / 100.0 + 0.5, (92 * 256) / 100.0 + 0.5},
        { (87 * 256) / 100.0 + 0.5, 256			 } 
};

enum mali_pp_scale_threshold_t {
	MALI_PP_THRESHOLD_40,
	MALI_PP_THRESHOLD_50,
	MALI_PP_THRESHOLD_90,
	MALI_PP_THRESHOLD_MAX,
};
static u32 mali_pp_scale_threshold [] = {
	(40 * 256) / 100.0 + 0.5,
	(50 * 256) / 100.0 + 0.5,
	(90 * 256) / 100.0 + 0.5,
};

static int mali_utilization_low = (256 * 30) / 100.0 + 0.5;
static int mali_utilization_mid = (256 * 60) / 100.0 + 0.5;
static int mali_utilization_high = (256 * 80) / 100.0 + 0.5;

static void do_scaling(struct work_struct *work)
{
	int err = mali_perf_set_num_pp_cores(num_cores_enabled);
	MALI_DEBUG_ASSERT(0 == err);
	MALI_IGNORE(err);
	if (currentStep != lastStep) {
		mali_dev_pause();
		mali_clock_set (mali_dvfs_clk[currentStep]);
		mali_dev_resume();
		lastStep = currentStep;
	}
}

static void enable_one_core(void)
{
	if (num_cores_enabled < num_cores_total)
	{
		++num_cores_enabled;
		schedule_work(&wq_work);
		MALI_DEBUG_PRINT(3, ("Core scaling: Enabling one more core\n"));
	}

	MALI_DEBUG_ASSERT(              1 <= num_cores_enabled);
	MALI_DEBUG_ASSERT(num_cores_total >= num_cores_enabled);
}

static void disable_one_core(void)
{
	if (1 < num_cores_enabled)
	{
		--num_cores_enabled;
		schedule_work(&wq_work);
		MALI_DEBUG_PRINT(3, ("Core scaling: Disabling one core\n"));
	}

	MALI_DEBUG_ASSERT(              1 <= num_cores_enabled);
	MALI_DEBUG_ASSERT(num_cores_total >= num_cores_enabled);
}

static void enable_max_num_cores(void)
{
	if (num_cores_enabled < num_cores_total)
	{
		num_cores_enabled = num_cores_total;
		schedule_work(&wq_work);
		MALI_DEBUG_PRINT(3, ("Core scaling: Enabling maximum number of cores\n"));
	}

	MALI_DEBUG_ASSERT(num_cores_total == num_cores_enabled);
}

void mali_core_scaling_init(int num_pp_cores, int clock_rate_index)
{
	INIT_WORK(&wq_work, do_scaling);

	num_cores_total   = num_pp_cores;
	num_cores_enabled = num_pp_cores;
	
	currentStep = clock_rate_index;
	lastStep = currentStep;
	/* NOTE: Mali is not fully initialized at this point. */
}

void mali_core_scaling_term(void)
{
	flush_scheduled_work();
}

#define PERCENT_OF(percent, max) ((int) ((percent)*(max)/100.0 + 0.5))

void mali_pp_scaling_update(struct mali_gpu_utilization_data *data)
{
	/*
	 * This function implements a very trivial PP core scaling algorithm.
	 *
	 * It is _NOT_ of production quality.
	 * The only intention behind this algorithm is to exercise and test the
	 * core scaling functionality of the driver.
	 * It is _NOT_ tuned for neither power saving nor performance!
	 *
	 * Other metrics than PP utilization need to be considered as well
	 * in order to make a good core scaling algorithm.
	 */

	MALI_DEBUG_PRINT(3, ("Utilization: (%3d, %3d, %3d), cores enabled: %d/%d\n", data->utilization_gpu, data->utilization_gp, data->utilization_pp, num_cores_enabled, num_cores_total));

	/* NOTE: this function is normally called directly from the utilization callback which is in
	 * timer context. */

	if (mali_pp_scale_threshold[MALI_PP_THRESHOLD_90] < data->utilization_pp)
	{
		enable_max_num_cores();
	}
	else if (mali_pp_scale_threshold[MALI_PP_THRESHOLD_50]< data->utilization_pp)
	{
		enable_one_core();
	}
	else if (mali_pp_scale_threshold[MALI_PP_THRESHOLD_40]< data->utilization_pp)
	{
		/* do nothing */
	}
	else if (0 < data->utilization_pp)
	{
		disable_one_core();
	}
	else
	{
		/* do nothing */
	}
}

void mali_pp_fs_scaling_update(struct mali_gpu_utilization_data *data)
{
	MALI_DEBUG_PRINT(2, ("Utilization: (%3d, %3d, %3d), cores enabled: %d/%d\n", data->utilization_gpu, data->utilization_gp, data->utilization_pp, num_cores_enabled, num_cores_total));
	MALI_DEBUG_PRINT(2, ("  %d   \n", currentStep));
	utilization = data->utilization_gpu;

	if (utilization > mali_dvfs_threshold[currentStep].upthreshold) {
		if (utilization < mali_utilization_high && currentStep < MALI_CLOCK_INDX_MAX) 
			currentStep ++;
		else
			currentStep = MALI_CLOCK_637;

		if (data->utilization_pp > 230) // 90%
			enable_max_num_cores();
		else
			enable_one_core();
	} else if (utilization < mali_dvfs_threshold[currentStep].downthreshold && currentStep > 0) {
		currentStep--;
		MALI_DEBUG_PRINT(2, ("Mali clock set %d..\n",currentStep));
	} else {
		if (data->utilization_pp < mali_pp_scale_threshold[0])
			disable_one_core();
		return;
	}

}

void reset_mali_scaling_stat(void)
{
	printk(" ****** scaling mode reset to default.*****\n");
	currentStep = mali_default_clock_step;
	enable_max_num_cores();
}

void mali_fs_scaling_update(struct mali_gpu_utilization_data *data)
{
	
	utilization = data->utilization_gpu;

	if (utilization > mali_dvfs_threshold[currentStep].upthreshold) {
		currentStep = MALI_CLOCK_637;
	} else if (utilization < mali_dvfs_threshold[currentStep].downthreshold && currentStep > 0) {
		currentStep--;
		MALI_DEBUG_PRINT(2, ("Mali clock set %d..\n",currentStep));
	} 
}

u32 get_mali_def_freq_idx(void)
{
	return mali_default_clock_step;
}

void set_mali_freq_idx(u32 idx)
{
	MALI_DEBUG_ASSERT(clock_rate_index < MALI_CLOCK_INDX_MAX);
	currentStep = idx;
	lastStep = MALI_CLOCK_INDX_MAX;
	mali_default_clock_step = currentStep;
	schedule_work(&wq_work);
	/* NOTE: Mali is not fully initialized at this point. */
}

void set_mali_qq_for_sched(u32 pp_num)
{
	num_cores_total   = pp_num;
	num_cores_enabled = pp_num;
	schedule_work(&wq_work);
}

u32 get_mali_qq_for_sched(void)
{
	return num_cores_total;	
}
