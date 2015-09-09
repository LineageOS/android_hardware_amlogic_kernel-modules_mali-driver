#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk-private.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/mali/mali_utgard.h>
#include "mali_scaling.h"
#include "mali_clock.h"

#ifndef AML_CLK_LOCK_ERROR
#define AML_CLK_LOCK_ERROR 1
#endif

static unsigned gpu_dbg_level = 0;
module_param(gpu_dbg_level, uint, 0644);
MODULE_PARM_DESC(gpu_dbg_level, "gpu debug level");

#define gpu_dbg(level, fmt, arg...)			\
	do {			\
		if (gpu_dbg_level >= (level))		\
		printk("gpu_debug"fmt , ## arg); 	\
	} while (0)

#define GPU_CLK_DBG(fmt, arg...)	\
	do {		  \
		gpu_dbg(1, "line(%d), clk_cntl=0x%08x\n" fmt, __LINE__, mplt_read(HHI_MALI_CLK_CNTL), ## arg);\
	} while (0)

//static DEFINE_SPINLOCK(lock);
static mali_plat_info_t* pmali_plat = NULL;
//static u32 mali_extr_backup = 0;
//static u32 mali_extr_sample_backup = 0;
struct timeval start;
struct timeval end;

int mali_clock_init_clk_tree(struct platform_device* pdev)
{
	mali_dvfs_threshold_table *dvfs_tbl = &pmali_plat->dvfs_table[pmali_plat->def_clock];
	struct clk *clk_mali_0_parent = dvfs_tbl->clkp_handle;
	struct clk *clk_mali_0 = pmali_plat->clk_mali_0;
#ifdef AML_CLK_LOCK_ERROR
	struct clk *clk_mali_1 = pmali_plat->clk_mali_1;
#endif
	struct clk *clk_mali = pmali_plat->clk_mali;

	clk_set_parent(clk_mali_0, clk_mali_0_parent);

	clk_prepare_enable(clk_mali_0);

	clk_set_parent(clk_mali, clk_mali_0);

#ifdef AML_CLK_LOCK_ERROR
	clk_set_parent(clk_mali_1, clk_mali_0_parent);
	clk_prepare_enable(clk_mali_1);
#endif

	GPU_CLK_DBG("%s:enable(%d), %s:enable(%d)\n",
	  clk_mali_0->name,  clk_mali_0->enable_count,
	  clk_mali_0_parent->name, clk_mali_0_parent->enable_count);

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
	mali_dvfs_threshold_table *dvfs_tbl = &pmali_plat->dvfs_table[idx];

	struct clk *clk_mali_0 = pmali_plat->clk_mali_0;
	struct clk *clk_mali_1 = pmali_plat->clk_mali_1;
	struct clk *clk_mali_x   = NULL;
	struct clk *clk_mali_x_parent = NULL;
	struct clk *clk_mali_x_old = NULL;
	struct clk *clk_mali   = pmali_plat->clk_mali;
	unsigned long time_use=0;

	clk_mali_x_old  = clk_get_parent(clk_mali);

	if (!clk_mali_x_old) {
		printk("could not get clk_mali_x_old or clk_mali_x_old\n");
		return 0;
	}
	if (clk_mali_x_old == clk_mali_0) {
		clk_mali_x = clk_mali_1;
	} else if (clk_mali_x_old == clk_mali_1) {
		clk_mali_x = clk_mali_0;
	} else {
		printk("unmatched clk_mali_x_old\n");
		return 0;
	}

	GPU_CLK_DBG("idx=%d, clk_freq=%d\n", idx, dvfs_tbl->clk_freq);
	clk_mali_x_parent = dvfs_tbl->clkp_handle;
	if (!clk_mali_x_parent) {
		printk("could not get clk_mali_x_parent\n");
		return 0;
	}

	GPU_CLK_DBG();
	ret = clk_set_rate(clk_mali_x_parent, dvfs_tbl->clkp_freq);
	GPU_CLK_DBG();
	ret = clk_set_parent(clk_mali_x, clk_mali_x_parent);
	GPU_CLK_DBG();
	ret = clk_set_rate(clk_mali_x, dvfs_tbl->clk_freq);
	GPU_CLK_DBG();
#ifndef AML_CLK_LOCK_ERROR
	ret = clk_prepare_enable(clk_mali_x);
#endif
	GPU_CLK_DBG("new %s:enable(%d)\n", clk_mali_x->name,  clk_mali_x->enable_count);
	do_gettimeofday(&start);
	udelay(1);// delay 10ns
	do_gettimeofday(&end);
	ret = clk_set_parent(clk_mali, clk_mali_x);
	GPU_CLK_DBG();

#ifndef AML_CLK_LOCK_ERROR
	clk_disable_unprepare(clk_mali_x_old);
#endif
	GPU_CLK_DBG("old %s:enable(%d)\n", clk_mali_x_old->name,  clk_mali_x_old->enable_count);
	time_use = (end.tv_sec - start.tv_sec)*1000000 + end.tv_usec - start.tv_usec;
	GPU_CLK_DBG("step 1, mali_mux use: %ld us\n", time_use);

	return 0;
}

int mali_clock_set(unsigned int clock)
{
	return mali_clock_critical(critical_clock_set, (size_t)clock);
}

void disable_clock(void)
{
	struct clk *clk_mali = pmali_plat->clk_mali;
	struct clk *clk_mali_x = NULL;

	clk_mali_x = clk_get_parent(clk_mali);
	GPU_CLK_DBG();
#ifndef AML_CLK_LOCK_ERROR
	clk_disable_unprepare(clk_mali_x);
#endif
	GPU_CLK_DBG();
}

void enable_clock(void)
{
	struct clk *clk_mali = pmali_plat->clk_mali;
	struct clk *clk_mali_x = NULL;

	clk_mali_x = clk_get_parent(clk_mali);
	GPU_CLK_DBG();
#ifndef AML_CLK_LOCK_ERROR
	clk_prepare_enable(clk_mali_x);
#endif
	GPU_CLK_DBG();
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
	struct mali_gpu_clk_item *clk_item;
	phandle dvfs_clk_hdl;
	mali_dvfs_threshold_table *dvfs_tbl = NULL;
	uint32_t *clk_sample = NULL;

	struct property *prop;
	const __be32 *p;
	int length = 0, i = 0;
	u32 u;

	int ret = 0;
	if (!gpu_dn) {
		printk("gpu device node not right\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(gpu_dn,"num_of_pp",
			&mpdata->cfg_pp);
	if (ret) {
		printk("read max pp failed\n");
		mpdata->cfg_pp = 6;
	}
	mpdata->scale_info.maxpp = mpdata->cfg_pp;
	printk("max pp is %d\n", mpdata->scale_info.maxpp);

	ret = of_property_read_u32(gpu_dn,"min_pp",
			&mpdata->cfg_min_pp);
	if (ret) {
		printk("read min pp failed\n");
		mpdata->cfg_min_pp = 1;
	}
	mpdata->scale_info.minpp = mpdata->cfg_min_pp;
	printk("min pp is %d\n", mpdata->scale_info.minpp);

	ret = of_property_read_u32(gpu_dn,"min_clk",
			&mpdata->cfg_min_clock);
	if (ret) {
		printk("read min clk failed\n");
		mpdata->cfg_min_clock = 0;
	}
	mpdata->scale_info.minclk = mpdata->cfg_min_clock;
	printk("min clk  is %d\n", mpdata->scale_info.minclk);

	mpdata->reg_base_hiubus = of_iomap(gpu_dn, 1);
	printk("hiu io source  0x%p\n", mpdata->reg_base_hiubus);

	mpdata->reg_base_aobus = of_iomap(gpu_dn, 2);
	printk("hiu io source  0x%p\n", mpdata->reg_base_aobus);

	ret = of_property_read_u32(gpu_dn,"sc_mpp",
			&mpdata->sc_mpp);
	if (ret) {
		printk("read min clk failed\n");
		mpdata->cfg_min_clock = mpdata->cfg_pp;
	}
	printk("num of pp used most of time %d\n", mpdata->sc_mpp);

	of_get_property(gpu_dn, "tbl", &length);

	length = length /sizeof(u32);
	printk("clock dvfs table size is %d\n", length);

	ret = of_property_read_u32(gpu_dn,"max_clk",
			&mpdata->cfg_clock);
	if (ret) {
		printk("read max clk failed\n");
		mpdata->cfg_clock = length-2;
	}

	mpdata->cfg_clock_bkup = mpdata->cfg_clock;
	mpdata->scale_info.maxclk = mpdata->cfg_clock;
	printk("max clk  is %d\n", mpdata->scale_info.maxclk);

	ret = of_property_read_u32(gpu_dn,"turbo_clk",
			&mpdata->turbo_clock);
	if (ret) {
		printk("read turbo clk failed\n");
		mpdata->turbo_clock = length-1;
	}
	printk("turbo clk  is %d\n", mpdata->turbo_clock);

	ret = of_property_read_u32(gpu_dn,"def_clk",
			&mpdata->def_clock);
	if (ret) {
		printk("read default clk failed\n");
		mpdata->def_clock = length/2 - 1;
	}
	printk("default clk  is %d\n", mpdata->def_clock);

	mpdata->dvfs_table = devm_kzalloc(&pdev->dev,
								  sizeof(struct mali_dvfs_threshold_table)*length,
								  GFP_KERNEL);
	dvfs_tbl = mpdata->dvfs_table;
	if (mpdata->dvfs_table == NULL) {
		printk("failed to alloc dvfs table\n");
		return -ENOMEM;
	}
	mpdata->dvfs_table_size = length;
	mpdata->clk_sample = devm_kzalloc(&pdev->dev, sizeof(u32)*length, GFP_KERNEL);
	if (mpdata->clk_sample == NULL) {
		printk("failed to alloc clk_sample table\n");
		return -ENOMEM;
	}
	clk_sample = mpdata->clk_sample;
///////////
	mpdata->clk_items = devm_kzalloc(&pdev->dev, sizeof(struct mali_gpu_clk_item) * length, GFP_KERNEL);
	if (mpdata->clk_items == NULL) {
		printk("failed to alloc clk_item table\n");
		return -ENOMEM;
	}
	clk_item = mpdata->clk_items;
//

	of_property_for_each_u32(gpu_dn, "tbl", prop, p, u) {
		dvfs_clk_hdl = (phandle) u;
		gpu_clk_dn = of_find_node_by_phandle(dvfs_clk_hdl);
		ret = of_property_read_u32(gpu_clk_dn,"clk_freq", &dvfs_tbl->clk_freq);
		if (ret) {
			printk("read clk_freq failed\n");
		}
		ret = of_property_read_string(gpu_clk_dn,"clk_parent",
										&dvfs_tbl->clk_parent);
		if (ret) {
			printk("read clk_parent failed\n");
		}
		dvfs_tbl->clkp_handle = devm_clk_get(&pdev->dev, dvfs_tbl->clk_parent);
		if (IS_ERR(dvfs_tbl->clkp_handle)) {
			printk("failed to get %s's clock pointer\n", dvfs_tbl->clk_parent);
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
		clk_item->clock = dvfs_tbl->clk_freq / 1000000;
		clk_item->vol = dvfs_tbl->voltage;

		*clk_sample = dvfs_tbl->clk_freq / 1000000;

		dvfs_tbl ++;
		clk_item ++;
		clk_sample ++;
		i++;
	}

	dvfs_tbl = mpdata->dvfs_table;
	clk_sample = mpdata->clk_sample;
	for (i = 0; i< length; i++) {
		printk("===============%d=================\n", i);
		printk("clk_freq=%d\nclk_parent=%s\nvoltage=%d\nkeep_count=%d\nthreshod=<%d %d>\n, clk_sample=%d\n",
					dvfs_tbl->clk_freq, dvfs_tbl->clk_parent,
					dvfs_tbl->voltage,  dvfs_tbl->keep_count,
					dvfs_tbl->downthreshold, dvfs_tbl->upthreshold, *clk_sample);
		dvfs_tbl ++;
		clk_sample ++;
	}

	mpdata->clk_mali = devm_clk_get(&pdev->dev, "clk_mali");
	mpdata->clk_mali_0 = devm_clk_get(&pdev->dev, "clk_mali_0");
	mpdata->clk_mali_1 = devm_clk_get(&pdev->dev, "clk_mali_1");
	if (IS_ERR(mpdata->clk_mali) || IS_ERR(mpdata->clk_mali_0) || IS_ERR(mpdata->clk_mali_1)) {
		dev_err(&pdev->dev, "failed to get clock pointer\n");
		return -EFAULT;
	}

	pmali_plat = mpdata;
	mpdata->pdev = pdev;
	return 0;
}
