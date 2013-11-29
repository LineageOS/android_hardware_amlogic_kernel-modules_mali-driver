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
//#include "mali_osk.h"

#define MALI_CLOCK_CTL  ((u32)(P_HHI_MALI_CLK_CNTL))

//static _mali_osk_lock_t *mali_clock_locker = NULL;
static u32 mali_default_clock_step = 0;

static u32 mali_dvfs_clk[] = {
                FCLK_DEV7 | 1,     /* 182.1 Mhz */
                FCLK_DEV4 | 1,     /* 318.7 Mhz */
                FCLK_DEV3 | 1,     /* 425 Mhz */
                FCLK_DEV5 | 0,     /* 510 Mhz */
                FCLK_DEV4 | 0,     /* 637.5 Mhz */
};

static u32 mali_dvfs_clk_sample[] = {
				182,     /* 182.1 Mhz */
				319,     /* 318.7 Mhz */
				425,     /* 425 Mhz */
				510,     /* 510 Mhz */
				637,     /* 637.5 Mhz */
};

void mali_clock_lock(void)
{
	//_mali_osk_lock_wait(mali_clock_locker, _MALI_OSK_LOCKMODE_RW);
}

void mali_clock_unlock(void)
{
	//_mali_osk_lock_signal(mali_clock_locker, _MALI_OSK_LOCKMODE_RW);
}

int mali_clock_init(u32 def_clk_idx)
{
	mali_default_clock_step = def_clk_idx;
	mali_clock_set(mali_default_clock_step);
	//mali_clock_locker = _mali_osk_lock_init(_MALI_OSK_LOCKFLAG_ORDERED | _MALI_OSK_LOCKFLAG_SPINLOCK | _MALI_OSK_LOCKFLAG_NONINTERRUPTABLE, 0, _MALI_OSK_LOCK_ORDER_PM_EXECUTE);
	//if (NULL == mali_clock_locker)
		//return _MALI_OSK_ERR_NOMEM;
	return 0;
}

void mali_clock_term(void)
{
	//if (mali_clock_locker)
		//_mali_osk_lock_term(mali_clock_locker);
	//mali_clock_locker = NULL;
}

u32 get_mali_default_clock_idx(void)
{
	return mali_default_clock_step;
}
void set_mali_default_clock_idx(u32 idx)
{
	mali_default_clock_step = idx;
}

int mali_clock_set(unsigned int  idx) {
	clrbits_le32(MALI_CLOCK_CTL, 1 << 8);
	clrbits_le32(MALI_CLOCK_CTL, (0x7F | (0x7 << 9)));
	writel(mali_dvfs_clk[idx], MALI_CLOCK_CTL); /* set clock to 333MHZ.*/
	setbits_le32(MALI_CLOCK_CTL, 1 << 8);
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

void try_open_clock(void)
{
	if ((readl(MALI_CLOCK_CTL) & (1 << 8)) == 0 ) {
		setbits_le32(P_HHI_MALI_CLK_CNTL, 1 << 8);
	}
}

void disable_clock(void)
{
	clrbits_le32(MALI_CLOCK_CTL, 1 << 8);
}


void enable_clock(void)
{
	u32 ret = 0;
	setbits_le32(MALI_CLOCK_CTL, 1 << 8);
	ret = readl(MALI_CLOCK_CTL) & (1 << 8);
}

u32 get_clock_state(void)
{
	u32 ret = 0;
	//mali_clock_lock();
	ret = readl(MALI_CLOCK_CTL) & (1 << 8);
	//mali_clock_unlock();
	return ret;
}

u32 get_mali_reg(void)
{
	return readl(MALI_CLOCK_CTL);
}

u32 get_mali_freq(u32 idx)
{
	return mali_dvfs_clk_sample[idx];
}
