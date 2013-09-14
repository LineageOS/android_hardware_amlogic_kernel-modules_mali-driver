#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#include <mach/register.h>
#include <mach/irqs.h>
#include <linux/io.h>
#include <mach/io.h>
#include <plat/io.h>
#include <asm/io.h>
#include "mali_clock.h"

unsigned int mali_default_clock_step = MALI_CLOCK_637;

int mali_clock_set(unsigned int  clock) {
	clrbits_le32(P_HHI_MALI_CLK_CNTL, 1 << 8);
	clrbits_le32(P_HHI_MALI_CLK_CNTL, (0x7F | (0x7 << 9)));
	writel(clock, P_HHI_MALI_CLK_CNTL | (1 << 8)); /* set clock to 333MHZ.*/
	setbits_le32(P_HHI_MALI_CLK_CNTL, 1 << 8);
}
