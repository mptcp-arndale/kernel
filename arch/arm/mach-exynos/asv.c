/* linux/arch/arm/mach-exynos/asv.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS4 - ASV(Adaptive Supply Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <plat/cputype.h>

#include <mach/map.h>
#include <mach/regs-iem.h>
#include <mach/asv.h>

static struct samsung_asv *exynos_asv;

static int __init exynos4_asv_init(void)
{
	int ret = -EINVAL;

	exynos_asv = kzalloc(sizeof(struct samsung_asv), GFP_KERNEL);
	if (!exynos_asv)
		goto out1;

	if (soc_is_exynos4210())
		ret = exynos4210_asv_init(exynos_asv);
	else if (soc_is_exynos4412() || soc_is_exynos4212())
		ret = exynos4x12_asv_init(exynos_asv);
	else {
		pr_info("EXYNOS: There is no type for ASV\n");
		goto out2;
	}

	if (exynos_asv->check_vdd_arm) {
		if (exynos_asv->check_vdd_arm()) {
			pr_info("EXYNOS: It is wrong vdd_arm\n");
			goto out2;
		}
	}

	/* Get HPM Delay value */
	if (exynos_asv->get_hpm) {
		if (exynos_asv->get_hpm(exynos_asv)) {
			pr_info("EXYNOS: Fail to get HPM Value\n");
			goto out2;
		}
	} else {
		pr_info("EXYNOS: Fail to get HPM Value\n");
		goto out2;
	}

	/* Get IDS ARM Value */
	if (exynos_asv->get_ids) {
		if (exynos_asv->get_ids(exynos_asv)) {
			pr_info("EXYNOS: Fail to get IDS Value\n");
			goto out2;
		}
	} else {
		pr_info("EXYNOS: Fail to get IDS Value\n");
		goto out2;
	}

	if (exynos_asv->store_result) {
		if (exynos_asv->store_result(exynos_asv)) {
			pr_info("EXYNOS: Can not success to store result\n");
			goto out2;
		}
	} else {
		pr_info("EXYNOS: No store_result function\n");
		goto out2;
	}

	return 0;
out2:
	kfree(exynos_asv);
out1:
	return -EINVAL;
}
device_initcall_sync(exynos4_asv_init);