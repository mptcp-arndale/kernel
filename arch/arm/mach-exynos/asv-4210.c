/* linux/arch/arm/mach-exynos/asv-4210.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS4210 - ASV(Adaptive Supply Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <plat/cputype.h>
#include <plat/clock.h>

#include <mach/regs-iem.h>
#include <mach/regs-clock.h>
#include <mach/cpufreq.h>
#include <mach/asv.h>

enum target_asv {
	EXYNOS4210_1200,
	EXYNOS4210_1400,
	EXYNOS4210_SINGLE_1200,
};

struct asv_judge_table exynos4210_1200_limit[] = {
	/* HPM , IDS */
	{8 , 4},
	{11 , 8},
	{14 , 12},
	{18 , 17},
	{21 , 27},
	{23 , 45},
	{25 , 55},
};

static struct asv_judge_table exynos4210_1400_limit[] = {
	/* HPM , IDS */
	{13 , 8},
	{17 , 12},
	{22 , 32},
	{26 , 52},
};

static struct asv_judge_table exynos4210_single_1200_limit[] = {
	/* HPM , IDS */
	{8 , 4},
	{14 , 12},
	{21 , 27},
	{25 , 55},
};

static int exynos4210_check_vdd_arm(void)
{
	/* It will be support */
	return 0;
}

static int exynos4210_asv_pre_clock_init(void)
{
	struct clk *clk_hpm;
	struct clk *clk_copy;
	struct clk *clk_parent;

	/* PWI clock setting */
	clk_copy = clk_get(NULL, "sclk_pwi");
	if (IS_ERR(clk_copy)) {
		pr_info("EXYNOS4210: ASV : SCLK_PWI clock get error\n");
		return -EINVAL;
	} else {
		clk_parent = clk_get(NULL, "xusbxti");

		if (IS_ERR(clk_parent)) {
			pr_info("EXYNOS4210: ASV: MOUT_APLL clock get error\n");
			clk_put(clk_copy);
			return -EINVAL;
		}
		if (clk_set_parent(clk_copy, clk_parent))
			pr_info("EXYNOS4210: ASV: Unable to set parent %s of clock %s.\n",
					clk_parent->name, clk_copy->name);

		clk_put(clk_parent);
	}
	clk_set_rate(clk_copy, 4800000);

	clk_put(clk_copy);

	/* HPM clock setting */
	clk_copy = clk_get(NULL, "dout_copy");
	if (IS_ERR(clk_copy)) {
		pr_info("EXYNOS4210: ASV: DOUT_COPY clock get error\n");
		return -EINVAL;
	} else {
		clk_parent = clk_get(NULL, "mout_mpll");
		if (IS_ERR(clk_parent)) {
			pr_info("EXYNOS4210: ASV: MOUT_APLL clock get error\n");
			clk_put(clk_copy);
			return -EINVAL;
		}
		if (clk_set_parent(clk_copy, clk_parent))
			pr_info("EXYNOS4210: ASV: Unable to set parent %s of clock %s.\n",
					clk_parent->name, clk_copy->name);

		clk_put(clk_parent);
	}

	clk_set_rate(clk_copy, (400 * 1000 * 1000));

	clk_put(clk_copy);

	clk_hpm = clk_get(NULL, "sclk_hpm");
	if (IS_ERR(clk_hpm)) {
		pr_info("EXYNOS4210: ASV: Fail to get sclk_hpm\n");
		return -EINVAL;
	}

	clk_set_rate(clk_hpm, (200 * 1000 * 1000));

	clk_put(clk_hpm);

	return 0;
}

static int exynos4210_asv_pre_clock_setup(void)
{
	/* APLL_CON0 level register */
	__raw_writel(0x80FA0601, EXYNOS4_APLL_CON0L8);
	__raw_writel(0x80C80601, EXYNOS4_APLL_CON0L7);
	__raw_writel(0x80C80602, EXYNOS4_APLL_CON0L6);
	__raw_writel(0x80C80604, EXYNOS4_APLL_CON0L5);
	__raw_writel(0x80C80601, EXYNOS4_APLL_CON0L4);
	__raw_writel(0x80C80601, EXYNOS4_APLL_CON0L3);
	__raw_writel(0x80C80601, EXYNOS4_APLL_CON0L2);
	__raw_writel(0x80C80601, EXYNOS4_APLL_CON0L1);

	/* IEM Divider register */
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L8);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L7);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L6);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L5);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L4);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L3);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L2);
	__raw_writel(0x00500000, EXYNOS4_CLKDIV_IEM_L1);

	return 0;
}

static int exynos4210_find_group(struct samsung_asv *asv_info,
			      enum target_asv exynos4_target)
{
	unsigned int ret = 0;
	unsigned int i;

	if (exynos4_target == EXYNOS4210_1200) {
		ret = ARRAY_SIZE(exynos4210_1200_limit);

		for (i = 0; i < ARRAY_SIZE(exynos4210_1200_limit); i++) {
			if (asv_info->hpm_result <= exynos4210_1200_limit[i].hpm_limit ||
			   asv_info->ids_result <= exynos4210_1200_limit[i].ids_limit) {
				ret = i;
				break;
			}
		}
	} else if (exynos4_target == EXYNOS4210_1400) {
		ret = ARRAY_SIZE(exynos4210_1400_limit);

		for (i = 0; i < ARRAY_SIZE(exynos4210_1400_limit); i++) {
			if (asv_info->hpm_result <= exynos4210_1400_limit[i].hpm_limit ||
			   asv_info->ids_result <= exynos4210_1400_limit[i].ids_limit) {
				ret = i;
				break;
			}
		}
	} else if (exynos4_target == EXYNOS4210_SINGLE_1200) {
		ret = ARRAY_SIZE(exynos4210_single_1200_limit);

		for (i = 0; i < ARRAY_SIZE(exynos4210_single_1200_limit); i++) {
			if (asv_info->hpm_result <= exynos4210_single_1200_limit[i].hpm_limit ||
			   asv_info->ids_result <= exynos4210_single_1200_limit[i].ids_limit) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

static int exynos4210_get_hpm(struct samsung_asv *asv_info)
{
	unsigned int i;
	unsigned int tmp;
	unsigned int hpm_delay = 0;
	void __iomem *iem_base;

	iem_base = ioremap(EXYNOS4_PA_IEM, (128 * 1024));

	if (!iem_base) {
		pr_info("EXYNOS: ioremap fail\n");
		return -EPERM;
	}

	/* Clock setting to get asv value */
	if (!asv_info->pre_clock_init) {
		pr_info("EXYNOS: No Pre-setup function\n");
		goto err;
	} else {
		if (asv_info->pre_clock_init()) {
			pr_info("EXYNOS: pre_clock_init function fail");
			goto err;
		} else {
			/* HPM enable  */
			tmp = __raw_readl(iem_base + EXYNOS4_APC_CONTROL);
			tmp |= APC_HPM_EN;
			__raw_writel(tmp, (iem_base + EXYNOS4_APC_CONTROL));

			asv_info->pre_clock_setup();

			/* IEM enable */
			tmp = __raw_readl(iem_base + EXYNOS4_IECDPCCR);
			tmp |= IEC_EN;
			__raw_writel(tmp, (iem_base + EXYNOS4_IECDPCCR));
		}
	}

	/* Get HPM Delay value */
	for (i = 0; i < LOOP_CNT; i++) {
		tmp = __raw_readb(iem_base + EXYNOS4_APC_DBG_DLYCODE);
		hpm_delay += tmp;
	}

	hpm_delay /= LOOP_CNT;

	/* Store result of hpm value */
	asv_info->hpm_result = hpm_delay;

	return 0;

err:
	iounmap(iem_base);

	return -EPERM;
}

static int exynos4210_get_ids(struct samsung_asv *asv_info)
{
	unsigned int pkg_id_val;

	if (!asv_info->ids_offset || !asv_info->ids_mask) {
		pr_info("EXYNOS4: No ids_offset or No ids_mask\n");
		return -EPERM;
	}

	pkg_id_val = __raw_readl(S5P_VA_CHIPID + 0x4);
	asv_info->pkg_id = pkg_id_val;
	asv_info->ids_result = ((pkg_id_val >> asv_info->ids_offset) &
							asv_info->ids_mask);

	return 0;
}

#define PACK_ID				8
#define PACK_MASK			0x3

static int exynos4210_asv_store_result(struct samsung_asv *asv_info)
{
	unsigned int result_grp;
	char *support_freq;

	/* Single chip is only support 1.2GHz */
	if (!((cpu_idcode >> PACK_ID) & PACK_MASK)) {
		result_grp = exynos4210_find_group(asv_info, EXYNOS4210_SINGLE_1200);
		result_grp |= SUPPORT_1200MHZ;
		support_freq = "1.2GHz";

		goto set_reg;
	}

	/* Check support freq */
	switch (asv_info->pkg_id & 0x7) {
	/* Support 1.2GHz */
	case 1:
	case 7:
		result_grp = exynos4210_find_group(asv_info, EXYNOS4210_1200);
		result_grp |= SUPPORT_1200MHZ;
		support_freq = "1.2GHz";
		break;
	/* Support 1.4GHz */
	case 5:
		result_grp = exynos4210_find_group(asv_info, EXYNOS4210_1400);
		result_grp |= SUPPORT_1200MHZ;
		support_freq = "1.4GHz";
		break;
	/* Defalut support 1.0GHz */
	default:
		result_grp = exynos4210_find_group(asv_info, EXYNOS4210_1200);
		result_grp |= SUPPORT_1000MHZ;
		support_freq = "1.0GHz";
		break;
	}

set_reg:
	__raw_writel(result_grp, S5P_INFORM2);

	pr_info(KERN_INFO "Support %s\n", support_freq);
	pr_info(KERN_INFO "ASV Group for This Exynos4210 is 0x%x\n", result_grp);

	return 0;
}

int exynos4210_asv_init(struct samsung_asv *asv_info)
{
	pr_info("EXYNOS4210: Adaptive Support Voltage init\n");

	asv_info->ids_offset = 24;
	asv_info->ids_mask = 0xFF;

	asv_info->get_ids = exynos4210_get_ids;
	asv_info->get_hpm = exynos4210_get_hpm;
	asv_info->check_vdd_arm = exynos4210_check_vdd_arm;
	asv_info->pre_clock_init = exynos4210_asv_pre_clock_init;
	asv_info->pre_clock_setup = exynos4210_asv_pre_clock_setup;
	asv_info->store_result = exynos4210_asv_store_result;

	return 0;
}