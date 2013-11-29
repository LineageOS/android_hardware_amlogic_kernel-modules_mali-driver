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
#include "mali_scaling.h"
#include "common/mali_osk_profiling.h"
#include "common/mali_kernel_utilization.h"
#include "common/mali_pp_scheduler.h"
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif /* CONFIG_PM_RUNTIME */

#include "mali_clock.h"

#define MALI_TABLE_SIZE 6

static int num_cores_total;
static int num_cores_enabled;
static int currentStep;
static int lastStep;
static struct work_struct wq_work;
static u32 mali_turbo_mode = 0;

//static u32 last_utilization_pp;
//static u32 last_utilization_gp;
//static u32 last_utilization_gp_pp;

unsigned int min_mali_clock = MALI_CLOCK_182;
unsigned int max_mali_clock = MALI_CLOCK_510;
unsigned int min_pp_num = 1;

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
		{ 0, 0, 0  , 200}, /* for 182.1  */
		{ 1, 1, 152, 205}, /* for 318.7  */
		{ 2, 2, 180, 212}, /* for 425.0  */
		{ 3, 3, 205, 236}, /* for 510.0  */
		{ 4, 4, 230, 256},  /* for 637.5  */
		{ 0, 0,   0,   0}
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
extern int try_exclu_cpu_exe2(exl_call_func_t func, void * p_arg);
static void do_scaling(struct work_struct *work)
{int err = 0;
	//int err = mali_perf_set_num_pp_cores(num_cores_enabled);
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

	MALI_DEBUG_ASSERT(              min_pp_num <= num_cores_enabled);
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

void mali_core_scaling_init(void)
{
	INIT_WORK(&wq_work, do_scaling);

	num_cores_total   = 4;
	num_cores_enabled = num_cores_total;
	
	currentStep = get_mali_default_clock_idx();
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
	currentStep = MALI_CLOCK_510;

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
		if (num_cores_enabled == min_pp_num) {
			if ( mali_pp_scale_threshold[MALI_PP_THRESHOLD_30]< data->utilization_pp )
				currentStep = MALI_CLOCK_318;
			else
				currentStep = MALI_CLOCK_425;
			ret = 1;
		} else {
			ret = disable_one_core();
		}
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
	u32 freq_for_mem_limit = (min_mali_clock > MALI_CLOCK_318)?min_mali_clock:MALI_CLOCK_318;
	u32 mali_up_limit = mali_turbo_mode ? MALI_CLOCK_637 : max_mali_clock;

	if (loading_complete > (2<<16) &&
			currentStep > freq_for_mem_limit) {
		currentStep --;
		MALI_DEBUG_PRINT(2, (" active time vs command complete:%d\n", loading_complete));
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
		MALI_DEBUG_PRINT(2, ("  > utilization:%d  currentStep:%d.pp:%d. upthreshold:%d.\n",
					utilization, currentStep, num_cores_enabled, mali_dvfs_threshold[currentStep].upthreshold ));
	} else if (utilization < mali_dvfs_threshold[currentStep].downthreshold && currentStep > min_mali_clock) {
		currentStep--;
		MALI_DEBUG_PRINT(2, (" <  utilization:%d  currentStep:%d. downthreshold:%d.\n",
					utilization, currentStep,mali_dvfs_threshold[currentStep].downthreshold ));
	} else {
		if (data->utilization_pp < mali_pp_scale_threshold[MALI_PP_THRESHOLD_30])
			ret = disable_one_core();
		MALI_DEBUG_PRINT(2, (" <  utilization:%d  currentStep:%d. downthreshold:%d.pp:%d\n",
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

unsigned int mali_fs_utilscale[] = {
                1821,
                3187,
                4250,
                5100,
                6375,
};

void mali_pp_scaling_staging(struct mali_gpu_utilization_data *data)
{
    int  scale_fs;
    int  scale_pp;
    int  scale_fs_pp;

    int  scale_mode; // 0:none 1:FS 2:PP 3:PP_FS

    u32 utilization = data->utilization_gpu;

    scale_mode = 0;
    scale_fs = -1;
    scale_pp = -1;
    scale_fs_pp = -1;
    if ( utilization < mali_pp_scale_threshold[MALI_PP_THRESHOLD_20] ) {
        if ( currentStep > min_mali_clock )
                scale_fs = mali_pp_scale_threshold[MALI_PP_THRESHOLD_60] - ((mali_fs_utilscale[currentStep]/mali_fs_utilscale[currentStep-1])*utilization);
        if ( num_cores_enabled > min_pp_num )
                scale_pp = mali_pp_scale_threshold[MALI_PP_THRESHOLD_60] - ((num_cores_enabled/(num_cores_enabled-1))*utilization);
        if ( (currentStep > min_mali_clock) && (num_cores_enabled > min_pp_num))
                scale_fs_pp = mali_pp_scale_threshold[MALI_PP_THRESHOLD_60] - ((mali_fs_utilscale[currentStep]/mali_fs_utilscale[currentStep-1])*(num_cores_enabled/(num_cores_enabled-1))*utilization);
    } else if ( utilization >  mali_pp_scale_threshold[MALI_PP_THRESHOLD_80]) {
        if ( currentStep < max_mali_clock )
                scale_fs = ((mali_fs_utilscale[currentStep]/mali_fs_utilscale[currentStep+1])*utilization) - mali_pp_scale_threshold[MALI_PP_THRESHOLD_40];
        if ( num_cores_enabled < num_cores_total )
                scale_pp = ((num_cores_enabled/(num_cores_enabled+1))*utilization) - mali_pp_scale_threshold[MALI_PP_THRESHOLD_40];
        if ((currentStep < max_mali_clock) && (num_cores_enabled < num_cores_total ) )
                scale_fs_pp = ((mali_fs_utilscale[currentStep]/mali_fs_utilscale[currentStep+1])*(num_cores_enabled/(num_cores_enabled+1))*utilization) -  mali_pp_scale_threshold[MALI_PP_THRESHOLD_40];
    }

    if ( scale_fs >= 0 )
            scale_mode = 1;
    if ( (scale_pp >= 0) && (scale_pp < scale_fs ) && (scale_fs >= 0))
            scale_mode = 2;
    if ( (scale_fs_pp >= 0 ) && (scale_fs_pp < scale_pp) && (scale_pp >= 0))
            scale_mode = 3;
    if ( (scale_fs_pp >= 0 ) && (scale_fs_pp <  scale_fs ) && (scale_fs >=0))
            scale_mode = 3;
    if ( utilization < mali_pp_scale_threshold[MALI_PP_THRESHOLD_20] ) {
        if ( (scale_mode == 1) || ( scale_mode == 3))
                    --currentStep;
        if ( (scale_mode == 2) || ( scale_mode == 3))
                    --num_cores_enabled;
    } else if ( utilization >  mali_pp_scale_threshold[MALI_PP_THRESHOLD_80]) {
        if ( (scale_mode == 1) || ( scale_mode == 3))
                    ++currentStep;
        if ( (scale_mode == 2) || ( scale_mode == 3))
                    ++num_cores_enabled;
    }

    printk(" mali_pp_fs_scaling_update:: > utilization:%d  currentStep:%d.pp:%d \n",
    						utilization, currentStep, num_cores_enabled);

    if ( scale_mode != 0 )
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

void reset_mali_scaling_stat(void)
{
	if (mali_turbo_mode)
		currentStep = MALI_CLOCK_637;
	else
		currentStep = max_mali_clock;
	enable_max_num_cores();
	schedule_work(&wq_work);
}

void set_mali_freq_idx(u32 idx)
{
	MALI_DEBUG_ASSERT(idx < MALI_CLOCK_INDX_MAX);
	currentStep = idx;
	lastStep = MALI_CLOCK_INDX_MAX;
	set_mali_default_clock_idx(idx);
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

u32 get_current_frequency(void) {
	return get_mali_freq(currentStep);
}

void set_turbo_mode(u32 mode) {
	mali_turbo_mode = mode;
}


