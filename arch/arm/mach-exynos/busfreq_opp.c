/* linux/arch/arm/mach-exynos/busfreq_opp.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS4 - BUS clock frequency scaling support with OPP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/ktime.h>
#include <linux/tick.h>
#include <linux/kernel_stat.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/opp.h>
#include <linux/clk.h>
#include <linux/workqueue.h>

#include <asm/mach-types.h>

#include <mach/ppmu.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/gpio.h>
#include <mach/regs-mem.h>
#include <mach/cpufreq.h>
#include <mach/dev.h>
#include <mach/busfreq.h>

#include <plat/map-s5p.h>
#include <plat/cpu.h>
#include <plat/clock.h>

struct busfreq_control {
	struct opp *opp_lock;
	struct device *dev;
};

static struct busfreq_control bus_ctrl;

void update_busfreq_stat(struct busfreq_data *data, unsigned int index)
{
	unsigned long long cur_time = get_jiffies_64();
	data->time_in_state[index] = cputime64_add(data->time_in_state[index], cputime_sub(cur_time, data->last_time));
	data->last_time = cur_time;
}

static struct opp *busfreq_monitor(struct busfreq_data *data)
{
	struct opp *opp;
	unsigned long lockfreq;
	unsigned long newfreq = 160000;
	unsigned long long cpu_load = 0;
	unsigned long long dmc_load = 0;

	ppmu_update(data->dev);

	cpu_load = ppmu_load[PPMU_CPU];
	dmc_load = (ppmu_load[PPMU_DMC0] + ppmu_load[PPMU_DMC1]) / 2;

	if (dmc_load > IDLE_THRESHOLD)
		newfreq = 400000;

	lockfreq = dev_max_freq(data->dev);

	if (lockfreq > newfreq)
		newfreq = lockfreq;

	opp = opp_find_freq_ceil(data->dev, &newfreq);

	if (bus_ctrl.opp_lock)
		opp = bus_ctrl.opp_lock;

	return opp;
}

static void exynos4_busfreq_timer(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct busfreq_data *data = container_of(delayed_work, struct busfreq_data,
			worker);

	struct opp *opp;
	unsigned int voltage;
	unsigned long currfreq;
	unsigned long newfreq;
	unsigned int index = 0;

	opp = busfreq_monitor(data);

	ppmu_start(data->dev);

	newfreq = opp_get_freq(opp);

	currfreq = opp_get_freq(data->curr_opp);

	index = data->get_table_index(opp);

	if (opp == data->curr_opp || newfreq == 0 || data->use == false)
		goto out;

	voltage = opp_get_voltage(opp);
	if (newfreq > currfreq) {
		if (!IS_ERR(data->vdd_mif)) {
			regulator_set_voltage(data->vdd_mif, voltage,
					voltage);
			voltage = data->get_int_volt(index);
		}
		regulator_set_voltage(data->vdd_int, voltage,
				voltage);
	}

	data->target(index);

	if (newfreq < currfreq) {
		if (!IS_ERR(data->vdd_mif)) {
			regulator_set_voltage(data->vdd_mif, voltage,
					voltage);
			voltage = data->get_int_volt(index);
		}
		regulator_set_voltage(data->vdd_int, voltage,
				voltage);
	}
	data->curr_opp = opp;

out:
	update_busfreq_stat(data, index);
	schedule_delayed_work(&data->worker, data->sampling_rate);
}

static int exynos4_buspm_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct busfreq_data *data = container_of(this, struct busfreq_data,
			exynos4_buspm_notifier);

	unsigned long voltage = opp_get_voltage(data->max_opp);
	unsigned long freq = opp_get_freq(data->max_opp);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		data->use = false;
		if (!IS_ERR(data->vdd_mif)) {
			regulator_set_voltage(data->vdd_mif, voltage,
					voltage);
			voltage = data->get_int_volt(freq);
		}
		regulator_set_voltage(data->vdd_int, voltage, voltage);
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		data->use = true;
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static int exynos4_busfreq_reboot_event(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct busfreq_data *data = container_of(this, struct busfreq_data,
			exynos4_reboot_notifier);

	unsigned long voltage = opp_get_voltage(data->max_opp);
	unsigned long freq = opp_get_freq(data->max_opp);

	if (!IS_ERR(data->vdd_mif)) {
		regulator_set_voltage(data->vdd_mif, voltage,
				voltage);
		voltage = data->get_int_volt(freq);
	}
	regulator_set_voltage(data->vdd_int, voltage, voltage);
	data->use = false;

	printk(KERN_INFO "REBOOT Notifier for BUSFREQ\n");
	return NOTIFY_DONE;
}

int exynos4_busfreq_lock(unsigned int nId,
	enum busfreq_level_request busfreq_level)
{
	return 0;
}

void exynos4_busfreq_lock_free(unsigned int nId)
{
}

static ssize_t show_level_lock(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(bus_ctrl.dev);
	struct busfreq_data *data = (struct busfreq_data *)platform_get_drvdata(pdev);
	int len = 0;
	unsigned long freq;

	freq = bus_ctrl.opp_lock == NULL ? 0 : opp_get_freq(bus_ctrl.opp_lock);

	len = sprintf(buf, "Current Freq : %lu\n", opp_get_freq(data->curr_opp));
	len += sprintf(buf + len, "Current Lock Freq : %lu\n", freq);

	return len;
}

static ssize_t store_level_lock(struct device *device, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct opp *opp;
	unsigned long freq;
	sscanf(buf, "%lu", &freq);
	if (freq == 0) {
		pr_info("Release bus level lock.\n");
		bus_ctrl.opp_lock = NULL;
		return count;
	}

	if (freq > 400000)
		freq = 400000;

	opp = opp_find_freq_ceil(bus_ctrl.dev, &freq);
	bus_ctrl.opp_lock = opp;
	pr_info("Lock Freq : %lu\n", opp_get_freq(opp));
	return count;
}

static ssize_t show_locklist(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return dev_lock_list(bus_ctrl.dev, buf);
}

static ssize_t show_time_in_state(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(bus_ctrl.dev);
	struct busfreq_data *data = (struct busfreq_data *)platform_get_drvdata(pdev);
	ssize_t len = 0;
	int i;

	for (i = 0; i < data->table_size; i++)
		len += sprintf(buf + len, "%u %llu\n", data->table[i].mem_clk,
				(unsigned long long)cputime64_to_clock_t(data->time_in_state[i]));

	return len;
}

static DEVICE_ATTR(curr_freq, 0666, show_level_lock, store_level_lock);
static DEVICE_ATTR(lock_list, 0666, show_locklist, NULL);
static DEVICE_ATTR(time_in_state, 0666, show_time_in_state, NULL);

static struct attribute *busfreq_attributes[] = {
	&dev_attr_curr_freq.attr,
	&dev_attr_lock_list.attr,
	&dev_attr_time_in_state.attr,
	NULL
};

static __devinit int exynos4_busfreq_probe(struct platform_device *pdev)
{
	struct busfreq_data *data;
	struct clk *sclk_dmc;
	unsigned long freq;
	unsigned int val;

	val = __raw_readl(S5P_VA_DMC0 + 0x4);
	val = (val >> 8) & 0xf;

	/* Check Memory Type Only support -> 0x5: 0xLPDDR2 */
	if (val != 0x05) {
		pr_err("[ %x ] Memory Type Undertermined.\n", val);
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct busfreq_data), GFP_KERNEL);
	if (!data) {
		pr_err("Unable to create busfreq_data struct.\n");
		return -ENOMEM;
	}

	data->exynos4_buspm_notifier.notifier_call =
		exynos4_buspm_notifier_event;
	data->exynos4_reboot_notifier.notifier_call =
		exynos4_busfreq_reboot_event;
	data->busfreq_attr_group.attrs = busfreq_attributes;

	if (soc_is_exynos4210()) {
		data->init = exynos4210_init;
		data->target = exynos4210_target;
		data->get_int_volt = NULL;
		data->get_table_index = exynos4210_get_table_index;
	} else {
		data->init = exynos4212_init;
		data->target = exynos4212_target;
		data->get_int_volt = exynos4212_get_int_volt;
		data->get_table_index = exynos4212_get_table_index;
	}

	data->dev = &pdev->dev;
	data->sampling_rate = usecs_to_jiffies(100000);
	bus_ctrl.opp_lock =  NULL;
	bus_ctrl.dev =  data->dev;

	INIT_DELAYED_WORK_DEFERRABLE(&data->worker, exynos4_busfreq_timer);

	if (data->init(&pdev->dev, data)) {
		pr_err("Failed to init busfreq.\n");
		goto err_cpufreq;
	}

	data->time_in_state = kzalloc(sizeof(cputime64_t) * data->table_size, GFP_KERNEL);
	if (!data->time_in_state) {
		pr_err("Unable to create time_in_state.\n");
		goto err_cpufreq;
	}

	sclk_dmc = clk_get(NULL, "sclk_dmc");

	if (IS_ERR(sclk_dmc)) {
		pr_err("Failed to get sclk_dmc.!\n");
		freq = 400000;
	} else {
		freq = clk_get_rate(sclk_dmc) / 1000;
		clk_put(sclk_dmc);
	}

	data->last_time = get_jiffies_64();
	data->curr_opp = opp_find_freq_ceil(&pdev->dev, &freq);

	data->busfreq_kobject = kobject_create_and_add("busfreq",
				&cpu_sysdev_class.kset.kobj);
	if (!data->busfreq_kobject)
		pr_err("Failed to create busfreq kobject.!\n");

	if (sysfs_create_group(data->busfreq_kobject, &data->busfreq_attr_group))
		pr_err("Failed to create attributes group.!\n");

	if (register_pm_notifier(&data->exynos4_buspm_notifier)) {
		pr_err("Failed to setup buspm notifier\n");
		goto err_pm_notifier;
	}

	data->use = true;

	if (register_reboot_notifier(&data->exynos4_reboot_notifier))
		pr_err("Failed to setup reboot notifier\n");

	platform_set_drvdata(pdev, data);

	schedule_delayed_work(&data->worker, data->sampling_rate);
	return 0;

err_pm_notifier:
	kfree(data->time_in_state);

err_cpufreq:
	if (!IS_ERR(data->vdd_int))
		regulator_put(data->vdd_int);

	if (!IS_ERR(data->vdd_mif))
		regulator_put(data->vdd_mif);

	kfree(data);
	return -ENODEV;
}

static __devexit int exynos4_busfreq_remove(struct platform_device *pdev)
{
	struct busfreq_data *data = platform_get_drvdata(pdev);

	unregister_pm_notifier(&data->exynos4_buspm_notifier);
	unregister_reboot_notifier(&data->exynos4_reboot_notifier);
	regulator_put(data->vdd_int);
	regulator_put(data->vdd_mif);
	sysfs_remove_group(data->busfreq_kobject, &data->busfreq_attr_group);
	kfree(data->time_in_state);
	kfree(data);

	return 0;
}

static int exynos4_busfreq_resume(struct device *dev)
{
	ppmu_reset(dev);
	return 0;
}

static const struct dev_pm_ops exynos4_busfreq_pm = {
	.resume = exynos4_busfreq_resume,
};

static struct platform_driver exynos4_busfreq_driver = {
	.probe  = exynos4_busfreq_probe,
	.remove = __devexit_p(exynos4_busfreq_remove),
	.driver = {
		.name   = "exynos4-busfreq",
		.owner  = THIS_MODULE,
		.pm     = &exynos4_busfreq_pm,
	},
};

static int __init exynos4_busfreq_init(void)
{
	return platform_driver_register(&exynos4_busfreq_driver);
}
late_initcall(exynos4_busfreq_init);

static void __exit exynos4_busfreq_exit(void)
{
	platform_driver_unregister(&exynos4_busfreq_driver);
}
module_exit(exynos4_busfreq_exit);