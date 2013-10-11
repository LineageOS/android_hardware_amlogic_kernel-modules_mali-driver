#ifndef _MALI_CLOCK_H_
#define _MALI_CLOCK_H_

typedef struct mali_dvfs_Tag{
	unsigned int step;
	unsigned int mali_clk;
}mali_dvfs_table;

typedef struct mali_dvfs_thresholdTag{
	unsigned int downthreshold;
	unsigned int upthreshold;
}mali_dvfs_threshold_table;

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

extern unsigned int mali_default_clock_step;
extern unsigned int mali_dvfs_clk[];
extern unsigned int min_mali_clock_index;
extern unsigned int max_mali_clock_index;

extern int mali_clock_set(unsigned int index);
extern void disable_clock(void);
extern void enable_clock(void);

#endif /* _MALI_CLOCK_H_ */
