/*
 * mali_kbase_runtime_pm.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

//#define DEBUG
#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <backend/gpu/mali_kbase_device_internal.h>
#include "mali_kbase_config_platform.h"
#include "mali_scaling.h"

static void enable_gpu_power_control(struct kbase_device *kbdev)
{
}

static void disable_gpu_power_control(struct kbase_device *kbdev)
{
}

void *reg_base_reset = NULL;
static int first = 1;

//remove this if sc2 worked fine.
//g12a
#define RESET0_MASK    0x10
#define RESET1_MASK    0x11
#define RESET2_MASK    0x12
#define RESET0_LEVEL   0x20
#define RESET1_LEVEL   0x21
#define RESET2_LEVEL   0x22
//sc2
#define RESETCTRL_RESET1_LEVEL  0x11
#define RESETCTRL_RESET1_MASK   0x21

#define Rd(r)                           readl((reg_base_reset) + ((r)<<2))
#define Wr(r, v)                        writel((v), ((reg_base_reset) + ((r)<<2)))
#define Mali_WrReg(regnum, value)       writel((value), kbdev->reg + (regnum))
#define Mali_RdReg(regnum)              readl(kbdev->reg + (regnum))

extern u64 kbase_pm_get_ready_cores(struct kbase_device *kbdev, enum kbase_pm_core_type type);

//[0]:CG [1]:SC0 [2]:SC2
static void  Mali_pwr_on_with_kdev( struct kbase_device *kbdev, uint32_t  mask)
{
	uint32_t    part1_done;
	uint32_t    shader_present;
	uint32_t    tiler_present;
	uint32_t    l2_present;

	part1_done = 0;
	Mali_WrReg(0x0000024, 0xffffffff); // clear interrupts

	shader_present = Mali_RdReg(0x100);
	tiler_present  = Mali_RdReg(0x110);
	l2_present     = Mali_RdReg(0x120);
	dev_info(kbdev->dev, "shader_present=%d, tiler_present=%d, l2_present=%d\n",
	shader_present, tiler_present, l2_present);

	if (  mask == 0 ) {
		Mali_WrReg(0x00000180, 0xffffffff);   // Power on all cores (shader low)
		Mali_WrReg(0x00000184, 0xffffffff);   // Power on all cores (shader high)
		Mali_WrReg(0x00000190, 0xffffffff);   // Power on all cores (tiler low)
		Mali_WrReg(0x00000194, 0xffffffff);   // Power on all cores (tiler high)
		Mali_WrReg(0x000001a0, 0xffffffff);   // Power on all cores (l2 low)
		Mali_WrReg(0x000001a4, 0xffffffff);   // Power on all cores (l2 high)
	} else {
		Mali_WrReg(0x00000180, mask);    // Power on all cores (shader low)
		Mali_WrReg(0x00000184, 0);       // Power on all cores (shader high)
		Mali_WrReg(0x00000190, mask);    // Power on all cores (tiler low)
		Mali_WrReg(0x00000194, 0);       // Power on all cores (tiler high)
		Mali_WrReg(0x000001a0, mask);    // Power on all cores (l2 low)
		Mali_WrReg(0x000001a4, 0);       // Power on all cores (l2 high)
	}

	part1_done = Mali_RdReg(0x0000020);
	while (0 == part1_done) { part1_done = Mali_RdReg(0x00000020); }
	Mali_WrReg(0x0000024, 0xffffffff); // clear interrupts
}

/*reset function before t7 by register*/
static void mali_reset_v0(int reset_g12a)
{
	u32 value;

	if (reset_g12a) {
		value = Rd(RESET0_MASK);
		value = value & (~(0x1<<20));
		Wr(RESET0_MASK, value);

		value = Rd(RESET0_LEVEL);
		value = value & (~(0x1<<20));
		Wr(RESET0_LEVEL, value);

		value = Rd(RESET2_MASK);
		value = value & (~(0x1<<14));
		Wr(RESET2_MASK, value);

		value = Rd(RESET2_LEVEL);
		value = value & (~(0x1<<14));
		Wr(RESET2_LEVEL, value);

		value = Rd(RESET0_LEVEL);
		value = value | ((0x1<<20));
		Wr(RESET0_LEVEL, value);

		value = Rd(RESET2_LEVEL);
		value = value | ((0x1<<14));
		Wr(RESET2_LEVEL, value);
	} else {
		/* JOHNT: remove this if sc2 worked fine. */
		value = ~(1 << 2);
		Wr(RESETCTRL_RESET1_MASK, value);
		Wr(RESETCTRL_RESET1_LEVEL, value);
		Wr(RESETCTRL_RESET1_LEVEL, 0xFFFFFFFF);
	}
}

/*
 * reset note:
 * there have mali reset and mali_capb bus reset.
 * reset step:[vlsi suggestion]
 * step1. mali-reset and mali-capb-reset level and mask config to 0
 * step2. mali-reset and mali-capb-reset level config to 1
 * Todo: sync old ic similar with t7,reset reg config by dts
 */
static void mali_reset(void)
{
	mali_plat_info_t* pmali_plat = get_mali_plat_data();
	u32 value = 0;
	int reset_g12a = pmali_plat->reset_g12a;

	if (!pmali_plat->reset_flag) {
		mali_reset_v0(reset_g12a);
		return;
	}

	/*
	 * for t7 , if old ic also sync with the follow method,
	 * need sync the dts similar with T7
	 */
	value = Rd(pmali_plat->module_reset.reg_mask);
	value = value & (~(0x1<<pmali_plat->module_reset.reg_bit));
	Wr(pmali_plat->module_reset.reg_mask, value);

	value = Rd(pmali_plat->module_reset.reg_level);
	value = value & (~(0x1<<pmali_plat->module_reset.reg_bit));
	Wr(pmali_plat->module_reset.reg_level, value);

	value = Rd(pmali_plat->apb_reset.reg_mask);
	value = value & (~(0x1<<pmali_plat->apb_reset.reg_bit));
	Wr(pmali_plat->apb_reset.reg_mask, value);

	value = Rd(pmali_plat->apb_reset.reg_level);
	value = value & (~(0x1<<pmali_plat->apb_reset.reg_bit));
	Wr(pmali_plat->apb_reset.reg_level, value);

	value = Rd(pmali_plat->apb_reset.reg_level);
	value = value | ((0x1<<pmali_plat->apb_reset.reg_bit));
	Wr(pmali_plat->apb_reset.reg_level, value);

	value = Rd(pmali_plat->module_reset.reg_level);
	value = value | ((0x1<<pmali_plat->module_reset.reg_bit));
	Wr(pmali_plat->module_reset.reg_level, value);
}

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	int ret = 1; /* Assume GPU has been powered off */
	int error;

	/* remove this if sc2 worked fine. */
	struct mali_plat_info_t *mpdata  = (struct mali_plat_info_t *) kbdev->platform_context;
	reg_base_reset = mpdata->reg_base_reset;


	if (first == 0) goto out;
	pm_runtime_enable(kbdev->dev);
	error = pm_runtime_get_sync(kbdev->dev);
	if (error == 1) {
		/*
		 * Let core know that the chip has not been
		 * powered off, so we can save on re-initialization.
		 */
		ret = 0;
	}
	udelay(100);
	dev_info(kbdev->dev, "pm_runtime_get_sync returned %d\n", error);

	first = 0;

	mali_reset();
	udelay(10); // OR POLL for reset done
	dev_dbg(kbdev->dev, "delay 10us\n");

	Mali_WrReg(GPU_CONTROL_REG(PWR_KEY), 0x2968A819);
	Mali_WrReg(GPU_CONTROL_REG(PWR_OVERRIDE1), 0xfff | (0x20<<16));
	Mali_pwr_on_with_kdev(kbdev, 0x1);
	dev_dbg(kbdev->dev, "pm_callback_power_on %p\n",
			(void *)kbdev->dev->pm_domain);

	enable_gpu_power_control(kbdev);
out:
	return ret;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "pm_callback_power_off\n");
	/*
	 * the pm_runtime is no use for ic before t7,
	 * todo: add dynamic switch power on/off for t7 with external top gpu power domain
	 */
	//pm_runtime_mark_last_busy(kbdev->dev);
	//pm_runtime_put_autosuspend(kbdev->dev);

#ifndef KBASE_PM_RUNTIME
	//disable_gpu_power_control(kbdev);
#endif
}

#ifdef KBASE_PM_RUNTIME
static int kbase_device_runtime_init(struct kbase_device *kbdev)
{
	int ret = 0;

	pm_runtime_set_autosuspend_delay(kbdev->dev, AUTO_SUSPEND_DELAY);
	pm_runtime_use_autosuspend(kbdev->dev);
	pm_runtime_set_active(kbdev->dev);
	dev_dbg(kbdev->dev, "kbase_device_runtime_init\n");
	pm_runtime_enable(kbdev->dev);

	if (!pm_runtime_enabled(kbdev->dev)) {
		dev_warn(kbdev->dev, "pm_runtime not enabled");
		ret = -ENOSYS;
	}

	return ret;
}

static void kbase_device_runtime_disable(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "kbase_device_runtime_disable\n");
	pm_runtime_disable(kbdev->dev);
}
#endif

static int pm_callback_runtime_on(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "pm_callback_runtime_on\n");

	enable_gpu_power_control(kbdev);
	return 0;
}

static void pm_callback_runtime_off(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "pm_callback_runtime_off\n");

	disable_gpu_power_control(kbdev);
}

static void pm_callback_resume(struct kbase_device *kbdev)
{
	int ret = pm_callback_runtime_on(kbdev);

	WARN_ON(ret);
}

static void pm_callback_suspend(struct kbase_device *kbdev)
{
	pm_callback_runtime_off(kbdev);
}

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback = pm_callback_suspend,
	.power_resume_callback = pm_callback_resume,
#ifdef KBASE_PM_RUNTIME
	.power_runtime_init_callback = kbase_device_runtime_init,
	.power_runtime_term_callback = kbase_device_runtime_disable,
	.power_runtime_on_callback = pm_callback_runtime_on,
	.power_runtime_off_callback = pm_callback_runtime_off,
#else				/* KBASE_PM_RUNTIME */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#endif				/* KBASE_PM_RUNTIME */
};
