/*
 * Samsung Exynos4 SoC series FIMC-IS slave interface driver
 *
 * exynos4 fimc-is helper functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 * Contact: Younghwan Joo, <yhwan.joo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/memory.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <linux/videodev2_samsung.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <plat/gpio-cfg.h>

#include "fimc-is-core.h"
#include "fimc-is-regs.h"
#include "fimc-is-cmd.h"
#include "fimc-is-param.h"

/*
Default setting values
*/
#ifndef CONFIG_VIDEO_S5K6A3
#ifdef CONFIG_VIDEO_S5K3H1
#define PREVIEW_WIDTH		640
#define PREVIEW_HEIGHT		480
#define CAPTURE_WIDTH		3232
#define CAPTURE_HEIGHT		2424
#define CAMCORDING_WIDTH	1920
#define CAMCORDING_HEIGHT	1080
#define PREVIEW_FRAMERATE	15
#define CAPTURE_FRAMERATE	15
#define CAMCORDING_FRAMERATE	15
#endif
#endif
#ifndef CONFIG_VIDEO_S5K6A3
#ifdef CONFIG_VIDEO_S5K3H2
#define PREVIEW_WIDTH		640
#define PREVIEW_HEIGHT		480
#define CAPTURE_WIDTH		3232
#define CAPTURE_HEIGHT		2424
#define CAMCORDING_WIDTH	1920
#define CAMCORDING_HEIGHT	1080
#define PREVIEW_FRAMERATE	15
#define CAPTURE_FRAMERATE	15
#define CAMCORDING_FRAMERATE	15
#endif
#endif
#ifdef CONFIG_VIDEO_S5K6A3
#define PREVIEW_WIDTH		640
#define PREVIEW_HEIGHT		480
#define CAPTURE_WIDTH		1396
#define CAPTURE_HEIGHT		1400
#define CAMCORDING_WIDTH	1280
#define CAMCORDING_HEIGHT	720
#define PREVIEW_FRAMERATE	30
#define CAPTURE_FRAMERATE	30
#define CAMCORDING_FRAMERATE	30
#endif

static const struct sensor_param init_val_sensor_preview = {
	.frame_rate = {
		.frame_rate = PREVIEW_FRAMERATE,
	},
};

static const struct isp_param init_val_isp_preview = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = PREVIEW_WIDTH, .height = PREVIEW_HEIGHT,
#ifndef ISP_STRGEN
		.format = OTF_INPUT_FORMAT_BAYER,
#else
		.format = OTF_INPUT_FORMAT_STRGEN_COLORBAR_BAYER,
#endif
		.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.dma2_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.af = {
		.cmd = ISP_AF_COMMAND_SET_FOCUSMODE,
		.mode = ISP_AF_MODE_AUTO,
		.face = ISP_AF_FACE_DISABLE,
		.continuous = ISP_AF_CONTINUOUS_DISABLE,
		.win_pos_x = 0, .win_pos_y = 0,
		.win_width = 0, .win_height = 0,
		.err = ISP_AF_ERROR_NO,
	},
	.flash = {
		.cmd = ISP_FLASH_COMMAND_DISABLE,
		.redeye = ISP_FLASH_REDEYE_DISABLE,
		.err = ISP_FLASH_ERROR_NO,
	},
	.awb = {
		.cmd = ISP_AWB_COMMAND_AUTO,
		.illumination = 0,
		.err = ISP_AWB_ERROR_NO,
	},
	.effect = {
		.cmd = ISP_IMAGE_EFFECT_DISABLE,
		.err = ISP_IMAGE_EFFECT_ERROR_NO,
	},
	.iso = {
		.cmd = ISP_ISO_COMMAND_AUTO,
		.value = 0,
		.err = ISP_ISO_ERROR_NO,
	},
	.adjust = {
		.cmd = ISP_ADJUST_COMMAND_AUTOCONTRAST,
		.contrast = 0,
		.saturation = 0,
		.sharpness = 0,
		.exposure = 0,
		.brightness = 0,
		.hue = 0,
		.err = ISP_ADJUST_ERROR_NO,
	},
	.metering = {
		.cmd = ISP_METERING_COMMAND_MATRIX,
		.win_pos_x = 0, .win_pos_y = 0,
		.win_width = PREVIEW_WIDTH, .win_height = PREVIEW_HEIGHT,
		.err = ISP_METERING_ERROR_NO,
	},
	.afc = {
		.cmd = ISP_AFC_COMMAND_AUTO,
		.manual = 0, .err = ISP_AFC_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = PREVIEW_WIDTH, .height = PREVIEW_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma1_output = {
#ifndef ISP_DMA
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
#else
		.cmd = DMA_OUTPUT_COMMAND_ENABLE,
#endif
		.width = PREVIEW_WIDTH, .height = PREVIEW_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV422,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_3,
		.order = DMA_INPUT_ORDER_NO,
		.buffer_number = 1,
		.buffer_address = 0x50060400,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.dma2_output = {
#ifndef ISP_DMA
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
#else
		.cmd = DMA_OUTPUT_COMMAND_ENABLE,
#endif
		.width = PREVIEW_WIDTH, .height = PREVIEW_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_BAYER,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_10BIT,
		.plane = DMA_OUTPUT_PLANE_1,
		.order = DMA_OUTPUT_ORDER_GB_BG,
		.buffer_number = 1,
		.buffer_address = 0x501D0000,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct drc_param init_val_drc_preview = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = PREVIEW_WIDTH, .height = PREVIEW_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = PREVIEW_WIDTH, .height = PREVIEW_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct fd_param init_val_fd_preview = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = PREVIEW_WIDTH, .height = PREVIEW_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.result = {
		.max_number = 10,
		.err = FD_ERROR_NO,
	},
	.mode = {
		.smile = FD_MODE_SMILE_DISABLE,
		.blink = FD_MODE_BLINK_DISABLE,
		.err = FD_ERROR_NO,
	},
};

static const struct sensor_param init_val_sensor_capture = {
	.frame_rate = {
		.frame_rate = CAPTURE_FRAMERATE,
	},
};

static const struct isp_param init_val_isp_capture = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = CAPTURE_WIDTH, .height = CAPTURE_HEIGHT,
#ifndef ISP_STRGEN
		.format = OTF_INPUT_FORMAT_BAYER,
#else
		.format = OTF_INPUT_FORMAT_STRGEN_COLORBAR_BAYER,
#endif
		.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.dma2_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.af = {
		.cmd = ISP_AF_COMMAND_SET_FOCUSMODE,
		.mode = ISP_AF_MODE_AUTO,
		.face = ISP_AF_FACE_DISABLE,
		.continuous = ISP_AF_CONTINUOUS_DISABLE,
		.win_pos_x = 0, .win_pos_y = 0,
		.win_width = 0, .win_height = 0,
		.err = ISP_AF_ERROR_NO,
	},
	.flash = {
		.cmd = ISP_FLASH_COMMAND_DISABLE,
		.redeye = ISP_FLASH_REDEYE_DISABLE,
		.err = ISP_FLASH_ERROR_NO,
	},
	.awb = {
		.cmd = ISP_AWB_COMMAND_AUTO,
		.illumination = 0,
		.err = ISP_AWB_ERROR_NO,
	},
	.effect = {
		.cmd = ISP_IMAGE_EFFECT_DISABLE,
		.err = ISP_IMAGE_EFFECT_ERROR_NO,
	},
	.iso = {
		.cmd = ISP_ISO_COMMAND_AUTO,
		.value = 0,
		.err = ISP_ISO_ERROR_NO,
	},
	.adjust = {
		.cmd = ISP_ADJUST_COMMAND_AUTOCONTRAST,
		.contrast = 0,
		.saturation = 0,
		.sharpness = 0,
		.exposure = 0,
		.brightness = 0,
		.hue = 0,
		.err = ISP_ADJUST_ERROR_NO,
	},
	.metering = {
		.cmd = ISP_METERING_COMMAND_MATRIX,
		.win_pos_x = 0, .win_pos_y = 0,
		.win_width = CAPTURE_WIDTH, .win_height = CAPTURE_HEIGHT,
		.err = ISP_METERING_ERROR_NO,
	},
	.afc = {
		.cmd = ISP_AFC_COMMAND_AUTO,
		.manual = 0, .err = ISP_AFC_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = CAPTURE_WIDTH, .height = CAPTURE_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma1_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.dma2_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct drc_param init_val_drc_capture = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = CAPTURE_WIDTH, .height = CAPTURE_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = CAPTURE_WIDTH, .height = CAPTURE_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct fd_param init_val_fd_capture = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		/* in FD case , bypass is not available */
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = CAPTURE_WIDTH, .height = CAPTURE_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.result = {
		.max_number = 10,
		.err = FD_ERROR_NO,
	},
	.mode = {
		.smile = FD_MODE_SMILE_DISABLE,
		.blink = FD_MODE_BLINK_DISABLE,
		.err = FD_ERROR_NO,
	},
};

static const struct sensor_param init_val_sensor_camcording = {
	.frame_rate = {
		.frame_rate = CAMCORDING_FRAMERATE,
	},
};

static const struct isp_param init_val_isp_camcording = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = CAMCORDING_WIDTH, .height = CAMCORDING_HEIGHT,
#ifndef ISP_STRGEN
		.format = OTF_INPUT_FORMAT_BAYER,
#else
		.format = OTF_INPUT_FORMAT_STRGEN_COLORBAR_BAYER,
#endif
		.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.dma2_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.af = {
		.cmd = ISP_AF_COMMAND_SET_FOCUSMODE,
		.mode = ISP_AF_MODE_AUTO,
		.face = ISP_AF_FACE_DISABLE,
		.continuous = ISP_AF_CONTINUOUS_DISABLE,
		.win_pos_x = 0, .win_pos_y = 0,
		.win_width = 0, .win_height = 0,
		.err = ISP_AF_ERROR_NO,
	},
	.flash = {
		.cmd = ISP_FLASH_COMMAND_DISABLE,
		.redeye = ISP_FLASH_REDEYE_DISABLE,
		.err = ISP_FLASH_ERROR_NO,
	},
	.awb = {
		.cmd = ISP_AWB_COMMAND_AUTO,
		.illumination = 0,
		.err = ISP_AWB_ERROR_NO,
	},
	.effect = {
		.cmd = ISP_IMAGE_EFFECT_DISABLE,
		.err = ISP_IMAGE_EFFECT_ERROR_NO,
	},
	.iso = {
		.cmd = ISP_ISO_COMMAND_AUTO,
		.value = 0,
		.err = ISP_ISO_ERROR_NO,
	},
	.adjust = {
		.cmd = ISP_ADJUST_COMMAND_AUTOCONTRAST,
		.contrast = 0,
		.saturation = 0,
		.sharpness = 0,
		.exposure = 0,
		.brightness = 0,
		.hue = 0,
		.err = ISP_ADJUST_ERROR_NO,
	},
	.metering = {
		.cmd = ISP_METERING_COMMAND_MATRIX,
		.win_pos_x = 0, .win_pos_y = 0,
		.win_width = CAMCORDING_WIDTH, .win_height = CAMCORDING_HEIGHT,
		.err = ISP_METERING_ERROR_NO,
	},
	.afc = {
		.cmd = ISP_AFC_COMMAND_AUTO,
		.manual = 0, .err = ISP_AFC_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = CAMCORDING_WIDTH, .height = CAMCORDING_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma1_output = {
#ifndef ISP_DMA
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
#else
		.cmd = DMA_OUTPUT_COMMAND_ENABLE,
#endif
		.width = CAMCORDING_WIDTH, .height = CAMCORDING_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV422,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_3,
		.order = DMA_INPUT_ORDER_NO,
		.buffer_number = 1,
		.buffer_address = 0x50060400,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.dma2_output = {
#ifndef ISP_DMA
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
#else
		.cmd = DMA_OUTPUT_COMMAND_ENABLE,
#endif
		.width = CAMCORDING_WIDTH, .height = CAMCORDING_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_BAYER,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_10BIT,
		.plane = DMA_OUTPUT_PLANE_1,
		.order = DMA_OUTPUT_ORDER_GB_BG,
		.buffer_number = 1,
		.buffer_address = 0x501D0000,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct drc_param init_val_drc_camcording = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = CAMCORDING_WIDTH, .height = CAMCORDING_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = CAMCORDING_WIDTH, .height = CAMCORDING_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct fd_param init_val_fd_camcording = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = CAMCORDING_WIDTH, .height = CAMCORDING_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.result = {
		.max_number = 10,
		.err = FD_ERROR_NO,
	},
	.mode = {
		.smile = FD_MODE_SMILE_DISABLE,
		.blink = FD_MODE_BLINK_DISABLE,
		.err = FD_ERROR_NO,
	},
};
/*
 Group 1. Interrupt
*/
void fimc_is_hw_set_intgr0_gd0(struct fimc_is_dev *dev)
{
	writel(INTGR0_INTGD0, dev->regs + INTGR0);
}

int fimc_is_hw_wait_intsr0_intsd0(struct fimc_is_dev *dev)
{
	u32 cfg = readl(dev->regs + INTSR0);
	u32 status = INTSR0_GET_INTSD0(cfg);
	while (status) {
		cfg = readl(dev->regs + INTSR0);
		status = INTSR0_GET_INTSD0(cfg);
	}
	return 0;
}

int fimc_is_hw_wait_intmsr0_intmsd0(struct fimc_is_dev *dev)
{
	u32 cfg = readl(dev->regs + INTMSR0);
	u32 status = INTMSR0_GET_INTMSD0(cfg);
	while (status) {
		cfg = readl(dev->regs + INTMSR0);
		status = INTMSR0_GET_INTMSD0(cfg);
	}
	return 0;
}

int fimc_is_fw_clear_irq1(struct fimc_is_dev *dev)
{
	u32 cfg = readl(dev->regs + INTSR1);

	writel(cfg, dev->regs + INTCR1);
	return 0;
}

int fimc_is_fw_clear_irq2(struct fimc_is_dev *dev)
{
	u32 cfg = readl(dev->regs + INTSR2);

	writel(cfg, dev->regs + INTCR2);
	return 0;
}

/*
 Group 2. Common
*/
void fimc_is_hw_open_sensor(struct fimc_is_dev *dev, u32 id, u32 sensor_index)
{
	fimc_is_hw_wait_intmsr0_intmsd0(dev);
	writel(HIC_OPEN_SENSOR, dev->regs + ISSR0);
	writel(id, dev->regs + ISSR1);
	switch (sensor_index) {
	case SENSOR_S5K3H1_CSI_A:
		/* Parameter1 : Sensor Name(IS_Sensor.h) */
		writel(SENSOR_NAME_S5K3H1, dev->regs + ISSR2);
		/* Parameter2 : I2c channel used by parameter1 sensor */
		writel(SENSOR_CONTROL_I2C0, dev->regs + ISSR3);
		break;
	case SENSOR_S5K3H1_CSI_B:
		writel(SENSOR_NAME_S5K3H1, dev->regs + ISSR2);
		writel(SENSOR_CONTROL_I2C1, dev->regs + ISSR3);
		break;
	case SENSOR_S5K3H2_CSI_A:
		writel(SENSOR_NAME_S5K3H2, dev->regs + ISSR2);
		writel(SENSOR_CONTROL_I2C0, dev->regs + ISSR3);
		break;
	case SENSOR_S5K3H2_CSI_B:
		writel(SENSOR_NAME_S5K3H2, dev->regs + ISSR2);
		writel(SENSOR_CONTROL_I2C1, dev->regs + ISSR3);
		break;
	case SENSOR_S5K6A3_CSI_A:
		writel(SENSOR_NAME_S5K6A3, dev->regs + ISSR2);
		writel(SENSOR_CONTROL_I2C0, dev->regs + ISSR3);
		break;
	case SENSOR_S5K6A3_CSI_B:
		writel(SENSOR_NAME_S5K6A3, dev->regs + ISSR2);
		writel(SENSOR_CONTROL_I2C1, dev->regs + ISSR3);
		break;
	}
	/* Parameter3 : Scenario ID(Initial Scenario) */
	writel(ISS_PREVIEW_STILL, dev->regs + ISSR4);
	fimc_is_hw_set_intgr0_gd0(dev);
}

void fimc_is_hw_close_sensor(struct fimc_is_dev *dev, u32 id)
{
	if (dev->sensor.id == id) {
		fimc_is_hw_wait_intmsr0_intmsd0(dev);
		writel(HIC_CLOSE_SENSOR, dev->regs + ISSR0);
		writel(dev->sensor.id, dev->regs + ISSR1);
		writel(dev->sensor.id, dev->regs + ISSR2);
		fimc_is_hw_set_intgr0_gd0(dev);
	}
}

void fimc_is_hw_diable_wdt(struct fimc_is_dev *dev)
{
	writel(0x00008000, dev->regs + WDT);
}

int fimc_is_hw_io_init(struct fimc_is_dev *dev)
{
	struct platform_device *pdev = to_platform_device(&dev->pdev->dev);
	if (dev->pdata->cfg_gpio) {
		dev->pdata->cfg_gpio(pdev);
	} else {
		printk(KERN_ERR "#### failed to Config GPIO ####\n");
		return -EINVAL;
	}
	return 0;
}

void fimc_is_hw_subip_poweroff(struct fimc_is_dev *dev)
{
	/* 1. Make FIMC-IS power-off state */
	fimc_is_hw_wait_intmsr0_intmsd0(dev);
	writel(HIC_POWER_DOWN, dev->regs + ISSR0);
	writel(dev->sensor.id, dev->regs + ISSR1);
	fimc_is_hw_set_intgr0_gd0(dev);
}

void fimc_is_hw_a5_power(struct fimc_is_dev *dev, int on)
{
	u32 cfg;
	u32 timeout;

	if (on) {
		/* 1. A5 start address setting */
		cfg = dev->mem.base;
		writel(cfg, dev->regs + BBOAR);
		/* 2. enable A5 */
		writel(0x00018000, PMUREG_ISP_ARM_OPTION);
		/* 3. A5 power on*/
		writel(0x1, PMUREG_ISP_ARM_CONFIGURATION);
	} else {
		/* 1. disable A5 */
		writel(0x00010000, PMUREG_ISP_ARM_OPTION);
		/* 2. A5 power off*/
		writel(0x0, PMUREG_ISP_ARM_CONFIGURATION);
		/* 3. Check A5 power off status register */
		timeout = 1000;
		while (__raw_readl(PMUREG_ISP_ARM_STATUS) & 0x1) {
			if (timeout == 0)
				printk(KERN_ERR "A5 power off failed\n");
			printk(KERN_INFO "Wait A5 power off\n");
			timeout--;
			udelay(1);
		}
	}
}

void fimc_is_hw_set_sensor_num(struct fimc_is_dev *dev)
{
	u32 cfg;
	writel(ISR_DONE, dev->regs + ISSR0);
	cfg = dev->sensor.id;
	writel(cfg, dev->regs + ISSR1);
	/* param 1 */
	writel(IHC_GET_SENSOR_NUMBER, dev->regs + ISSR2);
	/* param 2 */
	cfg = dev->sensor_num;
	writel(cfg, dev->regs + ISSR3);
}

void fimc_is_hw_set_load_setfile(struct fimc_is_dev *dev)
{
	u32 cfg;
	writel(ISR_DONE, dev->regs + ISSR0);
	cfg = dev->sensor.id;
	writel(cfg, dev->regs + ISSR1);
	/* param 1 */
	writel(IHC_LOAD_SET_FILE, dev->regs + ISSR2);
	/* param 2 */
	cfg = dev->sensor_num;
	writel(cfg, dev->regs + ISSR3);
}

int fimc_is_hw_get_sensor_num(struct fimc_is_dev *dev)
{
	u32 cfg = readl(dev->regs + ISSR11);
	if (dev->sensor_num == cfg)
		return 0;
	else
		return cfg;
}

int fimc_is_hw_set_param(struct fimc_is_dev *dev)
{
	fimc_is_hw_wait_intmsr0_intmsd0(dev);
	writel(HIC_SET_PARAMETER, dev->regs + ISSR0);
	writel(dev->sensor.id, dev->regs + ISSR1);

	writel(dev->scenario_id, dev->regs + ISSR2);

	writel(atomic_read(&dev->p_region_num), dev->regs + ISSR3);
	writel(dev->p_region_index1, dev->regs + ISSR4);
	writel(dev->p_region_index2, dev->regs + ISSR5);
	fimc_is_hw_set_intgr0_gd0(dev);
	return 0;
}

int fimc_is_hw_get_param(struct fimc_is_dev *dev, u16 offset)
{
	dev->i2h_cmd.num_valid_args = offset;
	switch (offset) {
	case 1:
		dev->i2h_cmd.arg[0] = readl(dev->regs + ISSR12);
		dev->i2h_cmd.arg[1] = 0;
		dev->i2h_cmd.arg[2] = 0;
		dev->i2h_cmd.arg[3] = 0;
		break;
	case 2:
		dev->i2h_cmd.arg[0] = readl(dev->regs + ISSR12);
		dev->i2h_cmd.arg[1] = readl(dev->regs + ISSR13);
		dev->i2h_cmd.arg[2] = 0;
		dev->i2h_cmd.arg[3] = 0;
		break;
	case 3:
		dev->i2h_cmd.arg[0] = readl(dev->regs + ISSR12);
		dev->i2h_cmd.arg[1] = readl(dev->regs + ISSR13);
		dev->i2h_cmd.arg[2] = readl(dev->regs + ISSR14);
		dev->i2h_cmd.arg[3] = 0;
		break;
	case 4:
		dev->i2h_cmd.arg[0] = readl(dev->regs + ISSR12);
		dev->i2h_cmd.arg[1] = readl(dev->regs + ISSR13);
		dev->i2h_cmd.arg[2] = readl(dev->regs + ISSR14);
		dev->i2h_cmd.arg[3] = readl(dev->regs + ISSR15);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void fimc_is_hw_set_stream(struct fimc_is_dev *dev, int on)
{
	if (on) {
		fimc_is_hw_wait_intmsr0_intmsd0(dev);
		writel(HIC_STREAM_ON, dev->regs + ISSR0);
		writel(dev->sensor.id, dev->regs + ISSR1);
		fimc_is_hw_set_intgr0_gd0(dev);
	} else {
		fimc_is_hw_wait_intmsr0_intmsd0(dev);
		writel(HIC_STREAM_OFF, dev->regs + ISSR0);
		writel(dev->sensor.id, dev->regs + ISSR1);
		fimc_is_hw_set_intgr0_gd0(dev);
	}
}

void fimc_is_hw_change_mode(struct fimc_is_dev *dev, int val)
{
	switch (val) {
	case IS_MODE_PREVIEW_STILL:
		dev->scenario_id = ISS_PREVIEW_STILL;
		fimc_is_hw_wait_intmsr0_intmsd0(dev);
		clear_bit(IS_ST_RUN, &dev->state);
		set_bit(IS_ST_CHANGE_MODE, &dev->state);
		writel(HIC_PREVIEW_STILL, dev->regs + ISSR0);
		writel(dev->sensor.id, dev->regs + ISSR1);
		fimc_is_hw_set_intgr0_gd0(dev);
		break;
	case IS_MODE_PREVIEW_VIDEO:
		dev->scenario_id = ISS_PREVIEW_VIDEO;
		fimc_is_hw_wait_intmsr0_intmsd0(dev);
		clear_bit(IS_ST_RUN, &dev->state);
		set_bit(IS_ST_CHANGE_MODE, &dev->state);
		writel(HIC_PREVIEW_VIDEO, dev->regs + ISSR0);
		writel(dev->sensor.id, dev->regs + ISSR1);
		fimc_is_hw_set_intgr0_gd0(dev);
		break;
	case IS_MODE_CAPTURE_STILL:
		dev->scenario_id = ISS_CAPTURE_STILL;
		fimc_is_hw_wait_intmsr0_intmsd0(dev);
		clear_bit(IS_ST_RUN, &dev->state);
		set_bit(IS_ST_CHANGE_MODE, &dev->state);
		writel(HIC_CAPTURE_STILL, dev->regs + ISSR0);
		writel(dev->sensor.id, dev->regs + ISSR1);
		fimc_is_hw_set_intgr0_gd0(dev);
		break;
	case IS_MODE_CAPTURE_VIDEO:
		dev->scenario_id = ISS_CAPTURE_VIDEO;
		fimc_is_hw_wait_intmsr0_intmsd0(dev);
		clear_bit(IS_ST_RUN, &dev->state);
		set_bit(IS_ST_CHANGE_MODE, &dev->state);
		writel(HIC_CAPTURE_VIDEO, dev->regs + ISSR0);
		writel(dev->sensor.id, dev->regs + ISSR1);
		fimc_is_hw_set_intgr0_gd0(dev);
		break;
	}
}
/*
 Group 3. Initial setting
*/
void fimc_is_hw_set_init(struct fimc_is_dev *dev)
{
	switch (dev->scenario_id) {
	case ISS_PREVIEW_STILL:
		IS_SET_PARAM_GLOBAL_SHOTMODE_CMD(dev, 1);
		IS_SET_PARAM_BIT(dev, PARAM_GLOBAL_SHOTMODE);
		IS_INC_PARAM_NUM(dev);
		IS_SENSOR_SET_FRAME_RATE(dev,
			init_val_sensor_preview.frame_rate.frame_rate);
		IS_SET_PARAM_BIT(dev, PARAM_SENSOR_FRAME_RATE);
		IS_INC_PARAM_NUM(dev);
		/* ISP */
		IS_ISP_SET_PARAM_CONTROL_CMD(dev,
			init_val_isp_preview.control.cmd);
		IS_ISP_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_isp_preview.control.bypass);
		IS_ISP_SET_PARAM_CONTROL_ERR(dev,
			init_val_isp_preview.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_isp_preview.otf_input.cmd);
		IS_ISP_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_isp_preview.otf_input.width);
		IS_ISP_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_isp_preview.otf_input.height);
		IS_ISP_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_isp_preview.otf_input.format);
		IS_ISP_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_isp_preview.otf_input.bitwidth);
		IS_ISP_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_isp_preview.otf_input.order);
		IS_ISP_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_isp_preview.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_INPUT1_CMD(dev,
			init_val_isp_preview.dma1_input.cmd);
		IS_ISP_SET_PARAM_DMA_INPUT1_WIDTH(dev,
			init_val_isp_preview.dma1_input.width);
		IS_ISP_SET_PARAM_DMA_INPUT1_HEIGHT(dev,
			init_val_isp_preview.dma1_input.height);
		IS_ISP_SET_PARAM_DMA_INPUT1_FORMAT(dev,
			init_val_isp_preview.dma1_input.format);
		IS_ISP_SET_PARAM_DMA_INPUT1_BITWIDTH(dev,
			init_val_isp_preview.dma1_input.bitwidth);
		IS_ISP_SET_PARAM_DMA_INPUT1_PLANE(dev,
			init_val_isp_preview.dma1_input.plane);
		IS_ISP_SET_PARAM_DMA_INPUT1_ORDER(dev,
			init_val_isp_preview.dma1_input.order);
		IS_ISP_SET_PARAM_DMA_INPUT1_BUFFERNUM(dev,
			init_val_isp_preview.dma1_input.buffer_number);
		IS_ISP_SET_PARAM_DMA_INPUT1_BUFFERADDR(dev,
			init_val_isp_preview.dma1_input.buffer_address);
		IS_ISP_SET_PARAM_DMA_INPUT1_ERR(dev,
			init_val_isp_preview.dma1_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA1_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_INPUT2_CMD(dev,
			init_val_isp_preview.dma2_input.cmd);
		IS_ISP_SET_PARAM_DMA_INPUT2_WIDTH(dev,
			init_val_isp_preview.dma2_input.width);
		IS_ISP_SET_PARAM_DMA_INPUT2_HEIGHT(dev,
			init_val_isp_preview.dma2_input.height);
		IS_ISP_SET_PARAM_DMA_INPUT2_FORMAT(dev,
			init_val_isp_preview.dma2_input.format);
		IS_ISP_SET_PARAM_DMA_INPUT2_BITWIDTH(dev,
			init_val_isp_preview.dma2_input.bitwidth);
		IS_ISP_SET_PARAM_DMA_INPUT2_PLANE(dev,
			init_val_isp_preview.dma2_input.plane);
		IS_ISP_SET_PARAM_DMA_INPUT2_ORDER(dev,
			init_val_isp_preview.dma2_input.order);
		IS_ISP_SET_PARAM_DMA_INPUT2_BUFFERNUM(dev,
			init_val_isp_preview.dma2_input.buffer_number);
		IS_ISP_SET_PARAM_DMA_INPUT2_BUFFERADDR(dev,
			init_val_isp_preview.dma2_input.buffer_address);
		IS_ISP_SET_PARAM_DMA_INPUT2_ERR(dev,
			init_val_isp_preview.dma2_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA2_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AF_CMD(dev, init_val_isp_preview.af.cmd);
		IS_ISP_SET_PARAM_AF_MODE(dev, init_val_isp_preview.af.mode);
		IS_ISP_SET_PARAM_AF_FACE(dev, init_val_isp_preview.af.face);
		IS_ISP_SET_PARAM_AF_CONTINUOUS(dev,
			init_val_isp_preview.af.continuous);
		IS_ISP_SET_PARAM_AF_WIN_POS_X(dev,
			init_val_isp_preview.af.win_pos_x);
		IS_ISP_SET_PARAM_AF_WIN_POS_Y(dev,
			init_val_isp_preview.af.win_pos_y);
		IS_ISP_SET_PARAM_AF_WIN_WIDTH(dev,
			init_val_isp_preview.af.win_width);
		IS_ISP_SET_PARAM_AF_WIN_HEIGHT(dev,
			init_val_isp_preview.af.win_height);
		IS_ISP_SET_PARAM_AF_ERR(dev, init_val_isp_preview.af.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AF);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_FLASH_CMD(dev,
			init_val_isp_preview.flash.cmd);
		IS_ISP_SET_PARAM_FLASH_REDEYE(dev,
			init_val_isp_preview.flash.redeye);
		IS_ISP_SET_PARAM_FLASH_ERR(dev,
			init_val_isp_preview.flash.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_FLASH);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AWB_CMD(dev, init_val_isp_preview.awb.cmd);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
			init_val_isp_preview.awb.illumination);
		IS_ISP_SET_PARAM_AWB_ERR(dev, init_val_isp_preview.awb.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AWB);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_EFFECT_CMD(dev,
			init_val_isp_preview.effect.cmd);
		IS_ISP_SET_PARAM_EFFECT_ERR(dev,
			init_val_isp_preview.effect.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_IMAGE_EFFECT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_ISO_CMD(dev,
			init_val_isp_preview.iso.cmd);
		IS_ISP_SET_PARAM_ISO_VALUE(dev,
			init_val_isp_preview.iso.value);
		IS_ISP_SET_PARAM_ISO_ERR(dev,
			init_val_isp_preview.iso.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ISO);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
			init_val_isp_preview.adjust.cmd);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev,
			init_val_isp_preview.adjust.contrast);
		IS_ISP_SET_PARAM_ADJUST_SATURATION(dev,
			init_val_isp_preview.adjust.saturation);
		IS_ISP_SET_PARAM_ADJUST_SHARPNESS(dev,
			init_val_isp_preview.adjust.sharpness);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev,
			init_val_isp_preview.adjust.exposure);
		IS_ISP_SET_PARAM_ADJUST_BRIGHTNESS(dev,
			init_val_isp_preview.adjust.brightness);
		IS_ISP_SET_PARAM_ADJUST_HUE(dev,
			init_val_isp_preview.adjust.hue);
		IS_ISP_SET_PARAM_ADJUST_ERR(dev,
			init_val_isp_preview.adjust.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_METERING_CMD(dev,
			init_val_isp_preview.metering.cmd);
		IS_ISP_SET_PARAM_METERING_WIN_POS_X(dev,
			init_val_isp_preview.metering.win_pos_x);
		IS_ISP_SET_PARAM_METERING_WIN_POS_Y(dev,
			init_val_isp_preview.metering.win_pos_y);
		IS_ISP_SET_PARAM_METERING_WIN_WIDTH(dev,
			init_val_isp_preview.metering.win_width);
		IS_ISP_SET_PARAM_METERING_WIN_HEIGHT(dev,
			init_val_isp_preview.metering.win_height);
		IS_ISP_SET_PARAM_METERING_ERR(dev,
			init_val_isp_preview.metering.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_METERING);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AFC_CMD(dev, init_val_isp_preview.afc.cmd);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev,
			init_val_isp_preview.afc.manual);
		IS_ISP_SET_PARAM_AFC_ERR(dev, init_val_isp_preview.afc.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AFC);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_OTF_OUTPUT_CMD(dev,
			init_val_isp_preview.otf_output.cmd);
		IS_ISP_SET_PARAM_OTF_OUTPUT_WIDTH(dev,
			init_val_isp_preview.otf_output.width);
		IS_ISP_SET_PARAM_OTF_OUTPUT_HEIGHT(dev,
			init_val_isp_preview.otf_output.height);
		IS_ISP_SET_PARAM_OTF_OUTPUT_FORMAT(dev,
			init_val_isp_preview.otf_output.format);
		IS_ISP_SET_PARAM_OTF_OUTPUT_BITWIDTH(dev,
			init_val_isp_preview.otf_output.bitwidth);
		IS_ISP_SET_PARAM_OTF_OUTPUT_ORDER(dev,
			init_val_isp_preview.otf_output.order);
		IS_ISP_SET_PARAM_OTF_OUTPUT_ERR(dev,
			init_val_isp_preview.otf_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_OTF_OUTPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_CMD(dev,
			init_val_isp_preview.dma1_output.cmd);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_WIDTH(dev,
			init_val_isp_preview.dma1_output.width);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_HEIGHT(dev,
			init_val_isp_preview.dma1_output.height);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_FORMAT(dev,
			init_val_isp_preview.dma1_output.format);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BITWIDTH(dev,
			init_val_isp_preview.dma1_output.bitwidth);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_PLANE(dev,
			init_val_isp_preview.dma1_output.plane);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_ORDER(dev,
			init_val_isp_preview.dma1_output.order);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BUFFER_NUMBER(dev,
			init_val_isp_preview.dma1_output.buffer_number);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BUFFER_ADDRESS(dev,
			init_val_isp_preview.dma1_output.buffer_address);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_ERR(dev,
			init_val_isp_preview.dma1_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA1_OUTPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_CMD(dev,
			init_val_isp_preview.dma2_output.cmd);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_WIDTH(dev,
			init_val_isp_preview.dma2_output.width);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_HEIGHT(dev,
			init_val_isp_preview.dma2_output.height);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_FORMAT(dev,
			init_val_isp_preview.dma2_output.format);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BITWIDTH(dev,
			init_val_isp_preview.dma2_output.bitwidth);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_PLANE(dev,
			init_val_isp_preview.dma2_output.plane);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_ORDER(dev,
			init_val_isp_preview.dma2_output.order);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BUFFER_NUMBER(dev,
			init_val_isp_preview.dma2_output.buffer_number);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BUFFER_ADDRESS(dev,
			init_val_isp_preview.dma2_output.buffer_address);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_ERR(dev,
			init_val_isp_preview.dma2_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA2_OUTPUT);
		IS_INC_PARAM_NUM(dev);

		/* DRC */
		IS_DRC_SET_PARAM_CONTROL_CMD(dev,
			init_val_drc_preview.control.cmd);
		IS_DRC_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_drc_preview.control.bypass);
		IS_DRC_SET_PARAM_CONTROL_ERR(dev,
			init_val_drc_preview.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_drc_preview.otf_input.cmd);
		IS_DRC_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_drc_preview.otf_input.width);
		IS_DRC_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_drc_preview.otf_input.height);
		IS_DRC_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_drc_preview.otf_input.format);
		IS_DRC_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_drc_preview.otf_input.bitwidth);
		IS_DRC_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_drc_preview.otf_input.order);
		IS_DRC_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_drc_preview.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_DMA_INPUT_CMD(dev,
			init_val_drc_preview.dma_input.cmd);
		IS_DRC_SET_PARAM_DMA_INPUT_WIDTH(dev,
			init_val_drc_preview.dma_input.width);
		IS_DRC_SET_PARAM_DMA_INPUT_HEIGHT(dev,
			init_val_drc_preview.dma_input.height);
		IS_DRC_SET_PARAM_DMA_INPUT_FORMAT(dev,
			init_val_drc_preview.dma_input.format);
		IS_DRC_SET_PARAM_DMA_INPUT_BITWIDTH(dev,
			init_val_drc_preview.dma_input.bitwidth);
		IS_DRC_SET_PARAM_DMA_INPUT_PLANE(dev,
			init_val_drc_preview.dma_input.plane);
		IS_DRC_SET_PARAM_DMA_INPUT_ORDER(dev,
			init_val_drc_preview.dma_input.order);
		IS_DRC_SET_PARAM_DMA_INPUT_BUFFERNUM(dev,
			init_val_drc_preview.dma_input.buffer_number);
		IS_DRC_SET_PARAM_DMA_INPUT_BUFFERADDR(dev,
			init_val_drc_preview.dma_input.buffer_address);
		IS_DRC_SET_PARAM_DMA_INPUT_ERR(dev,
			init_val_drc_preview.dma_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_DMA_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_OTF_OUTPUT_CMD(dev,
			init_val_drc_preview.otf_output.cmd);
		IS_DRC_SET_PARAM_OTF_OUTPUT_WIDTH(dev,
			init_val_drc_preview.otf_output.width);
		IS_DRC_SET_PARAM_OTF_OUTPUT_HEIGHT(dev,
			init_val_drc_preview.otf_output.height);
		IS_DRC_SET_PARAM_OTF_OUTPUT_FORMAT(dev,
			init_val_drc_preview.otf_output.format);
		IS_DRC_SET_PARAM_OTF_OUTPUT_BITWIDTH(dev,
			init_val_drc_preview.otf_output.bitwidth);
		IS_DRC_SET_PARAM_OTF_OUTPUT_ORDER(dev,
			init_val_drc_preview.otf_output.order);
		IS_DRC_SET_PARAM_OTF_OUTPUT_ERR(dev,
			init_val_drc_preview.otf_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_OTF_OUTPUT);
		IS_INC_PARAM_NUM(dev);

		/* FD */
		IS_FD_SET_PARAM_CONTROL_CMD(dev,
			init_val_fd_preview.control.cmd);
		IS_FD_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_fd_preview.control.bypass);
		IS_FD_SET_PARAM_CONTROL_ERR(dev,
			init_val_fd_preview.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_fd_preview.otf_input.cmd);
		IS_FD_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_fd_preview.otf_input.width);
		IS_FD_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_fd_preview.otf_input.height);
		IS_FD_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_fd_preview.otf_input.format);
		IS_FD_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_fd_preview.otf_input.bitwidth);
		IS_FD_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_fd_preview.otf_input.order);
		IS_FD_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_fd_preview.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_DMA_INPUT_CMD(dev,
			init_val_fd_preview.dma_input.cmd);
		IS_FD_SET_PARAM_DMA_INPUT_WIDTH(dev,
			init_val_fd_preview.dma_input.width);
		IS_FD_SET_PARAM_DMA_INPUT_HEIGHT(dev,
			init_val_fd_preview.dma_input.height);
		IS_FD_SET_PARAM_DMA_INPUT_FORMAT(dev,
			init_val_fd_preview.dma_input.format);
		IS_FD_SET_PARAM_DMA_INPUT_BITWIDTH(dev,
			init_val_fd_preview.dma_input.bitwidth);
		IS_FD_SET_PARAM_DMA_INPUT_PLANE(dev,
			init_val_fd_preview.dma_input.plane);
		IS_FD_SET_PARAM_DMA_INPUT_ORDER(dev,
			init_val_fd_preview.dma_input.order);
		IS_FD_SET_PARAM_DMA_INPUT_BUFFERNUM(dev,
			init_val_fd_preview.dma_input.buffer_number);
		IS_FD_SET_PARAM_DMA_INPUT_BUFFERADDR(dev,
			init_val_fd_preview.dma_input.buffer_address);
		IS_FD_SET_PARAM_DMA_INPUT_ERR(dev,
			init_val_fd_preview.dma_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_DMA_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_FD_RESULT_MAX_NUMBER(dev,
			init_val_fd_preview.result.max_number);
		IS_FD_SET_PARAM_FD_RESULT_ERR(dev,
			init_val_fd_preview.result.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_RESULT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_FD_MODE_SMILE(dev,
			init_val_fd_preview.mode.smile);
		IS_FD_SET_PARAM_FD_MODE_BLINK(dev,
			init_val_fd_preview.mode.blink);
		IS_FD_SET_PARAM_FD_MODE_ERR(dev,
			init_val_fd_preview.mode.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_MODE);
		IS_INC_PARAM_NUM(dev);

		dev->sensor.width_prev =
			init_val_isp_preview.otf_input.width;
		dev->sensor.height_prev =
			init_val_isp_preview.otf_input.height;
		break;
	case ISS_PREVIEW_VIDEO:
		IS_SET_PARAM_GLOBAL_SHOTMODE_CMD(dev, 1);
		IS_SET_PARAM_BIT(dev, PARAM_GLOBAL_SHOTMODE);
		IS_INC_PARAM_NUM(dev);
		IS_SENSOR_SET_FRAME_RATE(dev,
			init_val_sensor_preview.frame_rate.frame_rate);
		IS_SET_PARAM_BIT(dev, PARAM_SENSOR_FRAME_RATE);
		IS_INC_PARAM_NUM(dev);
		/* ISP */
		IS_ISP_SET_PARAM_CONTROL_CMD(dev,
			init_val_isp_preview.control.cmd);
		IS_ISP_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_isp_preview.control.bypass);
		IS_ISP_SET_PARAM_CONTROL_ERR(dev,
			init_val_isp_preview.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_isp_preview.otf_input.cmd);
		IS_ISP_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_isp_preview.otf_input.width);
		IS_ISP_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_isp_preview.otf_input.height);
		IS_ISP_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_isp_preview.otf_input.format);
		IS_ISP_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_isp_preview.otf_input.bitwidth);
		IS_ISP_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_isp_preview.otf_input.order);
		IS_ISP_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_isp_preview.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_INPUT1_CMD(dev,
			init_val_isp_preview.dma1_input.cmd);
		IS_ISP_SET_PARAM_DMA_INPUT1_WIDTH(dev,
			init_val_isp_preview.dma1_input.width);
		IS_ISP_SET_PARAM_DMA_INPUT1_HEIGHT(dev,
			init_val_isp_preview.dma1_input.height);
		IS_ISP_SET_PARAM_DMA_INPUT1_FORMAT(dev,
			init_val_isp_preview.dma1_input.format);
		IS_ISP_SET_PARAM_DMA_INPUT1_BITWIDTH(dev,
			init_val_isp_preview.dma1_input.bitwidth);
		IS_ISP_SET_PARAM_DMA_INPUT1_PLANE(dev,
			init_val_isp_preview.dma1_input.plane);
		IS_ISP_SET_PARAM_DMA_INPUT1_ORDER(dev,
			init_val_isp_preview.dma1_input.order);
		IS_ISP_SET_PARAM_DMA_INPUT1_BUFFERNUM(dev,
			init_val_isp_preview.dma1_input.buffer_number);
		IS_ISP_SET_PARAM_DMA_INPUT1_BUFFERADDR(dev,
			init_val_isp_preview.dma1_input.buffer_address);
		IS_ISP_SET_PARAM_DMA_INPUT1_ERR(dev,
			init_val_isp_preview.dma1_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA1_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_INPUT2_CMD(dev,
			init_val_isp_preview.dma2_input.cmd);
		IS_ISP_SET_PARAM_DMA_INPUT2_WIDTH(dev,
			init_val_isp_preview.dma2_input.width);
		IS_ISP_SET_PARAM_DMA_INPUT2_HEIGHT(dev,
			init_val_isp_preview.dma2_input.height);
		IS_ISP_SET_PARAM_DMA_INPUT2_FORMAT(dev,
			init_val_isp_preview.dma2_input.format);
		IS_ISP_SET_PARAM_DMA_INPUT2_BITWIDTH(dev,
			init_val_isp_preview.dma2_input.bitwidth);
		IS_ISP_SET_PARAM_DMA_INPUT2_PLANE(dev,
			init_val_isp_preview.dma2_input.plane);
		IS_ISP_SET_PARAM_DMA_INPUT2_ORDER(dev,
			init_val_isp_preview.dma2_input.order);
		IS_ISP_SET_PARAM_DMA_INPUT2_BUFFERNUM(dev,
			init_val_isp_preview.dma2_input.buffer_number);
		IS_ISP_SET_PARAM_DMA_INPUT2_BUFFERADDR(dev,
			init_val_isp_preview.dma2_input.buffer_address);
		IS_ISP_SET_PARAM_DMA_INPUT2_ERR(dev,
			init_val_isp_preview.dma2_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA2_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AF_CMD(dev, init_val_isp_preview.af.cmd);
		IS_ISP_SET_PARAM_AF_MODE(dev, init_val_isp_preview.af.mode);
		IS_ISP_SET_PARAM_AF_FACE(dev, init_val_isp_preview.af.face);
		IS_ISP_SET_PARAM_AF_CONTINUOUS(dev,
			init_val_isp_preview.af.continuous);
		IS_ISP_SET_PARAM_AF_WIN_POS_X(dev,
			init_val_isp_preview.af.win_pos_x);
		IS_ISP_SET_PARAM_AF_WIN_POS_Y(dev,
			init_val_isp_preview.af.win_pos_y);
		IS_ISP_SET_PARAM_AF_WIN_WIDTH(dev,
			init_val_isp_preview.af.win_width);
		IS_ISP_SET_PARAM_AF_WIN_HEIGHT(dev,
			init_val_isp_preview.af.win_height);
		IS_ISP_SET_PARAM_AF_ERR(dev, init_val_isp_preview.af.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AF);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_FLASH_CMD(dev,
			init_val_isp_preview.flash.cmd);
		IS_ISP_SET_PARAM_FLASH_REDEYE(dev,
			init_val_isp_preview.flash.redeye);
		IS_ISP_SET_PARAM_FLASH_ERR(dev,
			init_val_isp_preview.flash.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_FLASH);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AWB_CMD(dev, init_val_isp_preview.awb.cmd);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
			init_val_isp_preview.awb.illumination);
		IS_ISP_SET_PARAM_AWB_ERR(dev, init_val_isp_preview.awb.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AWB);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_EFFECT_CMD(dev,
			init_val_isp_preview.effect.cmd);
		IS_ISP_SET_PARAM_EFFECT_ERR(dev,
			init_val_isp_preview.effect.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_IMAGE_EFFECT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_ISO_CMD(dev,
			init_val_isp_preview.iso.cmd);
		IS_ISP_SET_PARAM_ISO_VALUE(dev,
			init_val_isp_preview.iso.value);
		IS_ISP_SET_PARAM_ISO_ERR(dev,
			init_val_isp_preview.iso.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ISO);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
			init_val_isp_preview.adjust.cmd);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev,
			init_val_isp_preview.adjust.contrast);
		IS_ISP_SET_PARAM_ADJUST_SATURATION(dev,
			init_val_isp_preview.adjust.saturation);
		IS_ISP_SET_PARAM_ADJUST_SHARPNESS(dev,
			init_val_isp_preview.adjust.sharpness);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev,
			init_val_isp_preview.adjust.exposure);
		IS_ISP_SET_PARAM_ADJUST_BRIGHTNESS(dev,
			init_val_isp_preview.adjust.brightness);
		IS_ISP_SET_PARAM_ADJUST_HUE(dev,
			init_val_isp_preview.adjust.hue);
		IS_ISP_SET_PARAM_ADJUST_ERR(dev,
			init_val_isp_preview.adjust.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_METERING_CMD(dev,
			init_val_isp_preview.metering.cmd);
		IS_ISP_SET_PARAM_METERING_WIN_POS_X(dev,
			init_val_isp_preview.metering.win_pos_x);
		IS_ISP_SET_PARAM_METERING_WIN_POS_Y(dev,
			init_val_isp_preview.metering.win_pos_y);
		IS_ISP_SET_PARAM_METERING_WIN_WIDTH(dev,
			init_val_isp_preview.metering.win_width);
		IS_ISP_SET_PARAM_METERING_WIN_HEIGHT(dev,
			init_val_isp_preview.metering.win_height);
		IS_ISP_SET_PARAM_METERING_ERR(dev,
			init_val_isp_preview.metering.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_METERING);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AFC_CMD(dev, init_val_isp_preview.afc.cmd);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev,
			init_val_isp_preview.afc.manual);
		IS_ISP_SET_PARAM_AFC_ERR(dev, init_val_isp_preview.afc.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AFC);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_OTF_OUTPUT_CMD(dev,
			init_val_isp_preview.otf_output.cmd);
		IS_ISP_SET_PARAM_OTF_OUTPUT_WIDTH(dev,
			init_val_isp_preview.otf_output.width);
		IS_ISP_SET_PARAM_OTF_OUTPUT_HEIGHT(dev,
			init_val_isp_preview.otf_output.height);
		IS_ISP_SET_PARAM_OTF_OUTPUT_FORMAT(dev,
			init_val_isp_preview.otf_output.format);
		IS_ISP_SET_PARAM_OTF_OUTPUT_BITWIDTH(dev,
			init_val_isp_preview.otf_output.bitwidth);
		IS_ISP_SET_PARAM_OTF_OUTPUT_ORDER(dev,
			init_val_isp_preview.otf_output.order);
		IS_ISP_SET_PARAM_OTF_OUTPUT_ERR(dev,
			init_val_isp_preview.otf_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_OTF_OUTPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_CMD(dev,
			init_val_isp_preview.dma1_output.cmd);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_WIDTH(dev,
			init_val_isp_preview.dma1_output.width);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_HEIGHT(dev,
			init_val_isp_preview.dma1_output.height);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_FORMAT(dev,
			init_val_isp_preview.dma1_output.format);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BITWIDTH(dev,
			init_val_isp_preview.dma1_output.bitwidth);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_PLANE(dev,
			init_val_isp_preview.dma1_output.plane);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_ORDER(dev,
			init_val_isp_preview.dma1_output.order);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BUFFER_NUMBER(dev,
			init_val_isp_preview.dma1_output.buffer_number);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BUFFER_ADDRESS(dev,
			init_val_isp_preview.dma1_output.buffer_address);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_ERR(dev,
			init_val_isp_preview.dma1_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA1_OUTPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_CMD(dev,
			init_val_isp_preview.dma2_output.cmd);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_WIDTH(dev,
			init_val_isp_preview.dma2_output.width);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_HEIGHT(dev,
			init_val_isp_preview.dma2_output.height);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_FORMAT(dev,
			init_val_isp_preview.dma2_output.format);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BITWIDTH(dev,
			init_val_isp_preview.dma2_output.bitwidth);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_PLANE(dev,
			init_val_isp_preview.dma2_output.plane);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_ORDER(dev,
			init_val_isp_preview.dma2_output.order);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BUFFER_NUMBER(dev,
			init_val_isp_preview.dma2_output.buffer_number);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BUFFER_ADDRESS(dev,
			init_val_isp_preview.dma2_output.buffer_address);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_ERR(dev,
			init_val_isp_preview.dma2_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA2_OUTPUT);
		IS_INC_PARAM_NUM(dev);

		/* DRC */
		IS_DRC_SET_PARAM_CONTROL_CMD(dev,
			init_val_drc_preview.control.cmd);
		IS_DRC_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_drc_preview.control.bypass);
		IS_DRC_SET_PARAM_CONTROL_ERR(dev,
			init_val_drc_preview.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_drc_preview.otf_input.cmd);
		IS_DRC_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_drc_preview.otf_input.width);
		IS_DRC_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_drc_preview.otf_input.height);
		IS_DRC_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_drc_preview.otf_input.format);
		IS_DRC_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_drc_preview.otf_input.bitwidth);
		IS_DRC_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_drc_preview.otf_input.order);
		IS_DRC_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_drc_preview.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_DMA_INPUT_CMD(dev,
			init_val_drc_preview.dma_input.cmd);
		IS_DRC_SET_PARAM_DMA_INPUT_WIDTH(dev,
			init_val_drc_preview.dma_input.width);
		IS_DRC_SET_PARAM_DMA_INPUT_HEIGHT(dev,
			init_val_drc_preview.dma_input.height);
		IS_DRC_SET_PARAM_DMA_INPUT_FORMAT(dev,
			init_val_drc_preview.dma_input.format);
		IS_DRC_SET_PARAM_DMA_INPUT_BITWIDTH(dev,
			init_val_drc_preview.dma_input.bitwidth);
		IS_DRC_SET_PARAM_DMA_INPUT_PLANE(dev,
			init_val_drc_preview.dma_input.plane);
		IS_DRC_SET_PARAM_DMA_INPUT_ORDER(dev,
			init_val_drc_preview.dma_input.order);
		IS_DRC_SET_PARAM_DMA_INPUT_BUFFERNUM(dev,
			init_val_drc_preview.dma_input.buffer_number);
		IS_DRC_SET_PARAM_DMA_INPUT_BUFFERADDR(dev,
			init_val_drc_preview.dma_input.buffer_address);
		IS_DRC_SET_PARAM_DMA_INPUT_ERR(dev,
			init_val_drc_preview.dma_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_DMA_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_OTF_OUTPUT_CMD(dev,
			init_val_drc_preview.otf_output.cmd);
		IS_DRC_SET_PARAM_OTF_OUTPUT_WIDTH(dev,
			init_val_drc_preview.otf_output.width);
		IS_DRC_SET_PARAM_OTF_OUTPUT_HEIGHT(dev,
			init_val_drc_preview.otf_output.height);
		IS_DRC_SET_PARAM_OTF_OUTPUT_FORMAT(dev,
			init_val_drc_preview.otf_output.format);
		IS_DRC_SET_PARAM_OTF_OUTPUT_BITWIDTH(dev,
			init_val_drc_preview.otf_output.bitwidth);
		IS_DRC_SET_PARAM_OTF_OUTPUT_ORDER(dev,
			init_val_drc_preview.otf_output.order);
		IS_DRC_SET_PARAM_OTF_OUTPUT_ERR(dev,
			init_val_drc_preview.otf_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_OTF_OUTPUT);
		IS_INC_PARAM_NUM(dev);

		/* FD */
		IS_FD_SET_PARAM_CONTROL_CMD(dev,
			init_val_fd_preview.control.cmd);
		IS_FD_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_fd_preview.control.bypass);
		IS_FD_SET_PARAM_CONTROL_ERR(dev,
			init_val_fd_preview.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_fd_preview.otf_input.cmd);
		IS_FD_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_fd_preview.otf_input.width);
		IS_FD_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_fd_preview.otf_input.height);
		IS_FD_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_fd_preview.otf_input.format);
		IS_FD_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_fd_preview.otf_input.bitwidth);
		IS_FD_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_fd_preview.otf_input.order);
		IS_FD_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_fd_preview.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_DMA_INPUT_CMD(dev,
			init_val_fd_preview.dma_input.cmd);
		IS_FD_SET_PARAM_DMA_INPUT_WIDTH(dev,
			init_val_fd_preview.dma_input.width);
		IS_FD_SET_PARAM_DMA_INPUT_HEIGHT(dev,
			init_val_fd_preview.dma_input.height);
		IS_FD_SET_PARAM_DMA_INPUT_FORMAT(dev,
			init_val_fd_preview.dma_input.format);
		IS_FD_SET_PARAM_DMA_INPUT_BITWIDTH(dev,
			init_val_fd_preview.dma_input.bitwidth);
		IS_FD_SET_PARAM_DMA_INPUT_PLANE(dev,
			init_val_fd_preview.dma_input.plane);
		IS_FD_SET_PARAM_DMA_INPUT_ORDER(dev,
			init_val_fd_preview.dma_input.order);
		IS_FD_SET_PARAM_DMA_INPUT_BUFFERNUM(dev,
			init_val_fd_preview.dma_input.buffer_number);
		IS_FD_SET_PARAM_DMA_INPUT_BUFFERADDR(dev,
			init_val_fd_preview.dma_input.buffer_address);
		IS_FD_SET_PARAM_DMA_INPUT_ERR(dev,
			init_val_fd_preview.dma_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_DMA_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_FD_RESULT_MAX_NUMBER(dev,
			init_val_fd_preview.result.max_number);
		IS_FD_SET_PARAM_FD_RESULT_ERR(dev,
			init_val_fd_preview.result.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_RESULT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_FD_MODE_SMILE(dev,
			init_val_fd_preview.mode.smile);
		IS_FD_SET_PARAM_FD_MODE_BLINK(dev,
			init_val_fd_preview.mode.blink);
		IS_FD_SET_PARAM_FD_MODE_ERR(dev,
			init_val_fd_preview.mode.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_MODE);
		IS_INC_PARAM_NUM(dev);

		dev->sensor.width_prev_cam =
			init_val_isp_preview.otf_input.width;
		dev->sensor.height_prev_cam =
			init_val_isp_preview.otf_input.height;
		break;

	case ISS_CAPTURE_STILL:
		IS_SET_PARAM_GLOBAL_SHOTMODE_CMD(dev, 1);
		IS_SET_PARAM_BIT(dev, PARAM_GLOBAL_SHOTMODE);
		IS_INC_PARAM_NUM(dev);
		IS_SENSOR_SET_FRAME_RATE(dev,
			init_val_sensor_capture.frame_rate.frame_rate);
		IS_SET_PARAM_BIT(dev, PARAM_SENSOR_FRAME_RATE);
		IS_INC_PARAM_NUM(dev);
		/* ISP */
		IS_ISP_SET_PARAM_CONTROL_CMD(dev,
			init_val_isp_capture.control.cmd);
		IS_ISP_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_isp_capture.control.bypass);
		IS_ISP_SET_PARAM_CONTROL_ERR(dev,
			init_val_isp_capture.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_isp_capture.otf_input.cmd);
		IS_ISP_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_isp_capture.otf_input.width);
		IS_ISP_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_isp_capture.otf_input.height);
		IS_ISP_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_isp_capture.otf_input.format);
		IS_ISP_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_isp_capture.otf_input.bitwidth);
		IS_ISP_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_isp_capture.otf_input.order);
		IS_ISP_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_isp_capture.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_INPUT1_CMD(dev,
			init_val_isp_capture.dma1_input.cmd);
		IS_ISP_SET_PARAM_DMA_INPUT1_WIDTH(dev,
			init_val_isp_capture.dma1_input.width);
		IS_ISP_SET_PARAM_DMA_INPUT1_HEIGHT(dev,
			init_val_isp_capture.dma1_input.height);
		IS_ISP_SET_PARAM_DMA_INPUT1_FORMAT(dev,
			init_val_isp_capture.dma1_input.format);
		IS_ISP_SET_PARAM_DMA_INPUT1_BITWIDTH(dev,
			init_val_isp_capture.dma1_input.bitwidth);
		IS_ISP_SET_PARAM_DMA_INPUT1_PLANE(dev,
			init_val_isp_capture.dma1_input.plane);
		IS_ISP_SET_PARAM_DMA_INPUT1_ORDER(dev,
			init_val_isp_capture.dma1_input.order);
		IS_ISP_SET_PARAM_DMA_INPUT1_BUFFERNUM(dev,
			init_val_isp_capture.dma1_input.buffer_number);
		IS_ISP_SET_PARAM_DMA_INPUT1_BUFFERADDR(dev,
			init_val_isp_capture.dma1_input.buffer_address);
		IS_ISP_SET_PARAM_DMA_INPUT1_ERR(dev,
			init_val_isp_capture.dma1_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA1_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_INPUT2_CMD(dev,
			init_val_isp_capture.dma2_input.cmd);
		IS_ISP_SET_PARAM_DMA_INPUT2_WIDTH(dev,
			init_val_isp_capture.dma2_input.width);
		IS_ISP_SET_PARAM_DMA_INPUT2_HEIGHT(dev,
			init_val_isp_capture.dma2_input.height);
		IS_ISP_SET_PARAM_DMA_INPUT2_FORMAT(dev,
			init_val_isp_capture.dma2_input.format);
		IS_ISP_SET_PARAM_DMA_INPUT2_BITWIDTH(dev,
			init_val_isp_capture.dma2_input.bitwidth);
		IS_ISP_SET_PARAM_DMA_INPUT2_PLANE(dev,
			init_val_isp_capture.dma2_input.plane);
		IS_ISP_SET_PARAM_DMA_INPUT2_ORDER(dev,
			init_val_isp_capture.dma2_input.order);
		IS_ISP_SET_PARAM_DMA_INPUT2_BUFFERNUM(dev,
			init_val_isp_capture.dma2_input.buffer_number);
		IS_ISP_SET_PARAM_DMA_INPUT2_BUFFERADDR(dev,
			init_val_isp_capture.dma2_input.buffer_address);
		IS_ISP_SET_PARAM_DMA_INPUT2_ERR(dev,
			init_val_isp_capture.dma2_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA2_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AF_CMD(dev, init_val_isp_capture.af.cmd);
		IS_ISP_SET_PARAM_AF_MODE(dev, init_val_isp_capture.af.mode);
		IS_ISP_SET_PARAM_AF_FACE(dev, init_val_isp_capture.af.face);
		IS_ISP_SET_PARAM_AF_CONTINUOUS(dev,
			init_val_isp_capture.af.continuous);
		IS_ISP_SET_PARAM_AF_WIN_POS_X(dev,
			init_val_isp_capture.af.win_pos_x);
		IS_ISP_SET_PARAM_AF_WIN_POS_Y(dev,
			init_val_isp_capture.af.win_pos_y);
		IS_ISP_SET_PARAM_AF_WIN_WIDTH(dev,
			init_val_isp_capture.af.win_width);
		IS_ISP_SET_PARAM_AF_WIN_HEIGHT(dev,
			init_val_isp_capture.af.win_height);
		IS_ISP_SET_PARAM_AF_ERR(dev, init_val_isp_capture.af.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AF);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_FLASH_CMD(dev,
			init_val_isp_capture.flash.cmd);
		IS_ISP_SET_PARAM_FLASH_REDEYE(dev,
			init_val_isp_capture.flash.redeye);
		IS_ISP_SET_PARAM_FLASH_ERR(dev,
			init_val_isp_capture.flash.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_FLASH);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AWB_CMD(dev, init_val_isp_capture.awb.cmd);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
			init_val_isp_capture.awb.illumination);
		IS_ISP_SET_PARAM_AWB_ERR(dev, init_val_isp_capture.awb.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AWB);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_EFFECT_CMD(dev,
			init_val_isp_capture.effect.cmd);
		IS_ISP_SET_PARAM_EFFECT_ERR(dev,
			init_val_isp_capture.effect.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_IMAGE_EFFECT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_ISO_CMD(dev,
			init_val_isp_capture.iso.cmd);
		IS_ISP_SET_PARAM_ISO_VALUE(dev,
			init_val_isp_capture.iso.value);
		IS_ISP_SET_PARAM_ISO_ERR(dev,
			init_val_isp_capture.iso.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ISO);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
			init_val_isp_capture.adjust.cmd);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev,
			init_val_isp_capture.adjust.contrast);
		IS_ISP_SET_PARAM_ADJUST_SATURATION(dev,
			init_val_isp_capture.adjust.saturation);
		IS_ISP_SET_PARAM_ADJUST_SHARPNESS(dev,
			init_val_isp_capture.adjust.sharpness);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev,
			init_val_isp_capture.adjust.exposure);
		IS_ISP_SET_PARAM_ADJUST_BRIGHTNESS(dev,
			init_val_isp_capture.adjust.brightness);
		IS_ISP_SET_PARAM_ADJUST_HUE(dev,
			init_val_isp_capture.adjust.hue);
		IS_ISP_SET_PARAM_ADJUST_ERR(dev,
			init_val_isp_capture.adjust.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_METERING_CMD(dev,
			init_val_isp_capture.metering.cmd);
		IS_ISP_SET_PARAM_METERING_WIN_POS_X(dev,
			init_val_isp_capture.metering.win_pos_x);
		IS_ISP_SET_PARAM_METERING_WIN_POS_Y(dev,
			init_val_isp_capture.metering.win_pos_y);
		IS_ISP_SET_PARAM_METERING_WIN_WIDTH(dev,
			init_val_isp_capture.metering.win_width);
		IS_ISP_SET_PARAM_METERING_WIN_HEIGHT(dev,
			init_val_isp_capture.metering.win_height);
		IS_ISP_SET_PARAM_METERING_ERR(dev,
			init_val_isp_capture.metering.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_METERING);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AFC_CMD(dev, init_val_isp_capture.afc.cmd);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev,
			init_val_isp_capture.afc.manual);
		IS_ISP_SET_PARAM_AFC_ERR(dev, init_val_isp_capture.afc.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AFC);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_OTF_OUTPUT_CMD(dev,
			init_val_isp_capture.otf_output.cmd);
		IS_ISP_SET_PARAM_OTF_OUTPUT_WIDTH(dev,
			init_val_isp_capture.otf_output.width);
		IS_ISP_SET_PARAM_OTF_OUTPUT_HEIGHT(dev,
			init_val_isp_capture.otf_output.height);
		IS_ISP_SET_PARAM_OTF_OUTPUT_FORMAT(dev,
			init_val_isp_capture.otf_output.format);
		IS_ISP_SET_PARAM_OTF_OUTPUT_BITWIDTH(dev,
			init_val_isp_capture.otf_output.bitwidth);
		IS_ISP_SET_PARAM_OTF_OUTPUT_ORDER(dev,
			init_val_isp_capture.otf_output.order);
		IS_ISP_SET_PARAM_OTF_OUTPUT_ERR(dev,
			init_val_isp_capture.otf_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_OTF_OUTPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_CMD(dev,
			init_val_isp_capture.dma1_output.cmd);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_WIDTH(dev,
			init_val_isp_capture.dma1_output.width);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_HEIGHT(dev,
			init_val_isp_capture.dma1_output.height);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_FORMAT(dev,
			init_val_isp_capture.dma1_output.format);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BITWIDTH(dev,
			init_val_isp_capture.dma1_output.bitwidth);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_PLANE(dev,
			init_val_isp_capture.dma1_output.plane);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_ORDER(dev,
			init_val_isp_capture.dma1_output.order);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BUFFER_NUMBER(dev,
			init_val_isp_capture.dma1_output.buffer_number);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BUFFER_ADDRESS(dev,
			init_val_isp_capture.dma1_output.buffer_address);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_ERR(dev,
			init_val_isp_capture.dma1_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA1_OUTPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_CMD(dev,
			init_val_isp_capture.dma2_output.cmd);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_WIDTH(dev,
			init_val_isp_capture.dma2_output.width);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_HEIGHT(dev,
			init_val_isp_capture.dma2_output.height);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_FORMAT(dev,
			init_val_isp_capture.dma2_output.format);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BITWIDTH(dev,
			init_val_isp_capture.dma2_output.bitwidth);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_PLANE(dev,
			init_val_isp_capture.dma2_output.plane);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_ORDER(dev,
			init_val_isp_capture.dma2_output.order);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BUFFER_NUMBER(dev,
			init_val_isp_capture.dma2_output.buffer_number);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BUFFER_ADDRESS(dev,
			init_val_isp_capture.dma2_output.buffer_address);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_ERR(dev,
			init_val_isp_capture.dma2_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA2_OUTPUT);
		IS_INC_PARAM_NUM(dev);

		/* DRC */
		IS_DRC_SET_PARAM_CONTROL_CMD(dev,
			init_val_drc_capture.control.cmd);
		IS_DRC_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_drc_capture.control.bypass);
		IS_DRC_SET_PARAM_CONTROL_ERR(dev,
			init_val_drc_capture.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_drc_capture.otf_input.cmd);
		IS_DRC_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_drc_capture.otf_input.width);
		IS_DRC_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_drc_capture.otf_input.height);
		IS_DRC_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_drc_capture.otf_input.format);
		IS_DRC_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_drc_capture.otf_input.bitwidth);
		IS_DRC_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_drc_capture.otf_input.order);
		IS_DRC_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_drc_capture.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_DMA_INPUT_CMD(dev,
			init_val_drc_capture.dma_input.cmd);
		IS_DRC_SET_PARAM_DMA_INPUT_WIDTH(dev,
			init_val_drc_capture.dma_input.width);
		IS_DRC_SET_PARAM_DMA_INPUT_HEIGHT(dev,
			init_val_drc_capture.dma_input.height);
		IS_DRC_SET_PARAM_DMA_INPUT_FORMAT(dev,
			init_val_drc_capture.dma_input.format);
		IS_DRC_SET_PARAM_DMA_INPUT_BITWIDTH(dev,
			init_val_drc_capture.dma_input.bitwidth);
		IS_DRC_SET_PARAM_DMA_INPUT_PLANE(dev,
			init_val_drc_capture.dma_input.plane);
		IS_DRC_SET_PARAM_DMA_INPUT_ORDER(dev,
			init_val_drc_capture.dma_input.order);
		IS_DRC_SET_PARAM_DMA_INPUT_BUFFERNUM(dev,
			init_val_drc_capture.dma_input.buffer_number);
		IS_DRC_SET_PARAM_DMA_INPUT_BUFFERADDR(dev,
			init_val_drc_capture.dma_input.buffer_address);
		IS_DRC_SET_PARAM_DMA_INPUT_ERR(dev,
			init_val_drc_capture.dma_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_DMA_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_OTF_OUTPUT_CMD(dev,
			init_val_drc_capture.otf_output.cmd);
		IS_DRC_SET_PARAM_OTF_OUTPUT_WIDTH(dev,
			init_val_drc_capture.otf_output.width);
		IS_DRC_SET_PARAM_OTF_OUTPUT_HEIGHT(dev,
			init_val_drc_capture.otf_output.height);
		IS_DRC_SET_PARAM_OTF_OUTPUT_FORMAT(dev,
			init_val_drc_capture.otf_output.format);
		IS_DRC_SET_PARAM_OTF_OUTPUT_BITWIDTH(dev,
			init_val_drc_capture.otf_output.bitwidth);
		IS_DRC_SET_PARAM_OTF_OUTPUT_ORDER(dev,
			init_val_drc_capture.otf_output.order);
		IS_DRC_SET_PARAM_OTF_OUTPUT_ERR(dev,
			init_val_drc_capture.otf_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_OTF_OUTPUT);
		IS_INC_PARAM_NUM(dev);

		/* FD */
		IS_FD_SET_PARAM_CONTROL_CMD(dev,
			init_val_fd_capture.control.cmd);
		IS_FD_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_fd_capture.control.bypass);
		IS_FD_SET_PARAM_CONTROL_ERR(dev,
			init_val_fd_capture.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_fd_capture.otf_input.cmd);
		IS_FD_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_fd_capture.otf_input.width);
		IS_FD_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_fd_capture.otf_input.height);
		IS_FD_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_fd_capture.otf_input.format);
		IS_FD_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_fd_capture.otf_input.bitwidth);
		IS_FD_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_fd_capture.otf_input.order);
		IS_FD_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_fd_capture.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_DMA_INPUT_CMD(dev,
			init_val_fd_capture.dma_input.cmd);
		IS_FD_SET_PARAM_DMA_INPUT_WIDTH(dev,
			init_val_fd_capture.dma_input.width);
		IS_FD_SET_PARAM_DMA_INPUT_HEIGHT(dev,
			init_val_fd_capture.dma_input.height);
		IS_FD_SET_PARAM_DMA_INPUT_FORMAT(dev,
			init_val_fd_capture.dma_input.format);
		IS_FD_SET_PARAM_DMA_INPUT_BITWIDTH(dev,
			init_val_fd_capture.dma_input.bitwidth);
		IS_FD_SET_PARAM_DMA_INPUT_PLANE(dev,
			init_val_fd_capture.dma_input.plane);
		IS_FD_SET_PARAM_DMA_INPUT_ORDER(dev,
			init_val_fd_capture.dma_input.order);
		IS_FD_SET_PARAM_DMA_INPUT_BUFFERNUM(dev,
			init_val_fd_capture.dma_input.buffer_number);
		IS_FD_SET_PARAM_DMA_INPUT_BUFFERADDR(dev,
			init_val_fd_capture.dma_input.buffer_address);
		IS_FD_SET_PARAM_DMA_INPUT_ERR(dev,
			init_val_fd_capture.dma_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_DMA_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_FD_RESULT_MAX_NUMBER(dev,
			init_val_fd_preview.result.max_number);
		IS_FD_SET_PARAM_FD_RESULT_ERR(dev,
			init_val_fd_preview.result.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_RESULT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_FD_MODE_SMILE(dev,
			init_val_fd_preview.mode.smile);
		IS_FD_SET_PARAM_FD_MODE_BLINK(dev,
			init_val_fd_preview.mode.blink);
		IS_FD_SET_PARAM_FD_MODE_ERR(dev,
			init_val_fd_preview.mode.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_MODE);
		IS_INC_PARAM_NUM(dev);
		break;

	case ISS_CAPTURE_VIDEO:
		IS_SET_PARAM_GLOBAL_SHOTMODE_CMD(dev, 1);
		IS_SET_PARAM_BIT(dev, PARAM_SENSOR_FRAME_RATE);
		IS_INC_PARAM_NUM(dev);
		IS_SENSOR_SET_FRAME_RATE(dev,
			init_val_sensor_camcording.frame_rate.frame_rate);
		IS_SET_PARAM_BIT(dev, PARAM_SENSOR_CONTROL);
		IS_INC_PARAM_NUM(dev);
		/* ISP */
		IS_ISP_SET_PARAM_CONTROL_CMD(dev,
			init_val_isp_camcording.control.cmd);
		IS_ISP_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_isp_camcording.control.bypass);
		IS_ISP_SET_PARAM_CONTROL_ERR(dev,
			init_val_isp_camcording.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_isp_camcording.otf_input.cmd);
		IS_ISP_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_isp_camcording.otf_input.width);
		IS_ISP_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_isp_camcording.otf_input.height);
		IS_ISP_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_isp_camcording.otf_input.format);
		IS_ISP_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_isp_camcording.otf_input.bitwidth);
		IS_ISP_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_isp_camcording.otf_input.order);
		IS_ISP_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_isp_camcording.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_INPUT1_CMD(dev,
			init_val_isp_camcording.dma1_input.cmd);
		IS_ISP_SET_PARAM_DMA_INPUT1_WIDTH(dev,
			init_val_isp_camcording.dma1_input.width);
		IS_ISP_SET_PARAM_DMA_INPUT1_HEIGHT(dev,
			init_val_isp_camcording.dma1_input.height);
		IS_ISP_SET_PARAM_DMA_INPUT1_FORMAT(dev,
			init_val_isp_camcording.dma1_input.format);
		IS_ISP_SET_PARAM_DMA_INPUT1_BITWIDTH(dev,
			init_val_isp_camcording.dma1_input.bitwidth);
		IS_ISP_SET_PARAM_DMA_INPUT1_PLANE(dev,
			init_val_isp_camcording.dma1_input.plane);
		IS_ISP_SET_PARAM_DMA_INPUT1_ORDER(dev,
			init_val_isp_camcording.dma1_input.order);
		IS_ISP_SET_PARAM_DMA_INPUT1_BUFFERNUM(dev,
			init_val_isp_camcording.dma1_input.buffer_number);
		IS_ISP_SET_PARAM_DMA_INPUT1_BUFFERADDR(dev,
			init_val_isp_camcording.dma1_input.buffer_address);
		IS_ISP_SET_PARAM_DMA_INPUT1_ERR(dev,
			init_val_isp_camcording.dma1_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA1_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_INPUT2_CMD(dev,
			init_val_isp_camcording.dma2_input.cmd);
		IS_ISP_SET_PARAM_DMA_INPUT2_WIDTH(dev,
			init_val_isp_camcording.dma2_input.width);
		IS_ISP_SET_PARAM_DMA_INPUT2_HEIGHT(dev,
			init_val_isp_camcording.dma2_input.height);
		IS_ISP_SET_PARAM_DMA_INPUT2_FORMAT(dev,
			init_val_isp_camcording.dma2_input.format);
		IS_ISP_SET_PARAM_DMA_INPUT2_BITWIDTH(dev,
			init_val_isp_camcording.dma2_input.bitwidth);
		IS_ISP_SET_PARAM_DMA_INPUT2_PLANE(dev,
			init_val_isp_camcording.dma2_input.plane);
		IS_ISP_SET_PARAM_DMA_INPUT2_ORDER(dev,
			init_val_isp_camcording.dma2_input.order);
		IS_ISP_SET_PARAM_DMA_INPUT2_BUFFERNUM(dev,
			init_val_isp_camcording.dma2_input.buffer_number);
		IS_ISP_SET_PARAM_DMA_INPUT2_BUFFERADDR(dev,
			init_val_isp_camcording.dma2_input.buffer_address);
		IS_ISP_SET_PARAM_DMA_INPUT2_ERR(dev,
			init_val_isp_camcording.dma2_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA2_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AF_CMD(dev, init_val_isp_camcording.af.cmd);
		IS_ISP_SET_PARAM_AF_MODE(dev, init_val_isp_camcording.af.mode);
		IS_ISP_SET_PARAM_AF_FACE(dev, init_val_isp_camcording.af.face);
		IS_ISP_SET_PARAM_AF_CONTINUOUS(dev,
			init_val_isp_camcording.af.continuous);
		IS_ISP_SET_PARAM_AF_WIN_POS_X(dev,
			init_val_isp_camcording.af.win_pos_x);
		IS_ISP_SET_PARAM_AF_WIN_POS_Y(dev,
			init_val_isp_camcording.af.win_pos_y);
		IS_ISP_SET_PARAM_AF_WIN_WIDTH(dev,
			init_val_isp_camcording.af.win_width);
		IS_ISP_SET_PARAM_AF_WIN_HEIGHT(dev,
			init_val_isp_camcording.af.win_height);
		IS_ISP_SET_PARAM_AF_ERR(dev, init_val_isp_camcording.af.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AF);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_FLASH_CMD(dev,
			init_val_isp_camcording.flash.cmd);
		IS_ISP_SET_PARAM_FLASH_REDEYE(dev,
			init_val_isp_camcording.flash.redeye);
		IS_ISP_SET_PARAM_FLASH_ERR(dev,
			init_val_isp_camcording.flash.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_FLASH);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AWB_CMD(dev, init_val_isp_camcording.awb.cmd);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
			init_val_isp_camcording.awb.illumination);
		IS_ISP_SET_PARAM_AWB_ERR(dev, init_val_isp_camcording.awb.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AWB);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_EFFECT_CMD(dev,
			init_val_isp_camcording.effect.cmd);
		IS_ISP_SET_PARAM_EFFECT_ERR(dev,
			init_val_isp_camcording.effect.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_IMAGE_EFFECT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_ISO_CMD(dev,
			init_val_isp_camcording.iso.cmd);
		IS_ISP_SET_PARAM_ISO_VALUE(dev,
			init_val_isp_camcording.iso.value);
		IS_ISP_SET_PARAM_ISO_ERR(dev,
			init_val_isp_camcording.iso.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ISO);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
			init_val_isp_camcording.adjust.cmd);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev,
			init_val_isp_camcording.adjust.contrast);
		IS_ISP_SET_PARAM_ADJUST_SATURATION(dev,
			init_val_isp_camcording.adjust.saturation);
		IS_ISP_SET_PARAM_ADJUST_SHARPNESS(dev,
			init_val_isp_camcording.adjust.sharpness);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev,
			init_val_isp_camcording.adjust.exposure);
		IS_ISP_SET_PARAM_ADJUST_BRIGHTNESS(dev,
			init_val_isp_camcording.adjust.brightness);
		IS_ISP_SET_PARAM_ADJUST_HUE(dev,
			init_val_isp_camcording.adjust.hue);
		IS_ISP_SET_PARAM_ADJUST_ERR(dev,
			init_val_isp_camcording.adjust.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_METERING_CMD(dev,
			init_val_isp_camcording.metering.cmd);
		IS_ISP_SET_PARAM_METERING_WIN_POS_X(dev,
			init_val_isp_camcording.metering.win_pos_x);
		IS_ISP_SET_PARAM_METERING_WIN_POS_Y(dev,
			init_val_isp_camcording.metering.win_pos_y);
		IS_ISP_SET_PARAM_METERING_WIN_WIDTH(dev,
			init_val_isp_camcording.metering.win_width);
		IS_ISP_SET_PARAM_METERING_WIN_HEIGHT(dev,
			init_val_isp_camcording.metering.win_height);
		IS_ISP_SET_PARAM_METERING_ERR(dev,
			init_val_isp_camcording.metering.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_METERING);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_AFC_CMD(dev, init_val_isp_camcording.afc.cmd);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev,
			init_val_isp_camcording.afc.manual);
		IS_ISP_SET_PARAM_AFC_ERR(dev, init_val_isp_camcording.afc.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AFC);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_OTF_OUTPUT_CMD(dev,
			init_val_isp_camcording.otf_output.cmd);
		IS_ISP_SET_PARAM_OTF_OUTPUT_WIDTH(dev,
			init_val_isp_camcording.otf_output.width);
		IS_ISP_SET_PARAM_OTF_OUTPUT_HEIGHT(dev,
			init_val_isp_camcording.otf_output.height);
		IS_ISP_SET_PARAM_OTF_OUTPUT_FORMAT(dev,
			init_val_isp_camcording.otf_output.format);
		IS_ISP_SET_PARAM_OTF_OUTPUT_BITWIDTH(dev,
			init_val_isp_camcording.otf_output.bitwidth);
		IS_ISP_SET_PARAM_OTF_OUTPUT_ORDER(dev,
			init_val_isp_camcording.otf_output.order);
		IS_ISP_SET_PARAM_OTF_OUTPUT_ERR(dev,
			init_val_isp_camcording.otf_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_OTF_OUTPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_CMD(dev,
			init_val_isp_camcording.dma1_output.cmd);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_WIDTH(dev,
			init_val_isp_camcording.dma1_output.width);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_HEIGHT(dev,
			init_val_isp_camcording.dma1_output.height);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_FORMAT(dev,
			init_val_isp_camcording.dma1_output.format);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BITWIDTH(dev,
			init_val_isp_camcording.dma1_output.bitwidth);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_PLANE(dev,
			init_val_isp_camcording.dma1_output.plane);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_ORDER(dev,
			init_val_isp_camcording.dma1_output.order);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BUFFER_NUMBER(dev,
			init_val_isp_camcording.dma1_output.buffer_number);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_BUFFER_ADDRESS(dev,
			init_val_isp_camcording.dma1_output.buffer_address);
		IS_ISP_SET_PARAM_DMA_OUTPUT1_ERR(dev,
			init_val_isp_camcording.dma1_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA1_OUTPUT);
		IS_INC_PARAM_NUM(dev);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_CMD(dev,
			init_val_isp_camcording.dma2_output.cmd);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_WIDTH(dev,
			init_val_isp_camcording.dma2_output.width);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_HEIGHT(dev,
			init_val_isp_camcording.dma2_output.height);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_FORMAT(dev,
			init_val_isp_camcording.dma2_output.format);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BITWIDTH(dev,
			init_val_isp_camcording.dma2_output.bitwidth);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_PLANE(dev,
			init_val_isp_camcording.dma2_output.plane);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_ORDER(dev,
			init_val_isp_camcording.dma2_output.order);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BUFFER_NUMBER(dev,
			init_val_isp_camcording.dma2_output.buffer_number);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_BUFFER_ADDRESS(dev,
			init_val_isp_camcording.dma2_output.buffer_address);
		IS_ISP_SET_PARAM_DMA_OUTPUT2_ERR(dev,
			init_val_isp_camcording.dma2_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_DMA2_OUTPUT);
		IS_INC_PARAM_NUM(dev);

		/* DRC */
		IS_DRC_SET_PARAM_CONTROL_CMD(dev,
			init_val_drc_camcording.control.cmd);
		IS_DRC_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_drc_camcording.control.bypass);
		IS_DRC_SET_PARAM_CONTROL_ERR(dev,
			init_val_drc_camcording.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_drc_camcording.otf_input.cmd);
		IS_DRC_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_drc_camcording.otf_input.width);
		IS_DRC_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_drc_camcording.otf_input.height);
		IS_DRC_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_drc_camcording.otf_input.format);
		IS_DRC_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_drc_camcording.otf_input.bitwidth);
		IS_DRC_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_drc_camcording.otf_input.order);
		IS_DRC_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_drc_camcording.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_DMA_INPUT_CMD(dev,
			init_val_drc_camcording.dma_input.cmd);
		IS_DRC_SET_PARAM_DMA_INPUT_WIDTH(dev,
			init_val_drc_camcording.dma_input.width);
		IS_DRC_SET_PARAM_DMA_INPUT_HEIGHT(dev,
			init_val_drc_camcording.dma_input.height);
		IS_DRC_SET_PARAM_DMA_INPUT_FORMAT(dev,
			init_val_drc_camcording.dma_input.format);
		IS_DRC_SET_PARAM_DMA_INPUT_BITWIDTH(dev,
			init_val_drc_camcording.dma_input.bitwidth);
		IS_DRC_SET_PARAM_DMA_INPUT_PLANE(dev,
			init_val_drc_camcording.dma_input.plane);
		IS_DRC_SET_PARAM_DMA_INPUT_ORDER(dev,
			init_val_drc_camcording.dma_input.order);
		IS_DRC_SET_PARAM_DMA_INPUT_BUFFERNUM(dev,
			init_val_drc_camcording.dma_input.buffer_number);
		IS_DRC_SET_PARAM_DMA_INPUT_BUFFERADDR(dev,
			init_val_drc_camcording.dma_input.buffer_address);
		IS_DRC_SET_PARAM_DMA_INPUT_ERR(dev,
			init_val_drc_camcording.dma_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_DMA_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_DRC_SET_PARAM_OTF_OUTPUT_CMD(dev,
			init_val_drc_camcording.otf_output.cmd);
		IS_DRC_SET_PARAM_OTF_OUTPUT_WIDTH(dev,
			init_val_drc_camcording.otf_output.width);
		IS_DRC_SET_PARAM_OTF_OUTPUT_HEIGHT(dev,
			init_val_drc_camcording.otf_output.height);
		IS_DRC_SET_PARAM_OTF_OUTPUT_FORMAT(dev,
			init_val_drc_camcording.otf_output.format);
		IS_DRC_SET_PARAM_OTF_OUTPUT_BITWIDTH(dev,
			init_val_drc_camcording.otf_output.bitwidth);
		IS_DRC_SET_PARAM_OTF_OUTPUT_ORDER(dev,
			init_val_drc_camcording.otf_output.order);
		IS_DRC_SET_PARAM_OTF_OUTPUT_ERR(dev,
			init_val_drc_camcording.otf_output.err);
		IS_SET_PARAM_BIT(dev, PARAM_DRC_OTF_OUTPUT);
		IS_INC_PARAM_NUM(dev);

		/* FD */
		IS_FD_SET_PARAM_CONTROL_CMD(dev,
			init_val_fd_camcording.control.cmd);
		IS_FD_SET_PARAM_CONTROL_BYPASS(dev,
			init_val_fd_camcording.control.bypass);
		IS_FD_SET_PARAM_CONTROL_ERR(dev,
			init_val_fd_camcording.control.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONTROL);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_OTF_INPUT_CMD(dev,
			init_val_fd_camcording.otf_input.cmd);
		IS_FD_SET_PARAM_OTF_INPUT_WIDTH(dev,
			init_val_fd_camcording.otf_input.width);
		IS_FD_SET_PARAM_OTF_INPUT_HEIGHT(dev,
			init_val_fd_camcording.otf_input.height);
		IS_FD_SET_PARAM_OTF_INPUT_FORMAT(dev,
			init_val_fd_camcording.otf_input.format);
		IS_FD_SET_PARAM_OTF_INPUT_BITWIDTH(dev,
			init_val_fd_camcording.otf_input.bitwidth);
		IS_FD_SET_PARAM_OTF_INPUT_ORDER(dev,
			init_val_fd_camcording.otf_input.order);
		IS_FD_SET_PARAM_OTF_INPUT_ERR(dev,
			init_val_fd_camcording.otf_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_DMA_INPUT_CMD(dev,
			init_val_fd_camcording.dma_input.cmd);
		IS_FD_SET_PARAM_DMA_INPUT_WIDTH(dev,
			init_val_fd_camcording.dma_input.width);
		IS_FD_SET_PARAM_DMA_INPUT_HEIGHT(dev,
			init_val_fd_camcording.dma_input.height);
		IS_FD_SET_PARAM_DMA_INPUT_FORMAT(dev,
			init_val_fd_camcording.dma_input.format);
		IS_FD_SET_PARAM_DMA_INPUT_BITWIDTH(dev,
			init_val_fd_camcording.dma_input.bitwidth);
		IS_FD_SET_PARAM_DMA_INPUT_PLANE(dev,
			init_val_fd_camcording.dma_input.plane);
		IS_FD_SET_PARAM_DMA_INPUT_ORDER(dev,
			init_val_fd_camcording.dma_input.order);
		IS_FD_SET_PARAM_DMA_INPUT_BUFFERNUM(dev,
			init_val_fd_camcording.dma_input.buffer_number);
		IS_FD_SET_PARAM_DMA_INPUT_BUFFERADDR(dev,
			init_val_fd_camcording.dma_input.buffer_address);
		IS_FD_SET_PARAM_DMA_INPUT_ERR(dev,
			init_val_fd_camcording.dma_input.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_DMA_INPUT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_FD_RESULT_MAX_NUMBER(dev,
			init_val_fd_preview.result.max_number);
		IS_FD_SET_PARAM_FD_RESULT_ERR(dev,
			init_val_fd_preview.result.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_RESULT);
		IS_INC_PARAM_NUM(dev);
		IS_FD_SET_PARAM_FD_MODE_SMILE(dev,
			init_val_fd_preview.mode.smile);
		IS_FD_SET_PARAM_FD_MODE_BLINK(dev,
			init_val_fd_preview.mode.blink);
		IS_FD_SET_PARAM_FD_MODE_ERR(dev,
			init_val_fd_preview.mode.err);
		IS_SET_PARAM_BIT(dev, PARAM_FD_MODE);
		IS_INC_PARAM_NUM(dev);
		break;
	}
}