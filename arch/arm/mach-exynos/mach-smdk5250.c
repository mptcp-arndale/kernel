/* linux/arch/arm/mach-exynos/mach-smdk5250.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/max8997.h>
#include <linux/mmc/host.h>
#include <linux/cma.h>
#include <linux/memblock.h>
#include <linux/fb.h>
#include <linux/smsc911x.h>
#include <linux/delay.h>

#include <video/platform_lcd.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <plat/gpio-cfg.h>
#include <plat/regs-serial.h>
#include <plat/exynos5.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/sdhci.h>
#include <plat/regs-srom.h>
#include <plat/fb.h>
#include <plat/fb-s5p.h>
#include <plat/fb-core.h>
#include <plat/regs-fb-v4.h>
#include <plat/iic.h>
#include <plat/pd.h>

#include <mach/map.h>
#include <mach/exynos-ion.h>
#include <mach/dev-sysmmu.h>
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
#include <mach/dwmci.h>
#endif

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SMDK5250_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SMDK5250_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SMDK5250_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg smdk5250_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
};

static struct resource smdk5250_smsc911x_resources[] = {
	[0] = {
		.start	= EXYNOS4_PA_SROM_BANK(1),
		.end	= EXYNOS4_PA_SROM_BANK(1) + SZ_64K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_EINT(5),
		.end	= IRQ_EINT(5),
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_LOW,
	},
};

static struct smsc911x_platform_config smsc9215_config = {
	.irq_polarity	= SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type	= SMSC911X_IRQ_TYPE_PUSH_PULL,
	.flags		= SMSC911X_USE_16BIT | SMSC911X_FORCE_INTERNAL_PHY,
	.phy_interface	= PHY_INTERFACE_MODE_MII,
	.mac		= {0x00, 0x80, 0x00, 0x23, 0x45, 0x67},
};

static struct platform_device smdk5250_smsc911x = {
	.name		= "smsc911x",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(smdk5250_smsc911x_resources),
	.resource	= smdk5250_smsc911x_resources,
	.dev		= {
		.platform_data	= &smsc9215_config,
	},
};

#ifdef CONFIG_FB_S3C
#if defined(CONFIG_LCD_MIPI_S6E8AB0)
static void mipi_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	/* reset */
	gpio_request_one(EXYNOS5_GPX1(5), GPIOF_OUT_INIT_HIGH, "GPX1");

	mdelay(5);
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPX1(5), 0);
		mdelay(5);
		gpio_set_value(EXYNOS5_GPX1(5), 1);
		mdelay(5);
		gpio_free(EXYNOS5_GPX1(5));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPX1(5), 0);
		mdelay(5);
		gpio_set_value(EXYNOS5_GPX1(5), 1);
		mdelay(5);
		gpio_free(EXYNOS5_GPX1(5));
	}
	/* power */
	gpio_request_one(EXYNOS5_GPX3(0), GPIOF_OUT_INIT_LOW, "GPX3");
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPX3(0), 1);
		gpio_free(EXYNOS5_GPX3(0));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPX3(0), 0);
		gpio_free(EXYNOS5_GPX3(0));
	}

	/* backlight */
	gpio_request_one(EXYNOS5_GPB2(0), GPIOF_OUT_INIT_LOW, "GPB2");
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPB2(0), 1);
		gpio_free(EXYNOS5_GPB2(0));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPB2(0), 0);
		gpio_free(EXYNOS5_GPB2(0));
	}
}

static struct plat_lcd_data smdk5250_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
};

static struct platform_device smdk5250_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.parent		= &s5p_device_fimd1.dev,
	.dev.platform_data	= &smdk5250_mipi_lcd_data,
};

static struct s3c_fb_pd_win smdk5250_fb_win0 = {
	.win_mode = {
		.left_margin	= 0xa,
		.right_margin	= 0xa,
		.upper_margin	= 80,
		.lower_margin	= 48,
		.hsync_len	= 32,
		.vsync_len	= 5,
		.xres		= 1280,
		.yres		= 800,
	},
	.virtual_x		= 1280,
	.virtual_y		= 800 * 2,
	.width			= 223,
	.height			= 125,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win smdk5250_fb_win1 = {
	.win_mode = {
		.left_margin	= 0xa,
		.right_margin	= 0xa,
		.upper_margin	= 80,
		.lower_margin	= 48,
		.hsync_len	= 32,
		.vsync_len	= 5,
		.xres		= 1280,
		.yres		= 800,
	},
	.virtual_x		= 1280,
	.virtual_y		= 800 * 2,
	.width			= 223,
	.height			= 125,
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_pd_win smdk5250_fb_win2 = {
	.win_mode = {
		.left_margin	= 0xa,
		.right_margin	= 0xa,
		.upper_margin	= 80,
		.lower_margin	= 48,
		.hsync_len	= 32,
		.vsync_len	= 5,
		.xres		= 1280,
		.yres		= 800,
	},
	.virtual_x		= 1280,
	.virtual_y		= 800 * 2,
	.width			= 223,
	.height			= 125,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#endif

static void exynos_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;

	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 *
	 * FIMD of DISP1_BLK Bypass selection : DISP1BLK_CFG[15]
	 * ---------------------
	 *  0 | MIE/MDNIE
	 *  1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1 << 15);	/* To save other reset values */
	reg |= (1 << 15);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
}

static struct s3c_fb_platdata smdk5250_lcd1_pdata __initdata = {
	.win[0]		= &smdk5250_fb_win0,
	.win[1]		= &smdk5250_fb_win1,
	.win[2]		= &smdk5250_fb_win2,
	.default_win	= 2,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
#if defined(CONFIG_LCD_MIPI_S6E8AB0)
	.vidcon1	= VIDCON1_INV_VCLK,
#endif
	.setup_gpio	= exynos_fimd_gpio_setup_24bpp,
};
#endif

#ifdef CONFIG_EXYNOS4_DEV_DWMCI
static void exynos_dwmci_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS5_GPC0(0); gpio < EXYNOS5_GPC0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS5_GPC1(3); gpio <= EXYNOS5_GPC1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(4));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
	case 4:
		for (gpio = EXYNOS5_GPC0(3); gpio <= EXYNOS5_GPC0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
		break;
	case 1:
		gpio = EXYNOS5_GPC0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	default:
		break;
	}
}

static struct dw_mci_board exynos_dwmci_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION | DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 66 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR | MMC_CAP_8_BIT_DATA,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci_cfg_gpio,
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
static struct s3c_sdhci_platdata smdk5250_hsmmc0_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
#ifdef CONFIG_EXYNOS4_SDHCI_CH0_8BIT
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA,
#endif
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC1
static struct s3c_sdhci_platdata smdk5250_hsmmc1_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC2
static struct s3c_sdhci_platdata smdk5250_hsmmc2_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
#ifdef CONFIG_EXYNOS4_SDHCI_CH2_8BIT
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA,
#endif
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC3
static struct s3c_sdhci_platdata smdk5250_hsmmc3_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
};
#endif

/* max8997 */
static struct regulator_consumer_supply max8997_buck1 =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply max8997_buck2 =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply max8997_buck3 =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply max8997_buck4 =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply __initdata ldo2_consumer =
	REGULATOR_SUPPLY("vdd_ldo2", NULL);

static struct regulator_consumer_supply __initdata ldo3_consumer =
	REGULATOR_SUPPLY("vdd_ldo3", NULL);

static struct regulator_consumer_supply __initdata ldo4_consumer =
	REGULATOR_SUPPLY("vdd_ldo4", NULL);

static struct regulator_consumer_supply __initdata ldo5_consumer =
	REGULATOR_SUPPLY("vdd_ldo5", NULL);

static struct regulator_consumer_supply __initdata ldo6_consumer =
	REGULATOR_SUPPLY("vdd_ldo6", NULL);

static struct regulator_consumer_supply __initdata ldo7_consumer =
	REGULATOR_SUPPLY("vdd_ldo7", NULL);

static struct regulator_consumer_supply __initdata ldo8_consumer =
	REGULATOR_SUPPLY("vdd_ldo8", NULL);

static struct regulator_consumer_supply __initdata ldo9_consumer =
	REGULATOR_SUPPLY("vdd_ldo9", NULL);

static struct regulator_consumer_supply __initdata ldo10_consumer =
	REGULATOR_SUPPLY("vdd_ldo10", NULL);

static struct regulator_consumer_supply __initdata ldo11_consumer =
	REGULATOR_SUPPLY("vdd_ldo11", NULL);

static struct regulator_consumer_supply __initdata ldo14_consumer =
	REGULATOR_SUPPLY("vdd_ldo14", NULL);

static struct regulator_consumer_supply __initdata ldo21_consumer =
	REGULATOR_SUPPLY("vdd_ldo21", NULL);

static struct regulator_init_data __initdata __maybe_unused max8997_ldo2_data = {
	.constraints	= {
		.name		= "vdd_ldo2 range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo2_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo3_data = {
	.constraints	= {
		.name		= "vdd_ldo3 range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo3_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo4_data = {
	.constraints	= {
		.name		= "vdd_ldo4 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo4_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo5_data = {
	.constraints	= {
		.name		= "vdd_ldo5 range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo5_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo6_data = {
	.constraints	= {
		.name		= "vdd_ldo6 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo6_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo7_data = {
	.constraints	= {
		.name		= "vdd_ldo7 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo7_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo8_data = {
	.constraints	= {
		.name		= "vdd_ldo8 range",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo8_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo9_data = {
	.constraints	= {
		.name		= "vdd_ldo9 range",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo9_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo10_data = {
	.constraints	= {
		.name		= "vdd_ldo10 range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo10_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo11_data = {
	.constraints	= {
		.name		= "vdd_ldo11 range",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo11_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo14_data = {
	.constraints	= {
		.name		= "vdd_ldo14 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo14_consumer,
};

static struct regulator_init_data __initdata __maybe_unused max8997_ldo21_data = {
	.constraints	= {
		.name		= "vdd_ldo21 range",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo21_consumer,
};

static struct regulator_init_data __initdata max8997_buck1_data = {
	.constraints	= {
		.name		= "vdd_arm range",
		.min_uV		= 900000,
		.max_uV		= 1500000,
		.always_on	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max8997_buck1,
};

static struct regulator_init_data __initdata max8997_buck2_data = {
	.constraints	= {
		.name		= "vdd_int range",
		.min_uV		= 950000,
		.max_uV		= 1000000,
		.always_on	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max8997_buck2,
};

static struct regulator_init_data __initdata max8997_buck3_data = {
	.constraints	= {
		.name		= "vdd_g3d range",
		.min_uV		= 950000,
		.max_uV		= 1150000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max8997_buck3,
};

static struct regulator_init_data __initdata max8997_buck4_data = {
	.constraints	= {
		.name		= "vdd_mif range",
		.min_uV		= 950000,
		.max_uV		= 1200000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max8997_buck4,
};

static struct max8997_regulator_data __initdata max8997_regulators[] = {
	{ MAX8997_BUCK1, &max8997_buck1_data, },
	{ MAX8997_BUCK2, &max8997_buck2_data, },
	{ MAX8997_BUCK3, &max8997_buck3_data, },
	{ MAX8997_BUCK4, &max8997_buck4_data, },
};

static struct max8997_platform_data __initdata exynos5_max8997_info = {
	.num_regulators = ARRAY_SIZE(max8997_regulators),
	.regulators     = max8997_regulators,

	.buck1_voltage[0] = 1250000, /* 1.25V */
	.buck1_voltage[1] = 1100000, /* 1.1V */
	.buck1_voltage[2] = 1100000, /* 1.1V */
	.buck1_voltage[3] = 1100000, /* 1.1V */
	.buck1_voltage[4] = 1100000, /* 1.1V */
	.buck1_voltage[5] = 1100000, /* 1.1V */
	.buck1_voltage[6] = 1000000, /* 1.0V */
	.buck1_voltage[7] = 950000, /* 0.95V */

	.buck2_voltage[0] = 1100000, /* 1.1V */
	.buck2_voltage[1] = 1000000, /* 1.0V */
	.buck2_voltage[2] = 950000, /* 0.95V */
	.buck2_voltage[3] = 900000, /* 0.9V */
	.buck2_voltage[4] = 1000000, /* 1.0V */
	.buck2_voltage[5] = 1000000, /* 1.0V */
	.buck2_voltage[6] = 950000, /* 0.95V */
	.buck2_voltage[7] = 900000, /* 0.9V */

	.buck5_voltage[0] = 1100000, /* 1.2V */
	.buck5_voltage[1] = 1100000, /* 1.1V */
	.buck5_voltage[2] = 1100000, /* 1.1V */
	.buck5_voltage[3] = 1100000, /* 1.1V */
	.buck5_voltage[4] = 1100000, /* 1.1V */
	.buck5_voltage[5] = 1100000, /* 1.1V */
	.buck5_voltage[6] = 1100000, /* 1.1V */
	.buck5_voltage[7] = 1100000, /* 1.1V */
};

static struct regulator_consumer_supply wm8994_fixed_voltage0_supplies[] = {
	REGULATOR_SUPPLY("AVDD2", "1-001a"),
	REGULATOR_SUPPLY("CPVDD", "1-001a"),
};

static struct regulator_consumer_supply wm8994_fixed_voltage1_supplies[] = {
	REGULATOR_SUPPLY("SPKVDD1", "1-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "1-001a"),
};

static struct regulator_consumer_supply wm8994_fixed_voltage2_supplies =
	REGULATOR_SUPPLY("DBVDD", "1-001a");

static struct regulator_init_data wm8994_fixed_voltage0_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm8994_fixed_voltage0_supplies),
	.consumer_supplies	= wm8994_fixed_voltage0_supplies,
};

static struct regulator_init_data wm8994_fixed_voltage1_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm8994_fixed_voltage1_supplies),
	.consumer_supplies	= wm8994_fixed_voltage1_supplies,
};

static struct regulator_init_data wm8994_fixed_voltage2_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_fixed_voltage2_supplies,
};

static struct fixed_voltage_config wm8994_fixed_voltage0_config = {
	.supply_name	= "VDD_1.8V",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &wm8994_fixed_voltage0_init_data,
};

static struct fixed_voltage_config wm8994_fixed_voltage1_config = {
	.supply_name	= "DC_5V",
	.microvolts	= 5000000,
	.gpio		= -EINVAL,
	.init_data	= &wm8994_fixed_voltage1_init_data,
};

static struct fixed_voltage_config wm8994_fixed_voltage2_config = {
	.supply_name	= "VDD_3.3V",
	.microvolts	= 3300000,
	.gpio		= -EINVAL,
	.init_data	= &wm8994_fixed_voltage2_init_data,
};

static struct platform_device wm8994_fixed_voltage0 = {
	.name		= "reg-fixed-voltage",
	.id		= 0,
	.dev		= {
		.platform_data	= &wm8994_fixed_voltage0_config,
	},
};

static struct platform_device wm8994_fixed_voltage1 = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev		= {
		.platform_data	= &wm8994_fixed_voltage1_config,
	},
};

static struct platform_device wm8994_fixed_voltage2 = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev		= {
		.platform_data	= &wm8994_fixed_voltage2_config,
	},
};

static struct regulator_consumer_supply wm8994_avdd1_supply =
	REGULATOR_SUPPLY("AVDD1", "1-001a");

static struct regulator_consumer_supply wm8994_dcvdd_supply =
	REGULATOR_SUPPLY("DCVDD", "1-001a");

static struct regulator_init_data wm8994_ldo1_data = {
	.constraints	= {
		.name		= "AVDD1",
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_avdd1_supply,
};

static struct regulator_init_data wm8994_ldo2_data = {
	.constraints	= {
		.name		= "DCVDD",
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_dcvdd_supply,
};

static struct wm8994_pdata wm8994_platform_data = {
	/* configure gpio1 function: 0x0001(Logic level input/output) */
	.gpio_defaults[0] = 0x0001,
	/* If the i2s0 and i2s2 is enabled simultaneously */
	.gpio_defaults[7] = 0x8100, /* GPIO8  DACDAT3 in */
	.gpio_defaults[8] = 0x0100, /* GPIO9  ADCDAT3 out */
	.gpio_defaults[9] = 0x0100, /* GPIO10 LRCLK3  out */
	.gpio_defaults[10] = 0x0100,/* GPIO11 BCLK3   out */
	.ldo[0] = { 0, NULL, &wm8994_ldo1_data },
	.ldo[1] = { 0, NULL, &wm8994_ldo2_data },
};

static struct i2c_board_info i2c_devs0[] __initdata = {
	{
		I2C_BOARD_INFO("max8997", 0x66),
		.platform_data	= &exynos5_max8997_info,
	}
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	{
		I2C_BOARD_INFO("wm8994", 0x1a),
		.platform_data	= &wm8994_platform_data,
	},
};

static struct platform_device *smdk5250_devices[] __initdata = {
	/* Samsung Power Domain */
	&exynos5_device_pd[PD_MFC],
	&exynos5_device_pd[PD_G3D],
	&exynos5_device_pd[PD_GPS],
	&exynos5_device_pd[PD_ISP],
	&exynos5_device_pd[PD_GSCL],
	&exynos5_device_pd[PD_DISP1],
#ifdef CONFIG_FB_S3C
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim,
#endif
	&s5p_device_fimd1,
#ifdef CONFIG_LCD_MIPI_S6E8AB0
	&smdk5250_mipi_lcd,
#endif
#endif
	&s3c_device_i2c0,
	&s3c_device_i2c1,
#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif
#ifdef CONFIG_SND_SAMSUNG_AC97
	&exynos_device_ac97,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos_device_i2s0,
#endif
#ifdef CONFIG_SND_SAMSUNG_PCM
	&exynos_device_pcm0,
#endif
#ifdef CONFIG_SND_SAMSUNG_SPDIF
	&exynos_device_spdif,
#endif
#ifdef CONFIG_SND_SAMSUNG_RP
	&exynos_device_srp,
#endif
	&wm8994_fixed_voltage0,
	&wm8994_fixed_voltage1,
	&wm8994_fixed_voltage2,
	&samsung_asoc_dma,
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
	&exynos_device_dwmci,
#endif
#ifdef CONFIG_ION_EXYNOS
	&exynos_device_ion,
#endif
#ifdef CONFIG_S5P_SYSTEM_MMU
	&SYSMMU_PLATDEV(mfc_l),
	&SYSMMU_PLATDEV(mfc_r),
	&SYSMMU_PLATDEV(2d),
	&SYSMMU_PLATDEV(jpeg),
	&SYSMMU_PLATDEV(gsc0),
	&SYSMMU_PLATDEV(gsc1),
	&SYSMMU_PLATDEV(gsc2),
	&SYSMMU_PLATDEV(gsc3),
#endif
	&s3c_device_rtc,
	&smdk5250_smsc911x,
};

#if defined(CONFIG_S5P_MEM_CMA)
static void __init exynos_cma_region_reserve(
			struct cma_region *regions_normal,
			struct cma_region *regions_secure)
{
	struct cma_region *reg;
	size_t size_secure = 0, align_secure = 0;
	phys_addr_t paddr = 0;

	for (reg = regions_normal; reg->size != 0; reg++) {
		if ((reg->alignment & (reg->alignment - 1)) || reg->reserved)
			continue;

		if (reg->start) {
			if (!memblock_is_region_reserved(reg->start, reg->size)
			    && memblock_reserve(reg->start, reg->size) >= 0)
				reg->reserved = 1;
		} else {
			paddr = __memblock_alloc_base(reg->size, reg->alignment,
					MEMBLOCK_ALLOC_ACCESSIBLE);
			if (paddr) {
				reg->start = paddr;
				reg->reserved = 1;
			}
		}
	}

	if (regions_secure && regions_secure->size) {
		for (reg = regions_secure; reg->size != 0; reg++)
			size_secure += reg->size;

		reg--;

		align_secure = reg->alignment;
		BUG_ON(align_secure & (align_secure - 1));

		paddr -= size_secure;
		paddr &= ~(align_secure - 1);

		if (!memblock_reserve(paddr, size_secure)) {
			do {
				reg->start = paddr;
				reg->reserved = 1;
				paddr += reg->size;
			} while (reg-- != regions_secure);
		}
	}
}

static void __init exynos_reserve_mem(void)
{
	static struct cma_region regions[] = {
#ifdef CONFIG_ANDROID_PMEM_MEMSIZE_PMEM
		{
			.name = "pmem",
			.size = CONFIG_ANDROID_PMEM_MEMSIZE_PMEM * SZ_1K,
			.start = 0,
		},
#endif
#ifdef CONFIG_ANDROID_PMEM_MEMSIZE_PMEM_GPU1
		{
			.name = "pmem_gpu1",
			.size = CONFIG_ANDROID_PMEM_MEMSIZE_PMEM_GPU1 * SZ_1K,
			.start = 0,
		},
#endif
		{
			.name = "ion",
			.size = 30 * SZ_1M,
			.start = 0
		},
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_GSC0
		{
			.name = "gsc0",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_GSC0 * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_GSC1
		{
			.name = "gsc1",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_GSC1 * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_GSC2
		{
			.name = "gsc2",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_GSC2 * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_GSC3
		{
			.name = "gsc3",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_GSC3 * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD
		{
			.name = "fimd",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_S5P_MFC
		{
			.name		= "fw",
			.size		= 1 << 20,
			{ .alignment	= 128 << 10 },
			.start		= 0x44000000,
		},
		{
			.name		= "b1",
			.size		= 32 << 20,
			.start		= 0x45000000,
		},
#endif
		{
			.size = 0
		},
	};
	static const char map[] __initconst =
		"android_pmem.0=pmem;android_pmem.1=pmem_gpu1;"
		"s3cfb.0=fimd;"
		"exynos-gsc.0=gsc0;exynos-gsc.1=gsc1;exynos-gsc.2=gsc2;exynos-gsc.3=gsc3;"
		"ion-exynos=ion,gsc0,gsc1,gsc2,gsc3,fimd;"
		"s5p-mfc-v6/f=fw;"
		"s5p-mfc-v6/a=b1;";

	cma_set_defaults(regions, map);

	exynos_cma_region_reserve(regions, NULL);
}
#else /* !CONFIG_S5P_MEM_CMA */
static inline void exynos_reserve_mem(void)
{
}
#endif

static void __init smdk5250_smsc911x_init(void)
{
	u32 cs1;

	/* configure nCS1 width to 16 bits */
	cs1 = __raw_readl(S5P_SROM_BW) &
		~(S5P_SROM_BW__CS_MASK << S5P_SROM_BW__NCS1__SHIFT);
	cs1 |= ((1 << S5P_SROM_BW__DATAWIDTH__SHIFT) |
		(1 << S5P_SROM_BW__WAITENABLE__SHIFT) |
		(1 << S5P_SROM_BW__BYTEENABLE__SHIFT)) <<
		S5P_SROM_BW__NCS1__SHIFT;
	__raw_writel(cs1, S5P_SROM_BW);

	/* set timing for nCS1 suitable for ethernet chip */
	__raw_writel((0x1 << S5P_SROM_BCX__PMC__SHIFT) |
		     (0x9 << S5P_SROM_BCX__TACP__SHIFT) |
		     (0xc << S5P_SROM_BCX__TCAH__SHIFT) |
		     (0x1 << S5P_SROM_BCX__TCOH__SHIFT) |
		     (0x6 << S5P_SROM_BCX__TACC__SHIFT) |
		     (0x1 << S5P_SROM_BCX__TCOS__SHIFT) |
		     (0x1 << S5P_SROM_BCX__TACS__SHIFT), S5P_SROM_BC1);
}

static void __init smdk5250_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(smdk5250_uartcfgs, ARRAY_SIZE(smdk5250_uartcfgs));
	exynos_reserve_mem();
}

#ifdef CONFIG_S5P_SYSTEM_MMU
static void __init exynos_sysmmu_init(void)
{
}
#else /* !CONFIG_S5P_SYSTEM_MMU */
static inline void exynos_sysmmu_init(void)
{
}
#endif

static void __init smdk5250_machine_init(void)
{
	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
#if defined(CONFIG_EXYNOS_DEV_PD) && defined(CONFIG_PM_RUNTIME)
	exynos_pd_enable(&exynos5_device_pd[PD_MFC].dev);
	exynos_pd_enable(&exynos5_device_pd[PD_G3D].dev);
	exynos_pd_enable(&exynos5_device_pd[PD_GPS].dev);
	exynos_pd_enable(&exynos5_device_pd[PD_ISP].dev);
	exynos_pd_enable(&exynos5_device_pd[PD_GSCL].dev);
	exynos_pd_enable(&exynos5_device_pd[PD_DISP1].dev);
#elif defined(CONFIG_EXYNOS_DEV_PD)
	/*
	 * These power domains should be always on
	 * without runtime pm support.
	 */
	exynos_pd_enable(&exynos5_device_pd[PD_MFC].dev);
	exynos_pd_enable(&exynos5_device_pd[PD_G3D].dev);
	exynos_pd_enable(&exynos5_device_pd[PD_GPS].dev);
	exynos_pd_enable(&exynos5_device_pd[PD_ISP].dev);
	exynos_pd_enable(&exynos5_device_pd[PD_GSCL].dev);
	exynos_pd_enable(&exynos5_device_pd[PD_DISP1].dev);
#endif
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
	exynos_dwmci_set_platdata(&exynos_dwmci_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_sdhci0_set_platdata(&smdk5250_hsmmc0_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_sdhci1_set_platdata(&smdk5250_hsmmc1_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_sdhci2_set_platdata(&smdk5250_hsmmc2_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	s3c_sdhci3_set_platdata(&smdk5250_hsmmc3_pdata);
#endif
#ifdef CONFIG_ION_EXYNOS
	exynos_ion_set_platdata();
#endif

#ifdef CONFIG_FB_S3C
	dev_set_name(&s5p_device_fimd1.dev, "s3cfb.1");
	clk_add_alias("lcd", "exynos5-fb.1", "lcd", &s5p_device_fimd1.dev);
	clk_add_alias("sclk_fimd", "exynos5-fb.1", "sclk_fimd",
			&s5p_device_fimd1.dev);
	s5p_fb_setname(1, "exynos5-fb");

	s5p_fimd1_set_platdata(&smdk5250_lcd1_pdata);
#endif

	exynos_sysmmu_init();

	platform_add_devices(smdk5250_devices, ARRAY_SIZE(smdk5250_devices));

#ifdef CONFIG_FB_S3C
	exynos4_fimd_setup_clock(&s5p_device_fimd1.dev, "sclk_fimd", "mout_mpll_user",
				800 * MHZ);
#endif
	smdk5250_smsc911x_init();
}

MACHINE_START(SMDK5250, "SMDK5250")
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= exynos5_init_irq,
	.map_io		= smdk5250_map_io,
	.init_machine	= smdk5250_machine_init,
	.timer		= &exynos4_timer,
MACHINE_END