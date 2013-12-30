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
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>


static ssize_t mpgpu_show(struct class *class,
    struct class_attribute *attr, char *buf)
{
    return 0;
}

#define PREHEAT_CMD "preheat"

void mali_plat_preheat(void);
static ssize_t mpgpu_store(struct class *class,
    struct class_attribute *attr, const char *buf, size_t count)
{
	if(!strncmp(buf,PREHEAT_CMD,strlen(PREHEAT_CMD)))
		mali_plat_preheat();
	return count;
}

static CLASS_ATTR(mpgpucmd, 0644, mpgpu_show, mpgpu_store);

static struct class mpgpu_class = {
	.name = "mpgpu",
};

int mpgpu_class_init(void)
{
	int error;
	
	error = class_register(&mpgpu_class);
	if (error) {
		printk(KERN_ERR "%s: class_register failed\n", __func__);
		return error;
	}
	error = class_create_file(&mpgpu_class, &class_attr_mpgpucmd);
	if (error) {
		printk(KERN_ERR "%s: class_create_file failed\n", __func__);
		class_unregister(&mpgpu_class);
	}

	return error;

}

void  mpgpu_class_exit(void)
{
	class_unregister(&mpgpu_class);
}


static int __init mpgpu_init(void)
{
	return mpgpu_class_init();
}

static void __exit mpgpu_exit(void)
{
	mpgpu_class_exit();
}

#ifndef MODULE
fs_initcall(mpgpu_init);
module_exit(mpgpu_exit);

MODULE_DESCRIPTION("AMLOGIC  mpgpu driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("aml-sh <kasin.li@amlogic.com>");
#endif


