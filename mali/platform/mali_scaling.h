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
 * @file arm_core_scaling.h
 * Example core scaling policy.
 */

#ifndef __ARM_CORE_SCALING_H__
#define __ARM_CORE_SCALING_H__

typedef struct mali_dvfs_threshold_table {
	unsigned int freq_index;
	unsigned int voltage;
	unsigned int keep_count;
	unsigned int downthreshold;
	unsigned int upthreshold;
} mali_dvfs_threshold_table_t;

/**
 * Initialize core scaling policy.
 *
 * @note The core scaling policy will assume that all PP cores are on initially.
 *
 * @param num_pp_cores Total number of PP cores.
 */
void mali_core_scaling_init(int pp, int clock_idx);

/**
 * Terminate core scaling policy.
 */
void mali_core_scaling_term(void);

/**
 * Update core scaling policy with new utilization data.
 *
 * @param data Utilization data.
 */

/**
 * cancel and flush scaling job queue.
 */
void flush_scaling_job(void);

u32 set_mali_dvfs_tbl_size(u32 size);
u32 get_max_dvfs_tbl_size(void);
uint32_t* get_mali_dvfs_tbl_addr(void);

/* get or set the max/min number of pp cores. */
u32 get_max_pp_num(void);
u32 set_max_pp_num(u32 num);
u32 get_min_pp_num(void);
u32 set_min_pp_num(u32 num);

/* get or set the max/min frequency of GPU. */
u32 get_max_mali_freq(void);
u32 set_max_mali_freq(u32 idx);
u32 get_min_mali_freq(void);
u32 set_min_mali_freq(u32 idx);

/* get or set the scale mode. */
u32 get_mali_schel_mode(void);
void set_mali_schel_mode(u32 mode);

/* preheat of the GPU. */
void mali_plat_preheat(void);

/* for frequency reporter in DS-5 streamline. */
u32 get_current_frequency(void);
#endif /* __ARM_CORE_SCALING_H__ */
