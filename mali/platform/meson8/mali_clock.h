#ifndef _MALI_CLOCK_H_
#define _MALI_CLOCK_H_

enum mali_clock_rate {
	MALI_CLOCK_91,
	MALI_CLOCK_182,
	MALI_CLOCK_318,
	MALI_CLOCK_425,
	MALI_CLOCK_637,
	
	MALI_CLOCK_INDX_MAX
};

#define MALI_DVFS_STEPS 5

typedef struct mali_dvfs_Tag{
	unsigned int step;
	unsigned int mali_clk;
}mali_dvfs_table;

typedef struct mali_dvfs_thresholdTag{
	unsigned int downthreshold;
	unsigned int upthreshold;
}mali_dvfs_threshold_table;

void set_mali_clock(unsigned int utilization_pp);

extern unsigned int mali_clock_table;
extern int mali_clock_set(int index);

#endif /* _MALI_CLOCK_H_ */
