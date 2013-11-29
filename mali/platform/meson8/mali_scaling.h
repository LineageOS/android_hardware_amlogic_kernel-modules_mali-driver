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

struct mali_gpu_utilization_data;

typedef struct mali_dvfs_threshold_table {
	unsigned int freq_index;
	unsigned int voltage;
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
void mali_core_scaling_init(void);

/**
 * Terminate core scaling policy.
 */
void mali_core_scaling_term(void);

/**
 * Update core scaling policy with new utilization data.
 *
 * @param data Utilization data.
 */

void mali_pp_fs_scaling_update(struct mali_gpu_utilization_data *data);
void mali_pp_scaling_update(struct mali_gpu_utilization_data *data);
void mali_fs_scaling_update(struct mali_gpu_utilization_data *data);
void mali_pp_scaling_staging(struct mali_gpu_utilization_data *data);
void reset_mali_scaling_stat(void);
void flush_scaling_job(void);
u32 get_current_frequency(void);
void set_turbo_mode(u32 mode);
u32 set_mali_dvfs_tbl_size(u32 size);
u32 get_max_dvfs_tbl_size(void);
uint32_t* get_mali_dvfs_tbl_addr(void);

#endif /* __ARM_CORE_SCALING_H__ */
