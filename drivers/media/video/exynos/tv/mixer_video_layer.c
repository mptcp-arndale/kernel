/*
 * Samsung TV Mixer driver
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * Tomasz Stanislawski, <t.stanislaws@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundiation. either version 2 of the License,
 * or (at your option) any later version
 */

#include "mixer.h"

#include <media/videobuf2-cma-phys.h>

/* AUXILIARY CALLBACKS */

static void mxr_video_layer_release(struct mxr_layer *layer)
{
	mxr_base_layer_release(layer);
}

static void mxr_video_format_set(struct mxr_layer *layer)
{
	mxr_reg_video_geo(layer->mdev, layer->cur_mxr, layer->idx, &layer->geo);
}

static void mxr_video_fix_geometry(struct mxr_layer *layer)
{
	struct mxr_geometry *geo = &layer->geo;

	geo->dst.x_offset = clamp_val(geo->dst.x_offset, 0,
			geo->dst.full_width - 1);
	geo->dst.y_offset = clamp_val(geo->dst.y_offset, 0,
			geo->dst.full_height - 1);

	/* mixer scale-up is unuseful. so no use it */
	geo->dst.width = clamp_val(geo->src.width, 1,
			geo->dst.full_width - geo->dst.x_offset);
	geo->dst.height = clamp_val(geo->src.height, 1,
			geo->dst.full_height - geo->dst.y_offset);
}

/* PUBLIC API */

struct mxr_layer *mxr_video_layer_create(struct mxr_device *mdev, int cur_mxr,
		int idx)
{
	struct mxr_layer *layer;
	struct mxr_layer_ops ops = {
		.release = mxr_video_layer_release,
		.format_set = mxr_video_format_set,
		.fix_geometry = mxr_video_fix_geometry,
	};

	layer = kzalloc(sizeof *layer, GFP_KERNEL);
	if (layer == NULL) {
		mxr_err(mdev, "not enough memory for layer.\n");
		goto fail;
	}

	layer->mdev = mdev;
	layer->idx = idx;
	layer->ops = ops;

	layer->cur_mxr = cur_mxr;
	return layer;

fail:
	return NULL;
}