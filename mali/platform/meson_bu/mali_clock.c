#include <linux/platform_device.h>
#include "mali_scaling.h"
#include "mali_clock.h"
#if 1
#ifndef CLK_DVFS_TBL_SIZE
#define CLK_DVFS_TBL_SIZE 5
#endif
//static DEFINE_SPINLOCK(lock);
static mali_plat_info_t* pmali_plat = NULL;
//static u32 mali_extr_backup = 0;
//static u32 mali_extr_sample_backup = 0;

int mali_clock_init_clk_tree(struct platform_device* pdev)
{
	struct clk *clk_mali_0_parent;
	struct clk *clk_mali_0;
	struct clk *clk_mali;

	clk_mali = clk_get(&pdev->dev, "clk_mali");
	clk_mali_0 = clk_get(&pdev->dev, "clk_mali_0");
	clk_mali_0_parent = clk_get(&pdev->dev, "fclk_div4");

	if (!clk_mali) {
		printk("could not get clk_mali\n");
		return 0;
	}
	if (!clk_mali_0) {
		printk("could not get clk_mali_0\n");
		return 0;
	}
	if (!clk_mali_0_parent) {
		printk("could not get clk_mali_0_parent\n");
		return 0;
	}

	clk_set_parent(clk_mali_0, clk_mali_0_parent);

	clk_prepare_enable(clk_mali_0_parent);
	//ret = clk_set_rate(clk_mali, 425000000);

	clk_prepare_enable(clk_mali_0);

	clk_set_parent(clk_mali, clk_mali_0);

	clk_prepare_enable(clk_mali);

	printk(" clk_mali_0_parent =%p, clk_mali_0=%p, clk_mali=%p\n ",
			clk_mali_0_parent, clk_mali_0, clk_mali);

	printk("pdev->drvdata=%p\n",
			dev_get_drvdata(&pdev->dev));

	return 0;
}

int mali_clock_init(mali_plat_info_t *pdev)
{
	*pdev = *pdev;
	return 0;
}

int mali_clock_critical(critical_t critical, size_t param)
{
	int ret = 0;

	ret = critical(param);

	return ret;
}

static int critical_clock_set(size_t param)
{
	int ret = 0;
	unsigned int idx = param;
	uint32_t pre_clk_rate = 0;
	struct platform_device *pdev = pmali_plat->pdev;
	mali_dvfs_threshold_table *dvfs_tbl = &pmali_plat->dvfs_table[idx];

	struct clk *clk_mali_0_parent = NULL;
	struct clk *clk_mali_0 = NULL;
	struct clk *clk_mali_1 = NULL;
	struct clk *clk_mali   = NULL;

	clk_mali = clk_get(&pdev->dev, "clk_mali");
	if (!clk_mali) {
		printk("could not get clk_mali\n");
		return 0;
	}
	clk_mali_0 = clk_get(&pdev->dev, "clk_mali_0");
	if (!clk_mali_0) {
		printk("could not get clk_mali_0\n");
		return 0;
	}
	clk_mali_1 = clk_get(&pdev->dev, "clk_mali_1");
	if (!clk_mali_1) {
		printk("could not get clk_mali_1\n");
		return 0;
	}
	clk_mali_0_parent = clk_get_parent(clk_mali_0);

	if (!clk_mali_0_parent) {
		printk("could not get clk_mali_0_parent\n");
		return 0;
	}
	clk_set_parent(clk_mali_1, clk_mali_0_parent);
	pre_clk_rate = clk_get_rate(clk_mali_0);
	ret = clk_set_rate(clk_mali_1, pre_clk_rate);
	clk_prepare_enable(clk_mali_1);
	clk_set_parent(clk_mali, clk_mali_1);
	clk_prepare_enable(clk_mali);
	clk_mali_0_parent = clk_get(&pdev->dev,
			dvfs_tbl->clk_parent);
	if (!clk_mali_0_parent) {
		printk("could not get clk_mali_0_parent\n");
		return 0;
	}

	ret = clk_set_rate(clk_mali_0_parent, dvfs_tbl->clkp_freq);
	clk_set_parent(clk_mali_0, clk_mali_0_parent);
	clk_prepare_enable(clk_mali_0_parent);
	ret = clk_set_rate(clk_mali, dvfs_tbl->clk_freq);

	clk_prepare_enable(clk_mali_0);

	clk_set_parent(clk_mali, clk_mali_0);

	clk_prepare_enable(clk_mali);
	clk_put(clk_mali);
	clk_put(clk_mali_0);
	clk_put(clk_mali_1);
	clk_put(clk_mali_0_parent);

	return 0;
}

int mali_clock_set(unsigned int clock)
{
	return mali_clock_critical(critical_clock_set, (size_t)clock);
}

void disable_clock(void)
{
	struct platform_device *pdev = pmali_plat->pdev;
	struct clk *clk_mali_0 = NULL;
	struct clk *clk_mali_1 = NULL;

	clk_mali_0 = clk_get(&pdev->dev, "clk_mali_0");
	clk_mali_1 = clk_get(&pdev->dev, "clk_mali_1");
	if (!clk_mali_0 || ! clk_mali_1) {
		printk("could not get clk_mali_1 or clk_mali_0\n");
		return ;
	}

	clk_disable_unprepare(clk_mali_0);
	clk_disable_unprepare(clk_mali_1);
}

void enable_clock(void)
{

	struct platform_device *pdev = pmali_plat->pdev;
	struct clk *clk_mali_0 = NULL;
	struct clk *clk_mali_1 = NULL;

	clk_mali_0 = clk_get(&pdev->dev, "clk_mali_0");
	clk_mali_1 = clk_get(&pdev->dev, "clk_mali_1");
	if (!clk_mali_0 || ! clk_mali_1) {
		printk("could not get clk_mali_1 or clk_mali_0\n");
		return ;
	}

	clk_prepare_enable(clk_mali_0);
	clk_prepare_enable(clk_mali_1);
}

u32 get_mali_freq(u32 idx)
{
	if (!mali_pm_statue) {
		return pmali_plat->clk_sample[idx];
	} else {
		return 0;
	}
}

void set_str_src(u32 data)
{
	printk("%s, %s, %d\n", __FILE__, __func__, __LINE__);
}

int mali_dt_info(struct platform_device *pdev, struct mali_plat_info_t *mpdata)
{
	struct device_node *gpu_dn = pdev->dev.of_node;
	struct device_node *gpu_clk_dn;
	phandle dvfs_tbl_hdl;
	phandle dvfs_clk_hdl[CLK_DVFS_TBL_SIZE];
	mali_dvfs_threshold_table *dvfs_tbl = mpdata->dvfs_table;
	uint32_t *clk_sample = mpdata->clk_sample;

	int i = 0;
	int ret = 0;
	if (!gpu_dn) {
		printk("gpu device node not right\n");
	}

	ret = of_property_read_u32(gpu_dn,"num_of_pp",
			&mpdata->cfg_pp);
	mpdata->scale_info.maxpp = mpdata->cfg_pp;

	ret = of_property_read_u32(gpu_dn,"dvfs_tbl",
			&dvfs_tbl_hdl);
	gpu_dn = of_find_node_by_phandle(dvfs_tbl_hdl);
	if (!gpu_dn) {
		printk("failed to find gpu dvfs table\n");
	}

	ret = of_property_read_u32(gpu_dn,"sc_mpp",
			&mpdata->sc_mpp);

	ret = of_property_read_u32_array(gpu_dn,"tbl",
			&dvfs_clk_hdl[0], CLK_DVFS_TBL_SIZE);

	for (i = 0; i<CLK_DVFS_TBL_SIZE; i++) {
		gpu_clk_dn = of_find_node_by_phandle(dvfs_clk_hdl[i]);
		ret = of_property_read_u32(gpu_clk_dn,"clk_freq", &dvfs_tbl->clk_freq);
		if (ret) {
			printk("read clk_freq failed\n");
		}
		ret = of_property_read_string(gpu_clk_dn,"clk_parent",
			    &dvfs_tbl->clk_parent);
		if (ret) {
			printk("read clk_parent failed\n");
		}
		ret = of_property_read_u32(gpu_clk_dn,"clkp_freq", &dvfs_tbl->clkp_freq);
		if (ret) {
			printk("read clk_parent freq failed\n");
		}
		ret = of_property_read_u32(gpu_clk_dn,"voltage", &dvfs_tbl->voltage);
		if (ret) {
			printk("read voltage failed\n");
		}
		ret = of_property_read_u32(gpu_clk_dn,"keep_count", &dvfs_tbl->keep_count);
		if (ret) {
			printk("read keep_count failed\n");
		}
		//downthreshold and upthreshold shall be u32
		ret = of_property_read_u32_array(gpu_clk_dn,"threshold",
			    &dvfs_tbl->downthreshold, 2);
		if (ret) {
			printk("read threshold failed\n");
		}
		dvfs_tbl->freq_index = i;
		*clk_sample = dvfs_tbl->clk_freq / 1000000;
		dvfs_tbl ++;
		clk_sample ++;
	}

	dvfs_tbl = mpdata->dvfs_table;
	clk_sample = mpdata->clk_sample;
	for (i = 0; i<CLK_DVFS_TBL_SIZE; i++) {
		printk("===============%d=================\n", i);
		printk("clk_freq=%d\nclk_parent=%s\nvoltage=%d\nkeep_count=%d\nthreshod=<%d %d>\n, clk_sample=%d\n",
				dvfs_tbl->clk_freq, dvfs_tbl->clk_parent,
				dvfs_tbl->voltage,  dvfs_tbl->keep_count,
				dvfs_tbl->downthreshold, dvfs_tbl->upthreshold, *clk_sample);
		dvfs_tbl ++;
		clk_sample ++;
	}

	pmali_plat = mpdata;
	mpdata->pdev = pdev;
	return 0;
}
#endif
