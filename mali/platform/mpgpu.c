/*******************************************************************
 *
 *  Copyright C 2013 by Amlogic, Inc. All Rights Reserved.
 *
 *  Description:
 *
 *  Author: Amlogic Software
 *  Created: 2010/4/1   19:46
 *
 *******************************************************************/
/* Standard Linux headers */
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>

#include <asm/io.h>

#include <linux/mali/mali_utgard.h>
#include <common/mali_kernel_common.h>
#include <common/mali_pmu.h>
#include "meson_main.h"

static ssize_t domain_stat_read(struct class *class, 
			struct class_attribute *attr, char *buf)
{
	unsigned int val;
#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8
	val = mali_pmu_get_status();
#else
	val = 0;
#endif
	return sprintf(buf, "%x\n", val);
}

#if MESON_CPU_TYPE > MESON_CPU_TYPE_MESON6TVD
static ssize_t mpgpu_read(struct class *class,
			struct class_attribute *attr, char *buf)
{
    return 0;
}

#define PREHEAT_CMD "preheat"

static ssize_t mpgpu_write(struct class *class,
			struct class_attribute *attr, const char *buf, size_t count)
{
	if(!strncmp(buf,PREHEAT_CMD,strlen(PREHEAT_CMD)))
		mali_plat_preheat();
	return count;
}

static ssize_t scale_mode_read(struct class *class,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_mali_schel_mode());
}

static ssize_t scale_mode_write(struct class *class,
			struct class_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (0 != ret)
	{
		return -EINVAL;
	}
	set_mali_schel_mode(val);

	return count;
}

static ssize_t max_pp_read(struct class *class,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_max_pp_num());
}

static ssize_t max_pp_write(struct class *class,
			struct class_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (0 != ret)
	{
		return -EINVAL;
	}
	ret = set_max_pp_num(val);

	return count;
}

static ssize_t min_pp_read(struct class *class,
			struct class_attribute *attr, char *buf)
{

	return sprintf(buf, "%d\n", get_min_pp_num());
}

static ssize_t min_pp_write(struct class *class,
			struct class_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (0 != ret)
	{
		return -EINVAL;
	}
	ret = set_min_pp_num(val);

	return count;
}

static ssize_t max_freq_read(struct class *class,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_max_mali_freq());
}

static ssize_t max_freq_write(struct class *class,
			struct class_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (0 != ret)
	{
		return -EINVAL;
	}
	ret = set_max_mali_freq(val);

	return count;
}

static ssize_t min_freq_read(struct class *class,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_min_mali_freq());
}

static ssize_t min_freq_write(struct class *class,
			struct class_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (0 != ret)
	{
		return -EINVAL;
	}
	ret = set_min_mali_freq(val);

	return count;
}

static ssize_t read_extr_src(struct class *class,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "usage echo 0(restore), 1(set fix src), xxx user mode\n");
}

static ssize_t write_extr_src(struct class *class,
			struct class_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (0 != ret)
	{
		return -EINVAL;
	}

	set_str_src(val);

	return count;
}
#endif


static struct class_attribute mali_class_attrs[] = {
	__ATTR(domain_stat,	0644, domain_stat_read, NULL),
#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8
	__ATTR(mpgpucmd,	0644, mpgpu_read,	mpgpu_write),
	__ATTR(scale_mode,	0644, scale_mode_read,  scale_mode_write),
	__ATTR(min_freq,	0644, min_freq_read,  	min_freq_write),
	__ATTR(max_freq,	0644, max_freq_read,	max_freq_write),
	__ATTR(min_pp,		0644, min_pp_read,	min_pp_write),
	__ATTR(max_pp,		0644, max_pp_read,	max_pp_write),
	__ATTR(extr_src,	0644, read_extr_src,	write_extr_src),
#endif
};

static struct class mpgpu_class = {
	.name = "mpgpu",
};

int mpgpu_class_init(void)
{
	int ret = 0;
	int i;
	int attr_num =  ARRAY_SIZE(mali_class_attrs);
	
	ret = class_register(&mpgpu_class);
	if (ret) {
		printk(KERN_ERR "%s: class_register failed\n", __func__);
		return ret;
	}
	for (i = 0; i< attr_num; i++) {
		ret = class_create_file(&mpgpu_class, &mali_class_attrs[i]);
		if (ret) {
			printk(KERN_ERR "%d ST: class item failed to register\n", i);
		}
	}
	return ret;
}

void  mpgpu_class_exit(void)
{
	class_unregister(&mpgpu_class);
}

#if 0
static int __init mpgpu_init(void)
{
	return mpgpu_class_init();
}

static void __exit mpgpu_exit(void)
{
	mpgpu_class_exit();
}

fs_initcall(mpgpu_init);
module_exit(mpgpu_exit);

MODULE_DESCRIPTION("AMLOGIC  mpgpu driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("aml-sh <kasin.li@amlogic.com>");
#endif


