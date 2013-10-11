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
static u32 last_utilization_pp;
static u32 last_utilization_gp;
static u32 last_utilization_gp_pp;

unsigned int min_mali_clock = MALI_CLOCK_182;
unsigned int max_mali_clock = MALI_CLOCK_637;
unsigned int min_pp_num = 1;

unsigned int mali_dvfs_clk[] = {
                FCLK_DEV7 | 1,     /* 182.1 Mhz */
                FCLK_DEV4 | 1,     /* 318.7 Mhz */
                FCLK_DEV3 | 1,     /* 425 Mhz */
                FCLK_DEV5 | 0,     /* 510 Mhz */
                FCLK_DEV4 | 0,     /* 637.5 Mhz */
};

static mali_dvfs_threshold_table mali_dvfs_threshold[]={
        { 0  		    		  , (43 * 256) / 100.0 + 0.5}, 
        { (40 * 256) / 100.0 + 0.5, (53 * 256) / 100.0 + 0.5},
        { (50 * 256) / 100.0 + 0.5, (83 * 256) / 100.0 + 0.5},
        { (80 * 256) / 100.0 + 0.5, (93 * 256) / 100.0 + 0.5},
        { (90 * 256) / 100.0 + 0.5, 256			 			}
};

enum mali_pp_scale_threshold_t {
	MALI_PP_THRESHOLD_30,
	MALI_PP_THRESHOLD_40,
	MALI_PP_THRESHOLD_50,
	MALI_PP_THRESHOLD_60,
	MALI_PP_THRESHOLD_90,
	MALI_PP_THRESHOLD_MAX,
};
static u32 mali_pp_scale_threshold [] = {
	(30 * 256) / 100.0 + 0.5,
	(40 * 256) / 100.0 + 0.5,
	(50 * 256) / 100.0 + 0.5,
	(60 * 256) / 100.0 + 0.5,
	(90 * 256) / 100.0 + 0.5,
};

static int mali_utilization_low	= (256 * 30) / 100.0 + 0.5;
static int mali_utilization_mid	= (256 * 60) / 100.0 + 0.5;
static int mali_utilization_high	= (256 * 80) / 100.0 + 0.5;

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

static u32 enable_one_core(void)
{
	u32 ret = 0;
	if (num_cores_enabled < num_cores_total)
	{
		++num_cores_enabled;
		schedule_work(&wq_work);
		ret = 1;
		MALI_DEBUG_PRINT(3, ("Core scaling: Enabling one more core\n"));
	}

	MALI_DEBUG_ASSERT(              1 <= num_cores_enabled);
	MALI_DEBUG_ASSERT(num_cores_total >= num_cores_enabled);
	return ret;
}

static void disable_one_core(void)
{
	u32 ret = 0;
	if (min_pp_num < num_cores_enabled)
	{
		--num_cores_enabled;
		schedule_work(&wq_work);
		ret = 1;
		MALI_DEBUG_PRINT(3, ("Core scaling: Disabling one core\n"));
	}

	MALI_DEBUG_ASSERT(              min_pp_num <= num_cores_enabled);
	MALI_DEBUG_ASSERT(num_cores_total >= num_cores_enabled);
	return ret;
}

static void enable_max_num_cores(void)
{
	u32 ret = 0;
	if (num_cores_enabled < num_cores_total)
	{
		num_cores_enabled = num_cores_total;
		schedule_work(&wq_work);
		ret = 1;
		MALI_DEBUG_PRINT(3, ("Core scaling: Enabling maximum number of cores\n"));
	}

	MALI_DEBUG_ASSERT(num_cores_total == num_cores_enabled);
	return ret;
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

void mali_pp_scaling_update(struct mali_gpu_utilization_data *data)
{
	currentStep = MALI_CLOCK_637;
	
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
		#if 0
		currentStep = MALI_CLOCK_425;
		schedule_work(&wq_work);
		#endif
	}
	else if (0 < data->utilization_pp)
	{
		if (num_cores_enabled == min_pp_num) {
			if ( mali_pp_scale_threshold[MALI_PP_THRESHOLD_30]< data->utilization_pp )
				currentStep = MALI_CLOCK_318;
			else
				currentStep = MALI_CLOCK_425;
			schedule_work(&wq_work);
		} else {
			disable_one_core();
		}
	}
	else
	{
		/* do nothing */
	}
}

void mali_pp_fs_scaling_update(struct mali_gpu_utilization_data *data)
{
	MALI_DEBUG_PRINT(3, ("Utilization: (%3d, %3d, %3d), cores enabled: %d/%d\n", data->utilization_gpu, data->utilization_gp, data->utilization_pp, num_cores_enabled, num_cores_total));
	MALI_DEBUG_PRINT(3, ("  %d   \n", currentStep));
	u32 utilization = data->utilization_gpu;

	if (utilization >= mali_dvfs_threshold[currentStep].upthreshold) {
		//printk("  > utilization:%d  currentStep:%d. upthreshold:%d.\n", utilization, currentStep, mali_dvfs_threshold[currentStep].upthreshold );
		if (utilization < mali_utilization_high && currentStep < max_mali_clock) 
			currentStep ++;
		else
			currentStep = max_mali_clock;

		if (data->utilization_pp > MALI_PP_THRESHOLD_90) { // 90%
			enable_max_num_cores();
		} else {
			enable_one_core();
		}
		schedule_work(&wq_work);
	} else if (utilization < mali_dvfs_threshold[currentStep].downthreshold && currentStep > min_mali_clock) {
		//printk(" <  utilization:%d  currentStep:%d. downthreshold:%d.\n", utilization, currentStep,mali_dvfs_threshold[currentStep].downthreshold );
		currentStep--;
		MALI_DEBUG_PRINT(2, ("Mali clock set %d..\n",currentStep));
		schedule_work(&wq_work);
	} else {
		//printk(" else utilization:%d  currentStep:%d. downthreshold:%d.\n", utilization, currentStep,mali_dvfs_threshold[currentStep].downthreshold );
		if (data->utilization_pp < mali_pp_scale_threshold[MALI_PP_THRESHOLD_30])
			disable_one_core();
		return;
	}
}

void reset_mali_scaling_stat(void)
{
	printk(" ****** scaling mode reset to default.*****\n");
	currentStep = max_mali_clock;
	enable_max_num_cores();
	schedule_work(&wq_work);
}

void mali_fs_scaling_update(struct mali_gpu_utilization_data *data)
{
	u32 utilization = data->utilization_gpu;

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
	MALI_DEBUG_ASSERT(idx < MALI_CLOCK_INDX_MAX);
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

u32 get_max_pp_num(void)
{
	return num_cores_total;	
}
u32 set_max_pp_num(u32 num)
{
	if (num > MALI_PP_NUMBER || num < min_pp_num )
		return -1;
	num_cores_total = num;
	if (num_cores_enabled > num_cores_total) {
		num_cores_enabled = num_cores_total;
		schedule_work(&wq_work);
	}
	
	return 0;	
}

u32 get_min_pp_num(void)
{
	return min_pp_num;	
}
u32 set_min_pp_num(u32 num)
{
	if (num > num_cores_total)
		return -1;
	min_pp_num = num;
	if (num_cores_enabled < min_pp_num) {
		num_cores_enabled = min_pp_num;
		schedule_work(&wq_work);
	}
	
	return 0;	
}

u32 get_max_mali_freq(void)
{
	return max_mali_clock;	
}
u32 set_max_mali_freq(u32 idx)
{
	if (idx >= MALI_CLOCK_INDX_MAX || idx < min_mali_clock )
		return -1;
	max_mali_clock = idx;
	if (currentStep > max_mali_clock) {
		currentStep = max_mali_clock;
		schedule_work(&wq_work);
	}
	
	return 0;	
}

u32 get_min_mali_freq(void)
{
	return min_mali_clock;	
}
u32 set_min_mali_freq(u32 idx)
{
	if (idx > max_mali_clock)
		return -1;
	min_mali_clock = idx;
	if (currentStep < min_mali_clock) {
		currentStep = min_mali_clock;
		schedule_work(&wq_work);
	}
	
	return 0;	
}



