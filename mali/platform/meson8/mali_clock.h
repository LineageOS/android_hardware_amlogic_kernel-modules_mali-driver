#ifndef _MALI_CLOCK_H_
#define _MALI_CLOCK_H_

/* fclk is 2550Mhz. */
#define FCLK_DEV3 (6 << 9)		/*	850   Mhz  */
#define FCLK_DEV4 (5 << 9)		/*	637.5 Mhz  */
#define FCLK_DEV5 (7 << 9)		/*	510   Mhz  */
#define FCLK_DEV7 (4 << 9)		/*	364.3 Mhz  */

enum mali_clock_rate {
//	MALI_CLOCK_91,
	MALI_CLOCK_182 = 0,
	MALI_CLOCK_318,
	MALI_CLOCK_425,
	MALI_CLOCK_510,
	MALI_CLOCK_637,
	
	MALI_CLOCK_INDX_MAX
};

extern unsigned int min_mali_clock_index;
extern unsigned int max_mali_clock_index;

int mali_clock_init(u32 def_clk_idx);
void mali_clock_term(void);
void mali_clock_lock(void);
void mali_clock_unlock(void);

int mali_clock_set(unsigned int index);
void disable_clock(void);
void enable_clock(void);

void try_open_clock(void);

u32 get_clock_state(void);
u32 get_mali_freq(u32 idx);
u32 get_mali_default_clock_idx(void);
void set_mali_default_clock_idx(u32 idx);
#endif /* _MALI_CLOCK_H_ */
