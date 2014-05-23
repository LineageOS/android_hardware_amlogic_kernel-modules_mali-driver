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
#include "common/mali_osk_profiling.h"
#include "common/mali_kernel_utilization.h"
#include "common/mali_pp_scheduler.h"

#include <meson_main.h>

#define MALI_TABLE_SIZE 6

#define LOG_MALI_SCALING 0
#define LOG_SCALING_CHANGE 0
#if LOG_SCALING_CHANGE
# define TRACE_STAY() MALI_DEBUG_PRINT(2, ("[SCALING]stay_count:%d\n", stay_count));
#else
# define TRACE_STAY()
#endif

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

enum enum_threshold_t {
	THRESHOLD_20,
	THRESHOLD_30,
	THRESHOLD_40,
	THRESHOLD_50,
	THRESHOLD_60,
	THRESHOLD_80,
	THRESHOLD_90,
	THRESHOLD_MAX,
};
static u32 mali_threshold [] = {
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
		{ 0, 0, 5, 0  , 180}, /* for 255  */
		{ 1, 1, 5, 152, 205}, /* for 364  */
		{ 2, 2, 5, 180, 212}, /* for 425  */
		{ 3, 3, 5, 205, 236}, /* for 510  */
		{ 4, 4, 5, 230, 256}, /* for 637  */
		{ 0, 0, 5,   0,   0}
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

	if (mali_threshold[THRESHOLD_90] < data->utilization_pp)
	{
		ret = enable_max_num_cores();
	}
	else if (mali_threshold[THRESHOLD_50]< data->utilization_pp)
	{
		ret = enable_one_core();
	}
	else if (mali_threshold[THRESHOLD_40]< data->utilization_pp)
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

#if LOG_MALI_SCALING
void trace_utilization(struct mali_gpu_utilization_data *data, u32 current_idx,
			u32 next, u32 count)
{
	char direction;
	if (next > current_idx)
		direction = '>';
	else if ((current_idx > min_mali_clock) && (current_idx < next))
		direction = '<';
	else
		direction = '~';
	
	if (count == 0) {
		MALI_DEBUG_PRINT(2, ("[SCALING]%c (%1d-->%1d)@%d{%3d - %3d}. pp:%d\n",
					direction,
					current_idx,
					next,
					data->utilization_gpu,
					mali_dvfs_threshold[lastStep].downthreshold,
					mali_dvfs_threshold[lastStep].upthreshold,
					num_cores_enabled));
	}
}
#endif

static void mali_decide_next_status(struct mali_gpu_utilization_data *data, int* next_fs_idx,
								int* pp_change_flag)
{
	u32 utilization, mali_up_limit, decided_fs_idx;
	u32 ld_left, ld_right;

	utilization = data->utilization_gpu;
	mali_up_limit = scaling_mode ==  MALI_TURBO_MODE? mali_clock_turbo_index : max_mali_clock;
	decided_fs_idx = currentStep;
	*pp_change_flag = 0;

	if (utilization >= mali_dvfs_threshold[currentStep].upthreshold) {
		if (utilization < mali_threshold[THRESHOLD_80] && currentStep < mali_up_limit)
			decided_fs_idx++;
		else
			decided_fs_idx = mali_up_limit;

		*pp_change_flag = 1;
	} else if ((utilization < mali_dvfs_threshold[currentStep].downthreshold) &&
						(currentStep > min_mali_clock)) {
		decided_fs_idx--;
	} else if (num_cores_enabled > 1) {
		ld_left = data->utilization_pp * num_cores_enabled;
		ld_right = (mali_dvfs_threshold[currentStep].upthreshold) * (num_cores_enabled - 1);

		if ((ld_left < ld_right) && (num_cores_enabled > min_pp_num))
			*pp_change_flag = -1;
	}
	*next_fs_idx = decided_fs_idx;
}

void mali_pp_fs_scaling_update(struct mali_gpu_utilization_data *data)
{
	static int stay_count = 0;
	int ret = 0;
	int pp_change_flag = 0;
	u32 next_idx = 0;

	mali_decide_next_status(data, &next_idx, &pp_change_flag);

	if (pp_change_flag == 1)
		ret = enable_max_num_cores();

	if (next_idx > currentStep) {
		ret = 1;
		currentStep = next_idx;
		stay_count = mali_dvfs_threshold[currentStep].keep_count;
	}

#if LOG_MALI_SCALING
	trace_utilization(data, currentStep, next_idx, stay_count);
#endif

	if((next_idx <= currentStep) || (pp_change_flag == -1)) {
		if (stay_count  == 0) {
			if (pp_change_flag == -1) {
				ret = disable_one_core();
				stay_count = mali_dvfs_threshold[currentStep].keep_count;
			}

			if (next_idx < currentStep) {
				ret = 1;
				currentStep = next_idx;
				stay_count = mali_dvfs_threshold[next_idx].keep_count;
			}
		} else {
			stay_count--;
		}
	}

	TRACE_STAY()
	if (ret == 1)
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
	int ret1, ret2 = 0;
#if 0
	printk(" aml mali test*************\n");
#endif
	if (currentStep < mali_default_clock_idx) {
		ret2 = 1;
		currentStep = mali_default_clock_idx;
	}
	ret1 = enable_max_num_cores();
	if (ret1 || ret2)
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
