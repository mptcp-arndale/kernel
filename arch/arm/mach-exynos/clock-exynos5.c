/* linux/arch/arm/mach-exynos/clock-exynos5.c
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5 - Clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>

#include <plat/cpu-freq.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/pll.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>
#include <plat/devs.h>
#include <plat/pm.h>
#include <plat/cputype.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-audss.h>
#include <mach/dev-sysmmu.h>
#include <mach/exynos-clock.h>

static struct clk exynos5_clk_sclk_hdmi24m = {
	.name		= "sclk_hdmi24m",
	.rate		= 24000000,
};

static struct clk exynos5_clk_sclk_hdmi27m = {
	.name		= "sclk_hdmi27m",
	.rate		= 27000000,
};

static struct clk exynos5_clk_sclk_hdmiphy = {
	.name		= "sclk_hdmiphy",
};

static struct clk exynos5_clk_sclk_dptxphy = {
	.name		= "sclk_dptx",
};

static struct clk exynos5_clk_sclk_usbphy = {
	.name		= "sclk_usbphy",
	.rate		= 48000000,
};

struct clksrc_clk exynos5_clk_audiocdclk0 = {
	.clk	= {
		.name		= "audiocdclk",
		.rate		= 16934400,
	},
};

static struct clk exynos5_clk_audiocdclk1 = {
	.name           = "audiocdclk",
};

static struct clk exynos5_clk_audiocdclk2 = {
	.name		= "audiocdclk",
};

static struct clk exynos5_clk_spdifcdclk = {
	.name		= "spdifcdclk",
};

static int exynos5_clksrc_mask_top_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_TOP, clk, enable);
}

static int exynos5_clk_ip_peric_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_PERIC, clk, enable);
}

static int exynos5_clk_ip_peris_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_PERIS, clk, enable);
}

static int exynos5_clksrc_mask_peric0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_PERIC0, clk, enable);
}

static int exynos5_clksrc_mask_peric1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_PERIC1, clk, enable);
}

static int exynos5_clksrc_mask_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_FSYS, clk, enable);
}

static int exynos5_clk_ip_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_FSYS, clk, enable);
}

static int exynos5_clk_ip_disp0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_DISP0, clk, enable);
}

static int exynos5_clk_ip_disp1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_DISP1, clk, enable);
}

static int exynos5_clk_ip_mfc_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_MFC, clk, enable);
}

static int exynos5_clk_hdmiphy_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(S5P_HDMI_PHY_CONTROL, clk, enable);
}

static int exynos5_clk_ip_gen_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_GEN, clk, enable);
}

static int exynos5_clksrc_mask_disp0_0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_DISP0_0, clk, enable);
}

static int exynos5_clksrc_mask_disp1_0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_DISP1_0, clk, enable);
}

static int exynos5_clksrc_mask_maudio_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKSRC_MASK_MAUDIO, clk, enable);
}

static int exynos5_clk_audss_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(S5P_CLKGATE_AUDSS, clk, enable);
}

static int exynos5_clk_ip_gscl_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_GSCL, clk, enable);
}

static int exynos5_clk_gate_block(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_BLOCK, clk, enable);
}

static int exynos5_clk_ip_acp_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5_CLKGATE_IP_ACP, clk, enable);
}

/* BPLL clock output
 * No need .ctrlbit, this is always on
*/
static struct clk clk_fout_bpll = {
	.name		= "fout_bpll",
	.id		= -1,
};

/* Possible clock sources for BPLL Mux */
static struct clk *clk_src_bpll_list[] = {
	[0] = &clk_fin_bpll,
	[1] = &clk_fout_bpll,
};

static struct clksrc_sources clk_src_bpll = {
	.sources	= clk_src_bpll_list,
	.nr_sources	= ARRAY_SIZE(clk_src_bpll_list),
};

/* CPLL clock output */
static struct clk clk_fout_cpll = {
	.name		= "fout_cpll",
	.id		= -1,
};

/* Possible clock sources for CPLL Mux */
static struct clk *clk_src_cpll_list[] = {
	[0] = &clk_fin_cpll,
	[1] = &clk_fout_cpll,
};

static struct clksrc_sources clk_src_cpll = {
	.sources	= clk_src_cpll_list,
	.nr_sources	= ARRAY_SIZE(clk_src_cpll_list),
};

/* Core list of CMU_CPU side */

static struct clksrc_clk exynos5_clk_mout_apll = {
	.clk	= {
		.name		= "mout_apll",
	},
	.sources = &clk_src_apll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CPU, .shift = 0, .size = 1 },
};

static struct clksrc_clk exynos5_clk_sclk_apll = {
	.clk	= {
		.name		= "sclk_apll",
		.parent		= &exynos5_clk_mout_apll.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CPU0, .shift = 24, .size = 3 },
};

static struct clksrc_clk exynos5_clk_mout_bpll = {
	.clk	= {
		.name		= "mout_bpll",
	},
	.sources = &clk_src_bpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CDREX, .shift = 0, .size = 1 },
};

static struct clksrc_clk exynos5_clk_mout_cpll = {
	.clk	= {
		.name		= "mout_cpll",
	},
	.sources = &clk_src_cpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 8, .size = 1 },
};

static struct clksrc_clk exynos5_clk_mout_epll = {
	.clk	= {
		.name		= "mout_epll",
		.parent		= &clk_fout_epll,
	},
	.sources = &clk_src_epll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 12, .size = 1 },
};

struct clksrc_clk exynos5_clk_mout_mpll = {
	.clk = {
		.name		= "mout_mpll",
	},
	.sources = &clk_src_mpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CORE1, .shift = 8, .size = 1 },
};

/* For VPLL */
static struct clk *exynos5_clkset_mout_vpllsrc_list[] = {
	[0] = &clk_fin_vpll,
	[1] = &exynos5_clk_sclk_hdmi27m,
};

static struct clksrc_sources exynos5_clkset_mout_vpllsrc = {
	.sources	= exynos5_clkset_mout_vpllsrc_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_mout_vpllsrc_list),
};

static struct clksrc_clk exynos5_clk_mout_vpllsrc = {
	.clk	= {
		.name		= "vpll_src",
		.enable		= exynos5_clksrc_mask_top_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos5_clkset_mout_vpllsrc,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 0, .size = 1 },
};

static struct clk *exynos5_clkset_sclk_vpll_list[] = {
	[0] = &exynos5_clk_mout_vpllsrc.clk,
	[1] = &clk_fout_vpll,
};

static struct clksrc_sources exynos5_clkset_sclk_vpll = {
	.sources	= exynos5_clkset_sclk_vpll_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_vpll_list),
};

static struct clksrc_clk exynos5_clk_sclk_vpll = {
	.clk	= {
		.name		= "sclk_vpll",
	},
	.sources = &exynos5_clkset_sclk_vpll,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 16, .size = 1 },
};

static struct clksrc_clk exynos5_clk_sclk_pixel = {
	.clk	= {
		.name		= "sclk_pixel",
		.parent		= &exynos5_clk_sclk_vpll.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_DISP1_0, .shift = 28, .size = 4 },
};

static struct clk *exynos5_clkset_sclk_hdmi_list[] = {
	[0] = &exynos5_clk_sclk_pixel.clk,
	[1] = &exynos5_clk_sclk_hdmiphy,
};

static struct clksrc_sources exynos5_clkset_sclk_hdmi = {
	.sources	= exynos5_clkset_sclk_hdmi_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_hdmi_list),
};

static struct clksrc_clk exynos5_clk_sclk_hdmi = {
	.clk	= {
		.name           = "sclk_hdmi",
		.enable		= exynos5_clksrc_mask_disp1_0_ctrl,
		.ctrlbit	= (1 << 20),
	},
	.sources = &exynos5_clkset_sclk_hdmi,
	.reg_src = { .reg = EXYNOS5_CLKSRC_DISP1_0, .shift = 20, .size = 1 },
};

static struct clksrc_clk *exynos5_sclk_tv[] = {
	&exynos5_clk_sclk_pixel,
	&exynos5_clk_sclk_hdmi,
};

/* BPLL USER */
static struct clk *exynos5_clk_src_bpll_user_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos5_clk_mout_bpll.clk,
};

static struct clksrc_sources exynos5_clk_src_bpll_user = {
	.sources	= exynos5_clk_src_bpll_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_src_bpll_user_list),
};

static struct clksrc_clk exynos5_clk_mout_bpll_user = {
	.clk	= {
		.name		= "mout_bpll_user",
	},
	.sources = &exynos5_clk_src_bpll_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 24, .size = 1 },
};

/* MPLL USER */
static struct clk *exynos5_clk_src_mpll_user_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos5_clk_mout_mpll.clk,
};

static struct clksrc_sources exynos5_clk_src_mpll_user = {
	.sources	= exynos5_clk_src_mpll_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_src_mpll_user_list),
};

static struct clksrc_clk exynos5_clk_mout_mpll_user = {
	.clk	= {
		.name		= "mout_mpll_user",
	},
	.sources = &exynos5_clk_src_mpll_user,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP2, .shift = 20, .size = 1 },
};

static struct clk *exynos5_clkset_mout_cpu_list[] = {
	[0] = &exynos5_clk_mout_apll.clk,
	[1] = &exynos5_clk_mout_mpll.clk,
};

static struct clksrc_sources exynos5_clkset_mout_cpu = {
	.sources	= exynos5_clkset_mout_cpu_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_mout_cpu_list),
};

static struct clksrc_clk exynos5_clk_mout_cpu = {
	.clk	= {
		.name		= "moutcpu",
	},
	.sources = &exynos5_clkset_mout_cpu,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CPU, .shift = 16, .size = 1 },
};

static struct clksrc_clk exynos5_clk_dout_armclk = {
	.clk	= {
		.name		= "dout_arm_clk",
		.parent		= &exynos5_clk_mout_cpu.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CPU0, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_arm2clk = {
	.clk	= {
		.name		= "dout_arm_clk",
		.parent		= &exynos5_clk_dout_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CPU0, .shift = 28, .size = 3 },
};

static struct clk exynos5_clk_armclk = {
	.name		= "armclk",
	.parent		= &exynos5_clk_dout_arm2clk.clk,
};

/* Core list of CMU_CDREX side */

static struct clk *exynos5_clkset_cdrex_list[] = {
	[0] = &exynos5_clk_mout_mpll.clk,
	[1] = &exynos5_clk_mout_bpll.clk,
};

static struct clksrc_sources exynos5_clkset_mout_cdrex = {
	.sources	= exynos5_clkset_cdrex_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_cdrex_list),
};

static struct clksrc_clk exynos5_clk_mout_cdrex = {
	.clk	= {
		.name		= "mout_cdrex",
	},
	.sources = &exynos5_clkset_mout_cdrex,
	.reg_src = { .reg = EXYNOS5_CLKSRC_CDREX, .shift = 4, .size = 1 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_CDREX, .shift = 16, .size = 3 },
};

static struct clksrc_clk __maybe_unused exynos5_clk_dout_aclk_cdrex = {
	.clk	= {
		.name		= "dout_aclk_cdrex",
		.parent		= &exynos5_clk_mout_cdrex.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_CDREX, .shift = 0, .size = 3 },
};

static struct clk exynos5_clk_mclk_cdrex = {
	.name		= "mclk_cdrex",
	.parent		= &exynos5_clk_mout_cdrex.clk,
};

/* Core list of CMU_TOP side */

struct clk *exynos5_clkset_aclk_top_list[] = {
	[0] = &exynos5_clk_mout_mpll_user.clk,
	[1] = &exynos5_clk_mout_bpll_user.clk,
};

struct clksrc_sources exynos5_clkset_aclk = {
	.sources	= exynos5_clkset_aclk_top_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_aclk_top_list),
};

/* For ACLK_400 */
static struct clksrc_clk exynos5_clk_mout_aclk_400 = {
	.clk	= {
		.name		= "mout_aclk_400",
	},
	.sources = &exynos5_clkset_aclk,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 20, .size = 1 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 24, .size = 3 },
};

static struct clk exynos5_clk_aclk_400 = {
	.name		= "aclk_400",
	.parent		= &exynos5_clk_mout_aclk_400.clk,
};

/* For ACLK_333 */
struct clk *exynos5_clkset_aclk_333_166_list[] = {
	[0] = &exynos5_clk_mout_cpll.clk,
	[1] = &exynos5_clk_mout_mpll_user.clk,
};

struct clksrc_sources exynos5_clkset_aclk_333_166 = {
	.sources	= exynos5_clkset_aclk_333_166_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_aclk_333_166_list),
};

static struct clksrc_clk exynos5_clk_mout_aclk_333 = {
	.clk	= {
		.name		= "mout_aclk_333",
	},
	.sources = &exynos5_clkset_aclk_333_166,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 16, .size = 1 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 20, .size = 3 },
};

static struct clk exynos5_clk_aclk_333 = {
	.name		= "aclk_333",
	.parent		= &exynos5_clk_mout_aclk_333.clk,
};

/* For ACLK_300_disp1_mid */
static struct clksrc_clk exynos5_clk_mout_aclk_300_disp1_mid = {
	.clk	= {
		.name		= "mout_aclk_300_disp1_mid",
	},
	.sources = &exynos5_clkset_aclk,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 14, .size = 1 },
};

/* For ACLK_300_disp1 */
struct clk *exynos5_clkset_aclk_300_disp1_list[] = {
	[0] = &exynos5_clk_mout_aclk_300_disp1_mid.clk,
	[1] = &exynos5_clk_sclk_vpll.clk,
};

struct clksrc_sources exynos5_clkset_aclk_300_disp1 = {
	.sources	= exynos5_clkset_aclk_300_disp1_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_aclk_300_disp1_list),
};

static struct clksrc_clk exynos5_clk_mout_aclk_300_disp1 = {
	.clk	= {
		.name		= "mout_aclk_300_disp1",
	},
	.sources = &exynos5_clkset_aclk_300_disp1,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 15, .size = 1 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 28, .size = 3 },
};

/* For ACLK_300_gscl_mid */
static struct clksrc_clk exynos5_clk_mout_aclk_300_gscl_mid = {
	.clk	= {
		.name		= "mout_aclk_300_gscl_mid",
	},
	.sources = &exynos5_clkset_aclk,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 24, .size = 1 },
};

/* For ACLK_300_gscl */
struct clk *exynos5_clkset_aclk_300_gscl_list[] = {
	[0] = &exynos5_clk_mout_aclk_300_gscl_mid.clk,
	[1] = &exynos5_clk_sclk_vpll.clk,
};

struct clksrc_sources exynos5_clkset_aclk_300_gscl = {
	.sources	= exynos5_clkset_aclk_300_disp1_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_aclk_300_gscl_list),
};

static struct clksrc_clk exynos5_clk_mout_aclk_300_gscl = {
	.clk	= {
		.name		= "mout_aclk_300_gscl",
	},
	.sources = &exynos5_clkset_aclk_300_gscl,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 25, .size = 1 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP1, .shift = 12, .size = 3 },
};

/* For ACLK_266 */
static struct clksrc_clk exynos5_clk_dout_aclk_266 = {
	.clk	= {
		.name		= "dout_aclk_266",
		.parent		= &exynos5_clk_mout_mpll_user.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 16, .size = 3 },
};

static struct clk exynos5_clk_aclk_266 = {
	.name		= "aclk_266",
	.parent		= &exynos5_clk_dout_aclk_266.clk,
};

/* For ACLK_200 */
static struct clksrc_clk exynos5_clk_mout_aclk_200 = {
	.clk	= {
		.name		= "mout_aclk_200",
	},
	.sources = &exynos5_clkset_aclk,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 12, .size = 1 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 12, .size = 3 },
};

static struct clk exynos5_clk_aclk_200 = {
	.name		= "aclk_200",
	.parent		= &exynos5_clk_mout_aclk_200.clk,
};

/* For ACLK_166 */
static struct clksrc_clk exynos5_clk_mout_aclk_166 = {
	.clk	= {
		.name		= "mout_aclk_166",
	},
	.sources = &exynos5_clkset_aclk_333_166,
	.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 8, .size = 1 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 8, .size = 3 },
};

static struct clk exynos5_clk_aclk_166 = {
	.name		= "aclk_166",
	.parent		= &exynos5_clk_mout_aclk_166.clk,
};

/* For ACLK_133 */
static struct clksrc_clk exynos5_clk_dout_aclk_133 = {
	.clk	= {
		.name		= "dout_aclk_133",
		.parent = &exynos5_clk_mout_mpll_user.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 4, .size = 3 },
};

static struct clk exynos5_clk_aclk_133 = {
	.name		= "aclk_133",
	.parent		= &exynos5_clk_dout_aclk_133.clk,
};

/* For ACLK_66 */
static struct clksrc_clk exynos5_clk_dout_aclk_66_pre = {
	.clk	= {
		.name		= "aclk_66_pre",
		.parent		= &exynos5_clk_mout_mpll_user.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP1, .shift = 24, .size = 3 },
};

static struct clksrc_clk exynos5_clk_aclk_66 = {
	.clk	= {
		.name		= "aclk_66",
		.parent		= &exynos5_clk_dout_aclk_66_pre.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 0, .size = 3 },
};

/* SCLK_JPEG */
struct clksrc_clk exynos5_clk_dout_jpeg = {
	.clk	= {
		.name		= "dout_jpeg",
		.parent		= &exynos5_clk_mout_cpll.clk,
	},
	.reg_div = { .reg = EXYNOS5_CLKDIV_GEN, .shift = 4, .size = 3 },
};

static struct clk exynos5_init_clocks[] = {
	{
		.name		= "uart",
		.devname	= "s5pv210-uart.0",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "uart",
		.devname	= "s5pv210-uart.1",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "uart",
		.devname	= "s5pv210-uart.2",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "uart",
		.devname	= "s5pv210-uart.3",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "uart",
		.devname	= "s5pv210-uart.4",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "uart",
		.devname	= "s5pv210-uart.5",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 5),
	}
};

static struct clk exynos5_init_clocks_off[] = {
	{
		.name		= "timers",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1<<24),
	}, {
		.name		= "rtc",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peris_ctrl,
		.ctrlbit	= (1<<20),
	}, {
		.name		= "hsmmc",
		.devname	= "s3c-sdhci.0",
		.parent		= &exynos5_clk_mout_aclk_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "hsmmc",
		.devname	= "s3c-sdhci.1",
		.parent		= &exynos5_clk_mout_aclk_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "hsmmc",
		.devname	= "s3c-sdhci.2",
		.parent		= &exynos5_clk_mout_aclk_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 14),
	}, {
		.name		= "hsmmc",
		.devname	= "s3c-sdhci.3",
		.parent		= &exynos5_clk_mout_aclk_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 15),
	}, {
		.name		= "dwmci",
		.parent		= &exynos5_clk_mout_aclk_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= "sata",
		.devname	= "ahci",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "sata_phy",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 24),
	}, {
		.name		= "sata_phy_i2c",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 25),
	}, {
		.name		= "lcd",
		.devname	= "s3cfb.0",
		.enable		= exynos5_clk_ip_disp0_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "lcd",
		.devname	= "s3cfb.1",
		.enable		= exynos5_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "dp",
		.devname	= "s5p-dp",
		.enable		= exynos5_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "mfc",
		.devname	= "s3c-mfc",
		.enable		= exynos5_clk_ip_mfc_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "hdmi",
		.devname	= "exynos5-hdmi",
		.enable		= exynos5_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "mixer",
		.devname	= "s5p-mixer",
		.enable		= exynos5_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 5),
	}, {
		.name           = "hdmiphy",
		.devname	= "exynos5-hdmi",
		.enable         = exynos5_clk_hdmiphy_ctrl,
		.ctrlbit        = (1 << 0),
	}, {
		.name		= "gscl",
		.devname	= "exynos-gsc.0",
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "gscl",
		.devname	= "exynos-gsc.1",
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "gscl",
		.devname	= "exynos-gsc.2",
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "gscl",
		.devname	= "exynos-gsc.3",
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "jpeg",
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "dsim0",
		.enable		= exynos5_clk_ip_disp1_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.1",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.2",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 21),
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.1",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 22),
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.2",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 23),
	}, {
		.name		= "spdif",
		.devname	= "samsung-spdif",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 26),
	}, {
		.name		= "ac97",
		.devname	= "samsung-ac97",
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 27),
	}, {
		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(mfc_l, 13),
		.enable		= exynos5_clk_ip_mfc_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(mfc_r, 14),
		.enable		= exynos5_clk_ip_mfc_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(2d, 9),
		.enable		= exynos5_clk_ip_acp_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(jpeg, 5),
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(gsc0, 20),
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(gsc1, 21),
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 8),
	}, {

		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(gsc2, 22),
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(gsc3, 23),
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 10),
	},
};

static struct clk exynos5_i2cs_clocks[] = {
	{
		.name		= "i2c",
		.devname	= "s3c2440-i2c.0",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.1",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.2",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.3",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.4",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 10),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.5",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 11),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.6",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.7",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-hdmiphy-i2c",
		.parent		= &exynos5_clk_aclk_66.clk,
		.enable		= exynos5_clk_ip_peric_ctrl,
		.ctrlbit	= (1 << 14),
	}
};

struct clk exynos5_init_dmaclocks[] = {
	{
		.name		= "pdma",
		.devname	= "s3c-pl330.0",
		.enable		= exynos5_clk_ip_gen_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "pdma",
		.devname	= "s3c-pl330.1",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "pdma",
		.devname	= "s3c-pl330.2",
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 1),
	},
};

static struct clk *clkset_sclk_audio0_list[] = {
	[0] = &exynos5_clk_audiocdclk0.clk,
	[1] = &clk_ext_xtal_mux,
	[2] = &exynos5_clk_sclk_hdmi27m,
	[3] = &exynos5_clk_sclk_dptxphy,
	[4] = &exynos5_clk_sclk_usbphy,
	[5] = &exynos5_clk_mout_mpll.clk,
	[6] = &exynos5_clk_mout_epll.clk,
	[7] = &exynos5_clk_sclk_vpll.clk,
	[8] = &exynos5_clk_mout_cpll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_audio0 = {
	.sources	= clkset_sclk_audio0_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_audio0_list),
};

static struct clksrc_clk exynos5_clk_sclk_audio0 = {
	.clk	= {
		.name		= "audio-bus",
		.enable		= exynos5_clksrc_mask_maudio_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos5_clkset_sclk_audio0,
	.reg_src = { .reg = EXYNOS5_CLKSRC_MAUDIO, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_MAUDIO, .shift = 0, .size = 4 },
};

static struct clk *exynos5_clkset_mout_audss_list[] = {
	&clk_ext_xtal_mux,
	&clk_fout_epll,
};

static struct clksrc_sources clkset_mout_audss = {
	.sources	= exynos5_clkset_mout_audss_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_mout_audss_list),
};

static struct clksrc_clk exynos5_clk_mout_audss = {
	.clk	= {
		.name		= "mout_audss",
	},
	.sources = &clkset_mout_audss,
	.reg_src = { .reg = S5P_CLKSRC_AUDSS, .shift = 0, .size = 1 },
};

static struct clk *exynos5_clkset_sclk_audss_list[] = {
	&exynos5_clk_mout_audss.clk,
	&exynos5_clk_audiocdclk0.clk,
	&exynos5_clk_sclk_audio0.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_audss = {
	.sources	= exynos5_clkset_sclk_audss_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_audss_list),
};

static struct clksrc_clk exynos5_clk_sclk_audss_i2s = {
	.clk		= {
		.name		= "i2sclk",
		.enable		= exynos5_clk_audss_ctrl,
		.ctrlbit	= S5P_AUDSS_CLKGATE_I2SSPECIAL,
	},
	.sources = &exynos5_clkset_sclk_audss,
	.reg_src = { .reg = S5P_CLKSRC_AUDSS, .shift = 2, .size = 2 },
	.reg_div = { .reg = S5P_CLKDIV_AUDSS, .shift = 8, .size = 4 },
};

static struct clksrc_clk exynos5_clk_dout_audss_srp = {
	.clk	= {
		.name		= "dout_srp",
		.parent		= &exynos5_clk_mout_audss.clk,
	},
	.reg_div = { .reg = S5P_CLKDIV_AUDSS, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos5_clk_sclk_audss_bus = {
	.clk	= {
		.name		= "busclk",
		.parent		= &exynos5_clk_dout_audss_srp.clk,
		.enable		= exynos5_clk_audss_ctrl,
		.ctrlbit	= S5P_AUDSS_CLKGATE_I2SBUS,
	},
	.reg_div = { .reg = S5P_CLKDIV_AUDSS, .shift = 4, .size = 4 },
};

static struct clk exynos5_init_audss_clocks[] = {
	{
		.name		= "srpclk",
		.parent		= &exynos5_clk_dout_audss_srp.clk,
		.enable		= exynos5_clk_audss_ctrl,
		.ctrlbit	= S5P_AUDSS_CLKGATE_RP | S5P_AUDSS_CLKGATE_UART
				| S5P_AUDSS_CLKGATE_TIMER,
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.0",
		.enable		= exynos5_clk_audss_ctrl,
		.ctrlbit	= S5P_AUDSS_CLKGATE_I2SSPECIAL | S5P_AUDSS_CLKGATE_I2SBUS,
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.0",
		.enable		= exynos5_clk_audss_ctrl,
		.ctrlbit	= S5P_AUDSS_CLKGATE_PCMSPECIAL | S5P_AUDSS_CLKGATE_PCMBUS,
	},
};

static struct clk *exynos5_clkset_sclk_audio1_list[] = {
	[0] = &exynos5_clk_audiocdclk1,
	[1] = &clk_ext_xtal_mux,
	[2] = &exynos5_clk_sclk_hdmi27m,
	[3] = &exynos5_clk_sclk_dptxphy,
	[4] = &exynos5_clk_sclk_usbphy,
	[5] = &exynos5_clk_mout_mpll.clk,
	[6] = &exynos5_clk_mout_epll.clk,
	[7] = &exynos5_clk_sclk_vpll.clk,
	[8] = &exynos5_clk_mout_cpll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_audio1 = {
	.sources	= exynos5_clkset_sclk_audio1_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_audio1_list),
};

static struct clksrc_clk exynos5_clk_sclk_audio1 = {
	.clk	= {
		.name		= "audio-bus",
		.enable		= exynos5_clksrc_mask_peric1_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos5_clkset_sclk_audio1,
	.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC1, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC4, .shift = 4, .size = 8 },
};

static struct clk *exynos5_clkset_sclk_audio2_list[] = {
	[0] = &exynos5_clk_audiocdclk2,
	[1] = &clk_ext_xtal_mux,
	[2] = &exynos5_clk_sclk_hdmi27m,
	[3] = &exynos5_clk_sclk_dptxphy,
	[4] = &exynos5_clk_sclk_usbphy,
	[5] = &exynos5_clk_mout_mpll.clk,
	[6] = &exynos5_clk_mout_epll.clk,
	[7] = &exynos5_clk_sclk_vpll.clk,
	[8] = &exynos5_clk_mout_cpll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_audio2 = {
	.sources	= exynos5_clkset_sclk_audio2_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_audio2_list),
};

static struct clksrc_clk exynos5_clk_sclk_audio2 = {
	.clk	= {
		.name		= "audio-bus",
		.enable		= exynos5_clksrc_mask_peric1_ctrl,
		.ctrlbit	= (1 << 4),
	},
	.sources = &exynos5_clkset_sclk_audio2,
	.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC1, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC4, .shift = 16, .size = 4 },
};

static struct clk *exynos5_clkset_sclk_spdif_list[] = {
	[0] = &exynos5_clk_sclk_audio0.clk,
	[1] = &exynos5_clk_sclk_audio1.clk,
	[2] = &exynos5_clk_sclk_audio2.clk,
	[3] = &exynos5_clk_spdifcdclk,
};

static struct clksrc_sources exynos5_clkset_sclk_spdif = {
	.sources	= exynos5_clkset_sclk_spdif_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_spdif_list),
};

static struct clksrc_clk exynos5_clk_sclk_spdif = {
	.clk	= {
		.name		= "sclk_spdif",
		.enable		= exynos5_clksrc_mask_peric1_ctrl,
		.ctrlbit	= (1 << 8),
		.ops		= &s5p_sclk_spdif_ops,
	},
	.sources = &exynos5_clkset_sclk_spdif,
	.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC1, .shift = 8, .size = 2 },
};

struct clk *exynos5_clkset_group_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = NULL,
	[2] = &exynos5_clk_sclk_hdmi24m,
	[3] = &exynos5_clk_sclk_dptxphy,
	[4] = &exynos5_clk_sclk_usbphy,
	[5] = &exynos5_clk_sclk_hdmiphy,
	[6] = &exynos5_clk_mout_mpll_user.clk,
	[7] = &exynos5_clk_mout_epll.clk,
	[8] = &exynos5_clk_sclk_vpll.clk,
	[9] = &exynos5_clk_mout_cpll.clk,
};

struct clksrc_sources exynos5_clkset_group = {
	.sources	= exynos5_clkset_group_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_group_list),
};

/* Possible clock sources for aclk_266_gscl_sub Mux */
static struct clk *clk_src_gscl_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_aclk_266,
};

static struct clksrc_sources clk_src_gscl = {
	.sources	= clk_src_gscl_list,
	.nr_sources	= ARRAY_SIZE(clk_src_gscl_list),
};


static struct clksrc_clk exynos5_clk_dout_mmc0 = {
	.clk		= {
		.name		= "dout_mmc0",
	},
	.sources = &exynos5_clkset_group,
	.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS1, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos5_clk_dout_mmc1 = {
	.clk		= {
		.name		= "dout_mmc1",
	},
	.sources = &exynos5_clkset_group,
	.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS1, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos5_clk_dout_mmc2 = {
	.clk		= {
		.name		= "dout_mmc2",
	},
	.sources = &exynos5_clkset_group,
	.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 8, .size = 4 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS2, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos5_clk_dout_mmc3 = {
	.clk		= {
		.name		= "dout_mmc3",
	},
	.sources = &exynos5_clkset_group,
	.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 12, .size = 4 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS2, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos5_clk_dout_mmc4 = {
	.clk		= {
		.name		= "dout_mmc4",
	},
	.sources = &exynos5_clkset_group,
	.reg_src = { .reg = EXYNOS5_CLKSRC_FSYS, .shift = 16, .size = 4 },
	.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS3, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos5_clksrcs[] = {
	{
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.0",
			.enable		= exynos5_clksrc_mask_peric0_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.sources = &exynos5_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 0, .size = 4 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC0, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.1",
			.enable		= exynos5_clksrc_mask_peric0_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.sources = &exynos5_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 4, .size = 4 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC0, .shift = 4, .size = 4 },
	}, {
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.2",
			.enable		= exynos5_clksrc_mask_peric0_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.sources = &exynos5_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 8, .size = 4 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC0, .shift = 8, .size = 4 },
	}, {
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.3",
			.enable		= exynos5_clksrc_mask_peric0_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.sources = &exynos5_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_PERIC0, .shift = 12, .size = 4 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_PERIC0, .shift = 12, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "s3c-sdhci.0",
			.parent		= &exynos5_clk_dout_mmc0.clk,
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS1, .shift = 8, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "s3c-sdhci.1",
			.parent         = &exynos5_clk_dout_mmc1.clk,
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS1, .shift = 24, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "s3c-sdhci.2",
			.parent         = &exynos5_clk_dout_mmc2.clk,
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS2, .shift = 8, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "s3c-sdhci.3",
			.parent         = &exynos5_clk_dout_mmc3.clk,
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS2, .shift = 24, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_dwmci",
			.parent         = &exynos5_clk_dout_mmc4.clk,
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 16),
		},
		.reg_div = { .reg = EXYNOS5_CLKDIV_FSYS3, .shift = 8, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_fimd",
			.devname	= "s3cfb.0",
			.enable		= exynos5_clksrc_mask_disp0_0_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.sources = &exynos5_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_DISP0_0, .shift = 0, .size = 4 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_DISP0_0, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimd",
			.devname	= "s3cfb.1",
			.enable		= exynos5_clksrc_mask_disp1_0_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.sources = &exynos5_clkset_group,
		.reg_src = { .reg = EXYNOS5_CLKSRC_DISP1_0, .shift = 0, .size = 4 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_DISP1_0, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_gscl",
			.parent		= &exynos5_clk_aclk_266,
		},
		.sources = &clk_src_gscl,
		.reg_src = { .reg = EXYNOS5_CLKSRC_TOP3, .shift = 8, .size = 1 },
	}, {
		.clk	= {
			.name		= "sclk_g3d",
			.devname	= "mali-t604.0",
			.enable		= exynos5_clk_gate_block,
			.ctrlbit	= (1 << 1),
		},
		.sources = &exynos5_clkset_aclk,
		.reg_src = { .reg = EXYNOS5_CLKSRC_TOP0, .shift = 20, .size = 1 },
		.reg_div = { .reg = EXYNOS5_CLKDIV_TOP0, .shift = 24, .size = 3 },
	},
};

/* Clock initialization code */
static struct clksrc_clk *exynos5_sysclks[] = {
	&exynos5_clk_audiocdclk0,
	&exynos5_clk_mout_apll,
	&exynos5_clk_sclk_apll,
	&exynos5_clk_mout_bpll,
	&exynos5_clk_mout_bpll_user,
	&exynos5_clk_mout_cpll,
	&exynos5_clk_mout_epll,
	&exynos5_clk_mout_mpll,
	&exynos5_clk_mout_mpll_user,
	&exynos5_clk_mout_vpllsrc,
	&exynos5_clk_sclk_vpll,
	&exynos5_clk_mout_cpu,
	&exynos5_clk_dout_armclk,
	&exynos5_clk_dout_arm2clk,
	&exynos5_clk_mout_cdrex,
	&exynos5_clk_dout_aclk_cdrex,
	&exynos5_clk_mout_aclk_400,
	&exynos5_clk_mout_aclk_333,
	&exynos5_clk_mout_aclk_300_disp1_mid,
	&exynos5_clk_mout_aclk_300_disp1,
	&exynos5_clk_mout_aclk_300_gscl_mid,
	&exynos5_clk_mout_aclk_300_gscl,
	&exynos5_clk_dout_aclk_266,
	&exynos5_clk_mout_aclk_200,
	&exynos5_clk_mout_aclk_166,
	&exynos5_clk_dout_aclk_133,
	&exynos5_clk_dout_aclk_66_pre,
	&exynos5_clk_aclk_66,
	&exynos5_clk_dout_mmc0,
	&exynos5_clk_dout_mmc1,
	&exynos5_clk_dout_mmc2,
	&exynos5_clk_dout_mmc3,
	&exynos5_clk_dout_mmc4,
	&exynos5_clk_mout_audss,
	&exynos5_clk_sclk_audss_bus,
	&exynos5_clk_sclk_audss_i2s,
	&exynos5_clk_dout_audss_srp,
	&exynos5_clk_sclk_audio0,
	&exynos5_clk_sclk_audio1,
	&exynos5_clk_sclk_audio2,
	&exynos5_clk_sclk_spdif,
	&exynos5_clk_dout_jpeg,
};

static unsigned long exynos5_epll_get_rate(struct clk *clk)
{
	return clk->rate;
}

static struct clk *exynos5_clks[] __initdata = {
	&exynos5_clk_sclk_hdmi27m,
	&exynos5_clk_sclk_hdmiphy,
	&clk_fout_bpll,
	&clk_fout_cpll,
};

static u32 epll_div[][6] = {
	{ 192000000, 0, 48, 3, 1, 0 },
	{ 180000000, 0, 45, 3, 1, 0 },
	{  73728000, 1, 73, 3, 3, 47710 },
	{  67737600, 1, 90, 4, 3, 20762 },
	{  49152000, 0, 49, 3, 3, 9961 },
	{  45158400, 0, 45, 3, 3, 10381 },
	{ 180633600, 0, 45, 3, 1, 10381 },
};

static int exynos5_epll_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int epll_con, epll_con_k;
	unsigned int i;
	unsigned int tmp;
	unsigned int epll_rate;
	unsigned int locktime;
	unsigned int lockcnt;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	if (clk->parent)
		epll_rate = clk_get_rate(clk->parent);
	else
		epll_rate = clk_ext_xtal_mux.rate;

	if (epll_rate != 24000000) {
		pr_err("Invalid Clock : recommended clock is 24MHz.\n");
		return -EINVAL;
	}

	epll_con = __raw_readl(EXYNOS5_EPLL_CON0);
	epll_con &= ~(0x1 << 27 | \
			PLL46XX_MDIV_MASK << PLL46XX_MDIV_SHIFT |   \
			PLL46XX_PDIV_MASK << PLL46XX_PDIV_SHIFT | \
			PLL46XX_SDIV_MASK << PLL46XX_SDIV_SHIFT);

	for (i = 0; i < ARRAY_SIZE(epll_div); i++) {
		if (epll_div[i][0] == rate) {
			epll_con_k = epll_div[i][5] << 0;
			epll_con |= epll_div[i][1] << 27;
			epll_con |= epll_div[i][2] << PLL46XX_MDIV_SHIFT;
			epll_con |= epll_div[i][3] << PLL46XX_PDIV_SHIFT;
			epll_con |= epll_div[i][4] << PLL46XX_SDIV_SHIFT;
			break;
		}
	}

	if (i == ARRAY_SIZE(epll_div)) {
		printk(KERN_ERR "%s: Invalid Clock EPLL Frequency\n",
				__func__);
		return -EINVAL;
	}

	epll_rate /= 1000000;

	/* 3000 max_cycls : specification data */
	locktime = 3000 / epll_rate * epll_div[i][3];
	lockcnt = locktime * 10000 / (10000 / epll_rate);

	__raw_writel(lockcnt, EXYNOS5_EPLL_LOCK);

	__raw_writel(epll_con, EXYNOS5_EPLL_CON0);
	__raw_writel(epll_con_k, EXYNOS5_EPLL_CON1);

	do {
		tmp = __raw_readl(EXYNOS5_EPLL_CON0);
	} while (!(tmp & 0x1 << EXYNOS5_EPLLCON0_LOCKED_SHIFT));

	clk->rate = rate;

	return 0;
}

static struct clk_ops exynos5_epll_ops = {
	.get_rate = exynos5_epll_get_rate,
	.set_rate = exynos5_epll_set_rate,
};

static int xtal_rate;

static unsigned long exynos5_fout_apll_get_rate(struct clk *clk)
{
	return s5p_get_pll35xx(xtal_rate, __raw_readl(EXYNOS5_APLL_CON0), pll_3500);
}

static struct clk_ops exynos5_fout_apll_ops = {
	.get_rate = exynos5_fout_apll_get_rate,
};

void __init_or_cpufreq exynos5_setup_clocks(void)
{
	struct clk *xtal_clk;
	unsigned long apll;
	unsigned long bpll;
	unsigned long cpll;
	unsigned long mpll;
	unsigned long epll;
	unsigned long vpll;
	unsigned long vpllsrc;
	unsigned long xtal;
	unsigned long armclk;
	unsigned long mout_cdrex;
	unsigned long aclk_400;
	unsigned long aclk_333;
	unsigned long aclk_266;
	unsigned long aclk_200;
	unsigned long aclk_166;
	unsigned long aclk_133;
	unsigned long aclk_66;
	unsigned int ptr;

	printk(KERN_DEBUG "%s: registering clocks\n", __func__);

	xtal_clk = clk_get(NULL, "xtal");
	BUG_ON(IS_ERR(xtal_clk));

	xtal = clk_get_rate(xtal_clk);

	xtal_rate = xtal;

	clk_put(xtal_clk);

	printk(KERN_DEBUG "%s: xtal is %ld\n", __func__, xtal);

	apll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_APLL_CON0), pll_3500);
	bpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_BPLL_CON0), pll_3500);
	cpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_CPLL_CON0), pll_3500);
	mpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS5_MPLL_CON0), pll_3500);
	epll = s5p_get_pll36xx(xtal, __raw_readl(EXYNOS5_EPLL_CON0),
			__raw_readl(EXYNOS5_EPLL_CON1), pll_3600);

	vpllsrc = clk_get_rate(&exynos5_clk_mout_vpllsrc.clk);
	vpll = s5p_get_pll36xx(vpllsrc, __raw_readl(EXYNOS5_VPLL_CON0),
			__raw_readl(EXYNOS5_VPLL_CON1), pll_3600);

	clk_fout_apll.ops = &exynos5_fout_apll_ops;
	clk_fout_bpll.rate = bpll;
	clk_fout_cpll.rate = cpll;
	clk_fout_mpll.rate = mpll;
	clk_fout_epll.rate = epll;
	clk_fout_vpll.rate = vpll;

	printk(KERN_INFO "EXYNOS5: PLL settings, A=%ld, B=%ld, C=%ld\n"
			"M=%ld, E=%ld V=%ld",
			apll, bpll, cpll, mpll, epll, vpll);

	armclk = clk_get_rate(&exynos5_clk_armclk);
	mout_cdrex = clk_get_rate(&exynos5_clk_mclk_cdrex);

	aclk_400 = clk_get_rate(&exynos5_clk_aclk_400);
	aclk_333 = clk_get_rate(&exynos5_clk_aclk_333);
	aclk_266 = clk_get_rate(&exynos5_clk_aclk_266);
	aclk_200 = clk_get_rate(&exynos5_clk_aclk_200);
	aclk_166 = clk_get_rate(&exynos5_clk_aclk_166);
	aclk_133 = clk_get_rate(&exynos5_clk_aclk_133);
	aclk_66 = clk_get_rate(&exynos5_clk_aclk_66.clk);

	printk(KERN_INFO "EXYNOS5: ARMCLK=%ld, CDREX=%ld, ACLK400=%ld\n"
			"ACLK333=%ld, ACLK266=%ld, ACLK200=%ld\n"
			"ACLK166=%ld, ACLK133=%ld, ACLK66=%ld\n",
			armclk, mout_cdrex, aclk_400,
			aclk_333, aclk_266, aclk_200,
			aclk_166, aclk_133, aclk_66);


	clk_fout_epll.ops = &exynos5_epll_ops;

	if (clk_set_parent(&exynos5_clk_mout_audss.clk, &clk_fout_epll))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				clk_fout_epll.name, exynos5_clk_mout_audss.clk.name);
#if defined(CONFIG_SND_SAMSUNG_PCM) && !defined(CONFIG_SND_SAMSUNG_PCM_USE_EPLL)
	if (clk_set_parent(&exynos5_clk_sclk_audio0.clk, &exynos5_clk_audiocdclk0.clk))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				exynos5_clk_audiocdclk0.clk.name, exynos5_clk_sclk_audio0.clk.name);
#else
	if (clk_set_parent(&exynos5_clk_sclk_audio0.clk, &exynos5_clk_mout_epll.clk))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				exynos5_clk_mout_epll.clk.name, exynos5_clk_sclk_audio0.clk.name);
#endif
	if (clk_set_parent(&exynos5_clk_sclk_audio1.clk, &exynos5_clk_mout_epll.clk))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				exynos5_clk_mout_epll.clk.name, exynos5_clk_sclk_audio1.clk.name);
	if (clk_set_parent(&exynos5_clk_sclk_audio2.clk, &exynos5_clk_mout_epll.clk))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				exynos5_clk_mout_epll.clk.name, exynos5_clk_sclk_audio2.clk.name);
	if (clk_set_parent(&exynos5_clk_mout_epll.clk, &clk_fout_epll))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				clk_fout_epll.name, exynos5_clk_mout_epll.clk.name);

	clk_set_rate(&exynos5_clk_sclk_apll.clk, 100000000);

	for (ptr = 0; ptr < ARRAY_SIZE(exynos5_clksrcs); ptr++)
		s3c_set_clksrc(&exynos5_clksrcs[ptr], true);
}

void __init exynos5_register_clocks(void)
{
	int ptr;

	s3c24xx_register_clocks(exynos5_clks, ARRAY_SIZE(exynos5_clks));

	for (ptr = 0; ptr < ARRAY_SIZE(exynos5_sysclks); ptr++)
		s3c_register_clksrc(exynos5_sysclks[ptr], 1);

	for (ptr = 0; ptr < ARRAY_SIZE(exynos5_sclk_tv); ptr++)
		s3c_register_clksrc(exynos5_sclk_tv[ptr], 1);

	s3c_register_clksrc(exynos5_clksrcs, ARRAY_SIZE(exynos5_clksrcs));
	s3c_register_clocks(exynos5_init_clocks, ARRAY_SIZE(exynos5_init_clocks));

	s3c_register_clocks(exynos5_init_clocks_off, ARRAY_SIZE(exynos5_init_clocks_off));
	s3c_disable_clocks(exynos5_init_clocks_off, ARRAY_SIZE(exynos5_init_clocks_off));

	s3c_register_clocks(exynos5_init_audss_clocks, ARRAY_SIZE(exynos5_init_audss_clocks));
	s3c_disable_clocks(exynos5_init_audss_clocks, ARRAY_SIZE(exynos5_init_audss_clocks));

	s3c_register_clocks(exynos5_init_dmaclocks, ARRAY_SIZE(exynos5_init_dmaclocks));
	s3c_disable_clocks(exynos5_init_dmaclocks, ARRAY_SIZE(exynos5_init_dmaclocks));

	s3c_register_clocks(exynos5_i2cs_clocks, ARRAY_SIZE(exynos5_i2cs_clocks));
	s3c_disable_clocks(exynos5_i2cs_clocks, ARRAY_SIZE(exynos5_i2cs_clocks));

	s3c_pwmclk_init();
}