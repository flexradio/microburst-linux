/*
 * mburst-regulator.c
 *
 * I2C controlled voltage regulator driver
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

//#define DEBUG 1
//#define FAKEIT

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mburst-regulator.h>

/* Driver local storage details */
struct mburst_vr_drvdata {
	int			min_uV;
	int			max_uV;
#ifdef FAKEIT
	int                     debug_val;
#endif
//	struct i2c_client       *client;
	struct regulator_desc	desc;
	struct regulator_dev	*rdev;
	spinlock_t		lock;
};

static int mburst_vr_get_voltage(struct regulator_dev *dev)
{
	struct mburst_vr_drvdata *vrdd = rdev_get_drvdata(dev);
//	struct i2c_client *client = vrdd->client;
	u32 uVolts;
	int ret;

	dev_dbg(&client->dev, "mburst_vr_get_voltage\n");

	ret = i2c_master_recv(client, (char *)&uVolts, 4);
	if (ret < 0) {
		dev_err(&client->dev, "I2C read error\n");
		goto get_out;
	}
	ret = be32_to_cpu(uVolts);

get_out:

#ifdef FAKEIT
	ret = vrdd->debug_val;
	printk("Get FAKE returning [%d]\n", ret);
#endif
	return ret;
}

static int mburst_vr_set_voltage(struct regulator_dev *dev,
				int min_uV, int max_uV)
{
	struct mburst_vr_drvdata *vrdd = rdev_get_drvdata(dev);
	struct i2c_client *client = vrdd->client;
	int uVolts;
	int ret;

	dev_dbg(&client->dev, "Set min[%d] max[%d]\n", min_uV, max_uV);

	if (min_uV > vrdd->max_uV || min_uV < vrdd->min_uV)
		return -EINVAL;
	if (max_uV > vrdd->max_uV || max_uV < vrdd->min_uV)
		return -EINVAL;

	uVolts =  cpu_to_be32((min_uV + max_uV)/2);

	/* write the new voltage value back */
	ret = i2c_master_send(client, (unsigned char *) &uVolts, 4);
	if (ret < 0)
		dev_err(&client->dev, "I2C read error\n");

#ifdef FAKEIT
	vrdd->debug_val = min_uV;
	ret = 0;
#endif
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

static const struct i2c_device_id mbreg_id[] = {
	{ "mburst-regulator", (kernel_ulong_t)&mvinfo },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mbreg_id);

static int __devinit mburst_vr_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct regulator_init_data *init_data;
	struct mburst_vr_drvdata *vrdd;
	struct mburst_vr_platform_data *mburst_vr_init_data; // ADDED XYZZY

	int error;

	if (!client) {
		printk("***** mb-reg: Invalid i2c client data\n");
		return -EINVAL;
	}

	mburst_vr_init_data = client->dev.platform_data;
	init_data = mburst_vr_init_data->pmic_init_data;

	if (!init_data) {
		printk("***** mb-reg: Invalid init data\n");
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

	vrdd->client = client;
	vrdd->min_uV = init_data->constraints.min_uV;
	vrdd->max_uV = init_data->constraints.max_uV;

	vrdd->rdev = regulator_register(&mb_reg, &client->dev,
					init_data, vrdd);

	if (IS_ERR(vrdd->rdev)) {
		error = PTR_ERR(vrdd->rdev);
		dev_err(&client->dev, "failed to register %s %s\n",
			id->name, mb_reg.name);
		goto err_free_vrdd;
	}

	i2c_set_clientdata(client, vrdd);
	dev_dbg(&client->dev, "%s regulator driver is registered.\n", id->name);

	return 0;

err_free_vrdd:
	kfree(vrdd);

	return error;
}

/**
 * mburst_vr_remove - I2C volt reg driver remove handler
 * @client:	I2c regulator driver client device structure
 *
 * Unregister driver as an I2C  client device driver
 */
static int __devexit mburst_vr_remove(struct i2c_client *client)
{
	struct mburst_vr_drvdata *vrdd = i2c_get_clientdata(client);
	regulator_unregister(vrdd->rdev);
	kfree(vrdd);
	return 0;
}

static struct i2c_driver mburst_vr_driver = {
	.driver = {
		.name = "mburst_vr",
		.owner = THIS_MODULE,
	},
	.probe = mburst_vr_probe,
	.remove = __devexit_p(mburst_vr_remove),
	.id_table	= mbreg_id,
};

/**
 * mburst_vr_init
 *
 * I2C based voltage regulator Module init function
 */
int __init mburst_vr_init(void)
{
	return i2c_add_driver(&mburst_vr_driver);
	//return platform_driver_register(&mburst_vr_driver);
}
subsys_initcall(mburst_vr_init);

/**
 * mburst_vr_exit
 *
 * Module exit function
 */
static void __exit mburst_vr_exit(void)
{
	i2c_del_driver(&mburst_vr_driver);
}
module_exit(mburst_vr_exit);

MODULE_AUTHOR("Flex Radio Systems");
MODULE_DESCRIPTION("I2C voltage regulator driver");
MODULE_LICENSE("GPL v2");
