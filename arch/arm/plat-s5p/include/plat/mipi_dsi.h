/* linux/arm/arch/mach-s5pc110/include/plat/mipi_dsi.h
 *
 *
 * Copyright (c) 2011 Samsung Electronics
 * InKi Dae <inki.dae <at> samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _MIPI_DSI_H
#define _MIPI_DSI_H

extern struct mipi_dsim_lcd_driver s6e63m0_mipi_lcd_driver;

extern int s5p_mipi_dsi_wr_data(struct mipi_dsim_device *dsim,
	unsigned int data_id, unsigned int data0, unsigned int data1);

enum mipi_ddi_interface {
	RGB_IF = 0x4000,
	I80_IF = 0x8000,
	YUV_601 = 0x10000,
	YUV_656 = 0x20000,
	MIPI_VIDEO = 0x1000,
	MIPI_COMMAND = 0x2000,
};

enum mipi_ddi_panel_select {
	DDI_MAIN_LCD = 0,
	DDI_SUB_LCD = 1,
};

enum mipi_ddi_model {
	S6DR117 = 0,
};

enum mipi_ddi_parameter {
	/* DSIM video interface parameter */
	DSI_VIRTUAL_CH_ID = 0,
	DSI_FORMAT = 1,
	DSI_VIDEO_MODE_SEL = 2,
};

#endif /* _MIPI_DSI_H */