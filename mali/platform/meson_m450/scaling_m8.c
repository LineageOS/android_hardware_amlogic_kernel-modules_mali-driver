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
#include <mach/smp.h>
#include "mali_kernel_common.h"
#include "common/mali_osk_profiling.h"
#include "common/mali_kernel_utilization.h"
#include "common/mali_pp_scheduler.h"

#include "meson_main.h"

#define MALI_TABLE_SIZE 6

static int num_cores_total;
static int num_cores_enabled;
static int currentStep;
static int lastStep;
static struct work_struct wq_work;

unsigned int min_mali_clock = 0;
unsigned int max_mali_clock = 3;
unsigned int min_pp_num = 1;

/* Configure dvfs mode */
enum mali_scale_mode_t {
	MALI_PP_SCALING = 0,
	MALI_PP_FS_SCALING,
	MALI_SCALING_DISABLE,
	MALI_TURBO_MODE,
	MALI_SCALING_MODE_MAX
};


static int  scaling_mode = MALI_PP_FS_SCALING;

enum mali_pp_scale_threshold_t {
	MALI_PP_THRESHOLD_20,
	MALI_PP_THRESHOLD_30,
	MALI_PP_THRESHOLD_40,
	MALI_PP_THRESHOLD_50,
	MALI_PP_THRESHOLD_60,
	MALI_PP_THRESHOLD_80,
	MALI_PP_THRESHOLD_90,
	MALI_PP_THRESHOLD_MAX,
};
static u32 mali_pp_scale_threshold [] = {
	51,  /* 20% */
	77,  /* 30% */
	102, /* 40% */
	128, /* 50% */
	154, /* 60% */
	205, /* 80% */
	230, /* 90% */
};


static u32 mali_dvfs_table_size = MALI_TABLE_SIZE;

static struct mali_dvfs_threshold_table mali_dvfs_threshold[MALI_TABLE_SIZE]={
		{ 0, 0, 2, 0  , 200}, /* for 182.1  */
		{ 1, 1, 2, 152, 205}, /* for 318.7  */
		{ 2, 2, 2, 180, 212}, /* for 425.0  */
		{ 3, 3, 2, 205, 236}, /* for 510.0  */
		{ 4, 4, 2, 230, 256},  /* for 637.5  */
		{ 0, 0, 2,   0,   0}
};

u32 set_mali_dvfs_tbl_size(u32 size)
{
	if (size <= 0 && size > MALI_TABLE_SIZE) return -1;
	mali_dvfs_table_size = size;
	return 0;
}

u32 get_max_dvfs_tbl_size(void)
{
	return MALI_TABLE_SIZE;
}

uint32_t* get_mali_dvfs_tbl_addr(void)
{
	return (uint32_t*)mali_dvfs_threshold;
}

static void do_scaling(struct work_struct *work)
{
	int err = mali_perf_set_num_pp_cores(num_cores_enabled);
	MALI_DEBUG_ASSERT(0 == err);
	MALI_IGNORE(err);
	if (mali_dvfs_threshold[currentStep].freq_index != mali_dvfs_threshold[lastStep].freq_index) {
		mali_dev_pause();
		mali_clock_set(mali_dvfs_threshold[currentStep].freq_index);
		mali_dev_resume();
		lastStep = currentStep;
	}
#ifdef CONFIG_MALI400_PROFILING
	_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
					MALI_PROFILING_EVENT_CHANNEL_GPU |
					MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
					get_current_frequency(),
					0,	0,	0,	0);
#endif
}

void flush_scaling_job(void)
{
	cancel_work_sync(&wq_work);
}

static u32 enable_one_core(void)
{
	u32 ret = 0;
	if (num_cores_enabled < num_cores_total)
	{
		++num_cores_enabled;
		ret = 1;
		MALI_DEBUG_PRINT(3, ("Core scaling: Enabling one more core\n"));
	}

	MALI_DEBUG_ASSERT(              1 <= num_cores_enabled);
	MALI_DEBUG_ASSERT(num_cores_total >= num_cores_enabled);
	return ret;
}

static u32 disable_one_core(void)
{
	u32 ret = 0;
	if (min_pp_num < num_cores_enabled)
	{
		--num_cores_enabled;
		ret = 1;
		MALI_DEBUG_PRINT(3, ("Core scaling: Disabling one core\n"));
	}

	MALI_DEBUG_ASSERT(min_pp_num <= num_cores_enabled);
	MALI_DEBUG_ASSERT(num_cores_total >= num_cores_enabled);
	return ret;
}

static u32 enable_max_num_cores(void)
{
	u32 ret = 0;
	if (num_cores_enabled < num_cores_total)
	{
		num_cores_enabled = num_cores_total;
		ret = 1;
		MALI_DEBUG_PRINT(3, ("Core scaling: Enabling maximum number of cores\n"));
	}

	MALI_DEBUG_ASSERT(num_cores_total == num_cores_enabled);
	return ret;
}

void mali_core_scaling_init(int pp, int clock_idx)
{
	INIT_WORK(&wq_work, do_scaling);

	num_cores_total   = pp;
	num_cores_enabled = num_cores_total;

	currentStep = clock_idx;
	lastStep = currentStep;
	/* NOTE: Mali is not fully initialized at this point. */
}

void mali_core_scaling_term(void)
{
	flush_scheduled_work();
}

void mali_pp_scaling_update(struct mali_gpu_utilization_data *data)
{
	int ret = 0;
	currentStep = mali_up_clock_idx;

	MALI_DEBUG_PRINT(3, ("Utilization: (%3d, %3d, %3d), cores enabled: %d/%d\n", data->utilization_gpu, data->utilization_gp, data->utilization_pp, num_cores_enabled, num_cores_total));

	/* NOTE: this function is normally called directly from the utilization callback which is in
	 * timer context. */

	if (mali_pp_scale_threshold[MALI_PP_THRESHOLD_90] < data->utilization_pp)
	{
		ret = enable_max_num_cores();
	}
	else if (mali_pp_scale_threshold[MALI_PP_THRESHOLD_50]< data->utilization_pp)
	{
		ret = enable_one_core();
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
		ret = disable_one_core();
	}
	else
	{
		/* do nothing */
	}
	if (ret == 1)
		schedule_work(&wq_work);
}

void mali_pp_fs_scaling_update(struct mali_gpu_utilization_data *data)
{
	u32 ret = 0;
	u32 utilization = data->utilization_gpu;
	//(data->utilization_pp < data->utilization_gp)?data->utilization_gp:data->utilization_pp;
	u32 loading_complete = (1<<16);//mali_utilization_bw_get_period();
	u32 mali_up_limit = (scaling_mode == MALI_TURBO_MODE) ? mali_clock_turbo_index : max_mali_clock;

	if (loading_complete > (2<<16) &&
			currentStep > min_mali_clock) {
		currentStep --;
		MALI_DEBUG_PRINT(3, (" active time vs command complete:%d\n", loading_complete));
		goto exit;
	}

	if (utilization >= mali_dvfs_threshold[currentStep].upthreshold) {
		if (utilization < mali_pp_scale_threshold[MALI_PP_THRESHOLD_80] && currentStep < mali_up_limit)
			currentStep ++;
		else
			currentStep = mali_up_limit;

		if (data->utilization_pp > MALI_PP_THRESHOLD_80) {
			enable_max_num_cores();
		} else {
			enable_one_core();
		}
		MALI_DEBUG_PRINT(3, ("  > utilization:%d  currentStep:%d.pp:%d. upthreshold:%d.\n",
					utilization, currentStep, num_cores_enabled, mali_dvfs_threshold[currentStep].upthreshold ));
	} else if (utilization < mali_dvfs_threshold[currentStep].downthreshold && currentStep > min_mali_clock) {
		currentStep--;
		MALI_DEBUG_PRINT(3, (" <  utilization:%d  currentStep:%d. downthreshold:%d.\n",
					utilization, currentStep,mali_dvfs_threshold[currentStep].downthreshold ));
	} else {
		if (data->utilization_pp < mali_pp_scale_threshold[MALI_PP_THRESHOLD_30])
			ret = disable_one_core();
		MALI_DEBUG_PRINT(3, (" <  utilization:%d  currentStep:%d. downthreshold:%d.pp:%d\n",
					utilization, currentStep,mali_dvfs_threshold[currentStep].downthreshold, num_cores_enabled));
	}

exit:
	if ((num_cores_enabled != num_cores_total) ||
			(mali_dvfs_threshold[currentStep].freq_index != mali_dvfs_threshold[lastStep].freq_index))
		schedule_work(&wq_work);
#ifdef CONFIG_MALI400_PROFILING
	else
		_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
						MALI_PROFILING_EVENT_CHANNEL_GPU |
						MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
						get_current_frequency(),
						0,	0,	0,	0);
#endif
}

static void reset_mali_scaling_stat(void)
{
	if (scaling_mode == MALI_TURBO_MODE)
		currentStep = mali_clock_turbo_index;
	else
		currentStep = max_mali_clock;
	enable_max_num_cores();
	schedule_work(&wq_work);
}

u32 get_max_pp_num(void)
{
	return num_cores_total;
}
u32 set_max_pp_num(u32 num)
{
	if (num < min_pp_num)
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
	if (idx >= mali_clock_turbo_index || idx < min_mali_clock )
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

void mali_plat_preheat(void)
{
	//printk(" aml mali test*************\n");
	int ret;
	ret = enable_max_num_cores();
	if (ret)
		schedule_work(&wq_work);
}

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data)
{
	switch (scaling_mode) {
	case MALI_PP_FS_SCALING:
		mali_pp_fs_scaling_update(data);
		break;
	case MALI_PP_SCALING:
		mali_pp_scaling_update(data);
		break;
	default:
		break;
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

u32 get_current_frequency(void)
{
	return get_mali_freq(currentStep);
}

