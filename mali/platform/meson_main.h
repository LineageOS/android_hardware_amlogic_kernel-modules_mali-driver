/*
 * mali_platform.h
 *
 *  Created on: Nov 8, 2013
 *      Author: amlogic
 */

#ifndef MESON_MAIN_H_
#define MESON_MAIN_H_
#include <linux/kernel.h>
#include <linux/platform_device.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <mach/cpu.h>

#include "mali_scaling.h"
#include "mali_clock.h"

extern struct device_type mali_pm_device;
extern u32 mali_dvfs_clk[];
extern u32 mali_dvfs_clk_sample[];
extern u32 mali_clock_turbo_index;
extern u32 mali_default_clock_idx;
extern u32 mali_up_clock_idx;
extern u32 set_max_mali_freq(u32 idx);
extern u32 get_max_mali_freq(void);
extern u32 get_mali_tbl_size(void);
extern int mali_pm_statue;

int mali_meson_init_start(struct platform_device* ptr_plt_dev);
int mali_meson_init_finish(struct platform_device* ptr_plt_dev);
int mali_meson_uninit(struct platform_device* ptr_plt_dev);
int mali_light_suspend(struct device *device);
int mali_light_resume(struct device *device);
int mali_deep_suspend(struct device *device);
int mali_deep_resume(struct device *device);

#endif /* MESON_MAIN_H_ */
