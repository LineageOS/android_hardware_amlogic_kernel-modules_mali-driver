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

unsigned int mali_default_clock_step = MALI_CLOCK_318;// MALI_CLOCK_318;

static DEFINE_SPINLOCK(lock);

int mali_clock_set(unsigned int  clock) {
	clrbits_le32(P_HHI_MALI_CLK_CNTL, 1 << 8);
	clrbits_le32(P_HHI_MALI_CLK_CNTL, (0x7F | (0x7 << 9)));
	writel(clock, P_HHI_MALI_CLK_CNTL | (1 << 8)); /* set clock to 333MHZ.*/
	setbits_le32(P_HHI_MALI_CLK_CNTL, 1 << 8);
}

void disable_clock(void)
{
	unsigned long flags;
	spin_lock_irqsave(&lock, flags);
	clrbits_le32(P_HHI_MALI_CLK_CNTL, 1 << 8);
	spin_unlock_irqrestore(&lock, flags);
}

u32 mali_clock_test(void)
{
	unsigned int clk_mux = 35;
    unsigned int regval = 0;
    /// Set the measurement gate to 64uS
    clrsetbits_le32(P_MSR_CLK_REG0,0xffff,121);///122us
    
    // Disable continuous measurement
    // disable interrupts
    clrsetbits_le32(P_MSR_CLK_REG0,
        ((1 << 18) | (1 << 17)|(0x1f << 20)),///clrbits
        (clk_mux << 20) |                    /// Select MUX
        (1 << 19) |                          /// enable the clock
        (1 << 16));
    // Wait for the measurement to be done
    regval = readl(P_MSR_CLK_REG0);
    do {
        regval = readl(P_MSR_CLK_REG0);
    } while (regval & (1 << 31));

    // disable measuring
    clrbits_le32(P_MSR_CLK_REG0, (1 << 16));
    regval = (readl(P_MSR_CLK_REG2)) & 0x000FFFFF;
    regval += (regval/10000) * 6;
    // Return value in MHz*measured_val
    return (regval << 13);
}


void enable_clock(void)
{
	unsigned long flags;
	spin_lock_irqsave(&lock, flags);
	setbits_le32(P_HHI_MALI_CLK_CNTL, 1 << 8);
	spin_unlock_irqrestore(&lock, flags);
}
