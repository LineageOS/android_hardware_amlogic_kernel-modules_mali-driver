#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#include <mach/register.h>
#include <mach/irqs.h>
#include <linux/io.h>
#include <mach/io.h>
#include <plat/io.h>
#include <asm/io.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"

#include "arm_core_scaling.h"
#include "mali_clock.h"

static DEFINE_SPINLOCK(lock);

/* fclk is 2550Mhz. */
#define FCLK_DEV3 (6 << 9)		/*	850   Mhz  */
#define FCLK_DEV4 (5 << 9)		/*	637.5 Mhz  */
#define FCLK_DEV5 (7 << 9)		/*	510   Mhz  */
#define FCLK_DEV7 (4 << 9)		/*	364.3 Mhz  */

unsigned int mali_dvfs_clk[MALI_DVFS_STEPS] = {
                FCLK_DEV7 | 3,     /* 91  Mhz */
                FCLK_DEV7 | 1,     /* 182.1 Mhz */
                FCLK_DEV4 | 1,     /* 318.7 Mhz */
                FCLK_DEV3 | 1,     /* 425 Mhz */
                FCLK_DEV4 | 0,     /* 637.5 Mhz */
};

mali_dvfs_threshold_table mali_dvfs_threshold[MALI_DVFS_STEPS]={
        {0, 180}, 	//0%-70%
        {154, 230},	//%60-90%
        {218, 230},	//%85-%90
        {218, 230},	//%85-%90
        {243, 256}  //%95-%100
};

int top_clk_limit = 4;

void set_mali_clock(unsigned int utilization_pp)
{
	static int currentStep = MALI_DVFS_STEPS;
	static int lastStep = MALI_DVFS_STEPS;

	if (utilization_pp > mali_dvfs_threshold[currentStep].upthreshold) {
		currentStep = MALI_DVFS_STEPS - 1;
	} else if (utilization_pp < mali_dvfs_threshold[currentStep].downthreshold && currentStep > 0) {
		currentStep--;
	} else {
		return;
	}

	if (currentStep != lastStep) {
		int current_indx;

		if (currentStep > top_clk_limit && top_clk_limit >= 0 && top_clk_limit < MALI_DVFS_STEPS) {
			current_indx = top_clk_limit;
		} else {
			current_indx = currentStep;
		}
		mali_clock_set (current_indx);
		lastStep = currentStep;
	}
}

int inline mali_clock_set(int index) {
	unsigned long flags;
	
	spin_lock_irqsave(&lock, flags);
	clrbits_le32(P_HHI_MALI_CLK_CNTL, 1 << 8);
	writel(mali_dvfs_clk[index], P_HHI_MALI_CLK_CNTL); /* set clock to 333MHZ.*/
	readl(P_HHI_MALI_CLK_CNTL);
	setbits_le32(P_HHI_MALI_CLK_CNTL, 1 << 8);
	spin_unlock_irqrestore(&lock, flags);
}
