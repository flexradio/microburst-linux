 /*
 * Microburst kernel voltage regulator driver
 *
 * Copyright (C) 2012 Flex Radio Systems
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mburst-regulator.h>

#define PROCFS_MAX_SIZE		1024
#define PROCFS_NAME 		"mb_regulator"

/* Driver local storage details */
struct mburst_vr_drvdata {
	int			min_uV;
	int			max_uV;
	int			setpoint;
	int			davinci_wants;
	struct regulator_desc	desc;
	struct regulator_dev	*rdev;
	spinlock_t		lock;
};

static int vr_davinci_wants = 995000;	//debug value remove!
static int vr_setpoint = 800000;

static struct proc_dir_entry *Our_Proc_File;
static char procfs_buffer[PROCFS_MAX_SIZE];

/* read function for procfs file */

int mb_proc_read(char *buf,char **start,off_t offset,int count,int *eof,void *data ) 
{
	int len=0;

	len  += sprintf(buf+len, "%d\n",vr_davinci_wants);

	//printk(KERN_INFO"MB procfile read: davinci wants %d uV\n",vr_davinci_wants);
   
	return len;
}

/* write function for procfs file */

int mb_proc_write(struct file *file,const char *buf,unsigned long count,void *data )
{
	if(count > PROCFS_MAX_SIZE)
	    count = PROCFS_MAX_SIZE;
	if(copy_from_user(procfs_buffer, buf, count))
	    return -EFAULT;

	sscanf(procfs_buffer, "%d", &vr_setpoint);
  
	//printk(KERN_INFO "MB procfile write: VR set to %d uV\n",vr_setpoint);
  	
	return count;
}

static int mburst_vr_get_voltage(struct regulator_dev *dev)
{

	int ret;

	//printk(KERN_INFO "Mburst VR Setpoint %d\n",vr_setpoint);

	ret = vr_setpoint;

	return ret;
}

static int mburst_vr_set_voltage(struct regulator_dev *dev,
				int min_uV, int max_uV)
{
	struct mburst_vr_drvdata *vrdd = rdev_get_drvdata(dev);
	int uVolts;
	int ret;

	if (min_uV > vrdd->max_uV || min_uV < vrdd->min_uV)
		return -EINVAL;
	if (max_uV > vrdd->max_uV || max_uV < vrdd->min_uV)
		return -EINVAL;

	uVolts =  (min_uV + max_uV)/2;

	vr_davinci_wants = uVolts;

	//printk(KERN_INFO "Mburst VR Davinci Requested Regulator to %d uV\n",uVolts);

	ret = 0;

	return ret;

}

/* Operations permitted */
static struct regulator_ops mburst_vr_ops = {
	.get_voltage = mburst_vr_get_voltage,
	.set_voltage = mburst_vr_set_voltage,
};

static struct regulator_desc mb_reg = {
	.name = "VFB",
	.id = 0,
	.ops = &mburst_vr_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
};

struct mbreg_vreg_info {
	int min_uV;
	int max_uV;
};

static const struct mbreg_vreg_info mvinfo = {
	.min_uV = 800000,
	.max_uV = 1025000,
};


static int __devinit mburst_vr_probe(struct platform_device *pdev)
{
	struct regulator_init_data *init_data;
	struct mburst_vr_drvdata *vrdd;
	struct mburst_vr_platform_data *mburst_vr_init_data;

	int error;

	if (!pdev) {
		printk(KERN_ERR "***** mb-reg: Device failed to register\n");
		return -EINVAL;
	}

	mburst_vr_init_data = pdev->dev.platform_data;
	init_data = mburst_vr_init_data->pmic_init_data;

	if (!init_data) {
		printk(KERN_ERR "***** mb-reg: Invalid init data\n");
		return -EINVAL;
	}

	vrdd = kzalloc(sizeof(*vrdd), GFP_KERNEL);
	if (!vrdd)
		return -ENOMEM;

	/* init spinlock for workqueue */
	spin_lock_init(&vrdd->lock);

	/* Store regulator specific information */
	vrdd->desc.id = 1;
	vrdd->desc.ops = &mburst_vr_ops;
	vrdd->desc.type = REGULATOR_VOLTAGE;
	vrdd->desc.owner = THIS_MODULE;

#ifdef FAKEIT
	vrdd->debug_val = 1000000;
#endif

	vrdd->min_uV = init_data->constraints.min_uV;
	vrdd->max_uV = init_data->constraints.max_uV;

	vrdd->rdev = regulator_register(&mb_reg, &pdev->dev,
					init_data, vrdd);

	if (IS_ERR(vrdd->rdev)) {
		error = PTR_ERR(vrdd->rdev);
		dev_err(&pdev->dev, "failed to register driver.\n");
		goto err_free_vrdd;
	}

	dev_dbg(&pdev->dev, "Regulator driver is registered.\n");

	return 0;

err_free_vrdd:
	kfree(vrdd);

	return error;
}

/**
 * mburst_vr_remove - Microburst volt reg driver remove handler
 *
 * Unregister driver as a platform device driver
 */
static int __devexit mburst_vr_remove(struct platform_device *pdev)
{
	struct mburst_vr_drvdata *vrdd = dev_get_drvdata(&pdev->dev);
	regulator_unregister(vrdd->rdev);
	kfree(vrdd);
	return 0;
}

static struct platform_driver mburst_vr_driver = {
	.driver = {
		.name = "mburst_vr",
		.owner = THIS_MODULE,
	},
	.probe = mburst_vr_probe,
	.remove = __devexit_p(mburst_vr_remove),
};

/**
 * mburst_vr_init
 *
 * Module init function
 */
int __init mburst_vr_init(void)
{

	/* create procfs files */

	Our_Proc_File = create_proc_entry(PROCFS_NAME, 0644, NULL);
	
	if (Our_Proc_File == NULL) {
		remove_proc_entry(PROCFS_NAME, NULL);
		printk(KERN_ERR "Error: Could not initialize /proc/%s\n",
			PROCFS_NAME);
		return -ENOMEM;
	}

	Our_Proc_File->read_proc  = mb_proc_read;
	Our_Proc_File->write_proc = mb_proc_write;
	//Our_Proc_File->owner 	  = THIS_MODULE;
//	Our_Proc_File->mode 	  = S_IFREG | S_IRUGO;
//	Our_Proc_File->uid 	  = 0;
//	Our_Proc_File->gid 	  = 0;
//	Our_Proc_File->size 	  = 37;

	printk(KERN_INFO "/proc/%s created\n", PROCFS_NAME);	


	return platform_driver_register(&mburst_vr_driver);

}
subsys_initcall(mburst_vr_init);

/**
 * mburst_vr_exit
 *
 * Module exit function
 */
static void __exit mburst_vr_exit(void)
{
	platform_driver_unregister(&mburst_vr_driver);
	remove_proc_entry(PROCFS_NAME, NULL);
}
module_exit(mburst_vr_exit);

MODULE_AUTHOR("Flex Radio Systems");
MODULE_DESCRIPTION("Microburst voltage regulator driver");
MODULE_LICENSE("GPL v2");
