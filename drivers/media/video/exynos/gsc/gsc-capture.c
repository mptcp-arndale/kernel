/* linux/drivers/media/video/exynos/gsc/gsc-capture.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS5 SoC series G-scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/bug.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/string.h>
#include <linux/i2c.h>
#include <media/v4l2-ioctl.h>
#include <media/exynos_gscaler.h>

#include "gsc-core.h"

static int queue_setup(struct vb2_queue *vq, unsigned int *num_buffers,
		       unsigned int *num_planes, unsigned long sizes[],
		       void *allocators[])
{
	struct gsc_ctx *ctx = vq->drv_priv;
	struct gsc_fmt *fmt = ctx->s_frame.fmt;
	int i;

	if (!fmt)
		return -EINVAL;

	*num_planes = fmt->num_planes;

	for (i = 0; i < fmt->num_planes; i++) {
		sizes[i] = get_plane_size(&ctx->d_frame, i);
		allocators[i] = ctx->gsc_dev->alloc_ctx;
	}

	return 0;
}
static int buffer_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct gsc_ctx *ctx = vq->drv_priv;
	struct gsc_dev *gsc = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->d_frame;
	int i;

	if (frame->fmt == NULL)
		return -EINVAL;

	for (i = 0; i < frame->fmt->num_planes; i++) {
		unsigned long size = frame->payload[i];

		if (vb2_plane_size(vb, i) < size) {
			v4l2_err(ctx->gsc_dev->cap.vfd,
				 "User buffer too small (%ld < %ld)\n",
				 vb2_plane_size(vb, i), size);
			return -EINVAL;
		}
		vb2_set_plane_payload(vb, i, size);
	}

	if (frame->cacheable)
		gsc->vb2->cache_flush(vb, frame->fmt->num_planes);

	return 0;
}

int gsc_cap_pipeline_s_stream(struct gsc_dev *gsc, int on)
{
	struct gsc_pipeline *p = &gsc->pipeline;
	int ret = 0;

	if (!p->sensor || !p->flite)
		return -ENODEV;

	if (on) {
		ret = v4l2_subdev_call(p->flite, video, s_stream, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
		ret = v4l2_subdev_call(p->csis, video, s_stream, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
		ret = v4l2_subdev_call(p->sensor, video, s_stream, 1);
	} else {
		ret = v4l2_subdev_call(p->sensor, video, s_stream, 0);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
		ret = v4l2_subdev_call(p->csis, video, s_stream, 0);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
		ret = v4l2_subdev_call(p->flite, video, s_stream, 0);
	}

	return ret == -ENOIOCTLCMD ? 0 : ret;
}

static void buffer_queue(struct vb2_buffer *vb)
{
	struct gsc_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct gsc_dev *gsc = ctx->gsc_dev;
	struct gsc_capture_device *cap = &gsc->cap;
	int min_bufs;
	unsigned long flags;

	spin_lock_irqsave(&gsc->slock, flags);
	if (!test_bit(ST_CAPT_SUSPENDED, &gsc->state) &&
	    !test_bit(ST_CAPT_STREAM, &gsc->state)) {
		gsc_dbg("buf_index : %d\n", cap->buf_index);
		gsc_hw_set_output_buf_masking(gsc, cap->buf_index, 0);
	}

	min_bufs = cap->reqbufs_cnt > 1 ? 2 : 1;

	if (vb2_is_streaming(&cap->vbq) &&
		(gsc_hw_get_nr_unmask_bits(gsc) >= min_bufs) &&
		!test_bit(ST_CAPT_STREAM, &gsc->state)) {
		gsc_hw_enable_control(gsc, true);

		if (!test_and_set_bit(ST_CAPT_PIPE_STREAM, &gsc->state)) {
			spin_unlock_irqrestore(&gsc->slock, flags);
			gsc_cap_pipeline_s_stream(gsc, 1);
			return;
		}
	}
	spin_unlock_irqrestore(&gsc->slock, flags);

	return;
}

static int gsc_capture_get_scaler_factor(u32 src, u32 tar, u32 *ratio)
{
	u32 sh = 3;
	tar *= 4;
	if (tar >= src) {
		*ratio = 0;
		return 0;
	}

	while (--sh) {
		u32 tmp = 1 << sh;
		if (src >= tar * tmp)
			*ratio = sh;
	}
	return 0;
}

static int gsc_capture_scaler_info(struct gsc_ctx *ctx)
{
	struct gsc_frame *s_frame = &ctx->s_frame;
	struct gsc_frame *d_frame = &ctx->d_frame;
	struct gsc_scaler *sc = &ctx->scaler;

	gsc_capture_get_scaler_factor(s_frame->crop.width, d_frame->crop.width,
				      &sc->pre_hratio);
	gsc_capture_get_scaler_factor(s_frame->crop.width, d_frame->crop.width,
				      &sc->pre_vratio);

	sc->main_hratio = (s_frame->crop.width / d_frame->crop.width) * 2 * (1 << 16);
	sc->main_vratio = (s_frame->crop.height / d_frame->crop.height) * 2 * (1 << 16);

	gsc_dbg("src width : %d, src height : %d, dst width : %d,\
		dst height : %d\n", s_frame->crop.width, s_frame->crop.height,\
		d_frame->crop.width, d_frame->crop.height);
	gsc_dbg("pre_hratio : 0x%x, pre_vratio : 0x%x, main_hratio : 0x%x,\
			main_vratio : 0x%x\n", sc->pre_hratio,\
			sc->pre_vratio, sc->main_hratio, sc->main_vratio);

	return 0;
}

static int start_streaming(struct vb2_queue *q)
{
	struct gsc_ctx *ctx = q->drv_priv;
	struct gsc_dev *gsc = ctx->gsc_dev;
	struct gsc_capture_device *cap = &gsc->cap;
	struct vb2_buffer *vb;
	int ret, i, min_bufs;

	gsc_hw_set_input_path(ctx);
	gsc_hw_set_in_size(ctx);
	gsc_hw_set_in_image_format(ctx);
	gsc_hw_set_output_path(ctx);
	gsc_hw_set_out_size(ctx);
	gsc_hw_set_out_image_format(ctx);
	gsc_hw_set_global_alpha(ctx);

	gsc_capture_scaler_info(ctx);
	gsc_hw_set_prescaler(ctx);
	gsc_hw_set_mainscaler(ctx);

	gsc_hw_set_output_buf_mask_all(gsc);
	for (i = 0; i < cap->reqbufs_cnt; i++) {
		vb = q->bufs[i];
		ret = gsc_prepare_addr(ctx, vb, &ctx->d_frame, &ctx->d_frame.addr);
		if (ret) {
			gsc_err("Prepare G-Scaler address failed\n");
			return -EINVAL;
		}
		gsc_hw_set_output_addr(gsc, &ctx->d_frame.addr, vb->v4l2_buf.index);
	}

	min_bufs = cap->reqbufs_cnt > 1 ? 2 : 1;
	if ((gsc_hw_get_nr_unmask_bits(gsc) >= min_bufs) &&
		!test_bit(ST_CAPT_STREAM, &gsc->state)) {
		gsc_hw_enable_control(gsc, true);

		if (!test_and_set_bit(ST_CAPT_PIPE_STREAM, &gsc->state))
			gsc_cap_pipeline_s_stream(gsc, 1);
	}

	return 0;
}

static int gsc_capture_state_cleanup(struct gsc_dev *gsc)
{
	unsigned long flags;
	bool streaming;

	spin_lock_irqsave(&gsc->slock, flags);
	streaming = gsc->state & (1 << ST_CAPT_PIPE_STREAM);

	gsc->state &= ~(1 << ST_CAPT_RUN | 1 << ST_CAPT_STREAM |
			1 << ST_CAPT_PIPE_STREAM | 1 << ST_CAPT_PEND);

	set_bit(ST_CAPT_SUSPENDED, &gsc->state);
	spin_unlock_irqrestore(&gsc->slock, flags);

	if (streaming)
		return gsc_cap_pipeline_s_stream(gsc, 0);
	else
		return 0;
}

static int gsc_cap_stop_capture(struct gsc_dev *gsc)
{
	int ret;
	if (!gsc_cap_active(gsc)) {
		gsc_warn("already stopped\n");
		return 0;
	}
	gsc_hw_enable_control(gsc, false);
	ret = gsc_wait_operating(gsc);
	if (ret) {
		gsc_err("GSCALER_OP_STATUS is operating\n");
		return ret;
	}

	return gsc_capture_state_cleanup(gsc);
}

static int stop_streaming(struct vb2_queue *q)
{
	struct gsc_ctx *ctx = q->drv_priv;
	struct gsc_dev *gsc = ctx->gsc_dev;

	if (!gsc_cap_active(gsc))
		return -EINVAL;

	return gsc_cap_stop_capture(gsc);
}

static struct vb2_ops gsc_capture_qops = {
	.queue_setup		= queue_setup,
	.buf_prepare		= buffer_prepare,
	.buf_queue		= buffer_queue,
	.wait_prepare		= gsc_unlock,
	.wait_finish		= gsc_lock,
	.start_streaming	= start_streaming,
	.stop_streaming		= stop_streaming,
};

/*
 * The video node ioctl operations
 */
static int gsc_vidioc_querycap_capture(struct file *file, void *priv,
				       struct v4l2_capability *cap)
{
	struct gsc_dev *gsc = video_drvdata(file);

	strncpy(cap->driver, gsc->pdev->name, sizeof(cap->driver) - 1);
	strncpy(cap->card, gsc->pdev->name, sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE_MPLANE;

	return 0;
}

static int gsc_capture_enum_fmt_mplane(struct file *file, void *priv,
				    struct v4l2_fmtdesc *f)
{
	return gsc_enum_fmt_mplane(f);
}

static int gsc_capture_try_fmt_mplane(struct file *file, void *fh,
				   struct v4l2_format *f)
{
	struct gsc_dev *gsc = video_drvdata(file);

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	return gsc_try_fmt_mplane(gsc, f);
}

static int gsc_capture_s_fmt_mplane(struct file *file, void *fh,
				 struct v4l2_format *f)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_ctx *ctx = gsc->cap.ctx;
	struct gsc_frame *frame;
	struct v4l2_pix_format_mplane *pix;
	int i, ret = 0;

	ret = gsc_capture_try_fmt_mplane(file, fh, f);
	if (ret)
		return ret;

	if (vb2_is_streaming(&gsc->cap.vbq)) {
		printk("queue (%d) busy\n", f->type);
		return -EBUSY;
	}

	frame = &ctx->d_frame;

	pix = &f->fmt.pix_mp;
	frame->fmt = find_format(&pix->pixelformat, NULL, 0);
	if (!frame->fmt)
		return -EINVAL;

	for (i = 0; i < frame->fmt->nr_comp; i++)
		frame->payload[i] =
			pix->plane_fmt[i].bytesperline * pix->height;

	gsc_set_frame_size(frame, pix->width, pix->height);

	gsc_dbg("f_w: %d, f_h: %d", frame->f_width, frame->f_height);

	return 0;
}

static int gsc_capture_g_fmt_mplane(struct file *file, void *fh,
				 struct v4l2_format *f)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_ctx *ctx = gsc->cap.ctx;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	return gsc_g_fmt_mplane(ctx, f);
}

static int gsc_capture_reqbufs(struct file *file, void *priv,
			    struct v4l2_requestbuffers *reqbufs)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_capture_device *cap = &gsc->cap;
	struct gsc_frame *frame;
	int ret;

	frame = ctx_get_frame(cap->ctx, reqbufs->type);
	frame->cacheable = cap->ctx->ctrl_val.cacheable;
	gsc->vb2->set_cacheable(gsc->alloc_ctx, frame->cacheable);

	ret = vb2_reqbufs(&cap->vbq, reqbufs);
	if (!ret)
		cap->reqbufs_cnt = reqbufs->count;

	return ret;

}

static int gsc_capture_querybuf(struct file *file, void *priv,
			   struct v4l2_buffer *buf)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_capture_device *cap = &gsc->cap;

	return vb2_querybuf(&cap->vbq, buf);
}

static int gsc_capture_qbuf(struct file *file, void *priv,
			  struct v4l2_buffer *buf)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_capture_device *cap = &gsc->cap;

	return vb2_qbuf(&cap->vbq, buf);
}

static int gsc_capture_dqbuf(struct file *file, void *priv,
			   struct v4l2_buffer *buf)
{
	struct gsc_dev *gsc = video_drvdata(file);
	return vb2_dqbuf(&gsc->cap.vbq, buf,
		file->f_flags & O_NONBLOCK);
}

static int gsc_capture_cropcap(struct file *file, void *fh,
			    struct v4l2_cropcap *cr)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_ctx *ctx = gsc->cap.ctx;

	if (cr->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	cr->bounds.left		= 0;
	cr->bounds.top		= 0;
	cr->bounds.width	= ctx->d_frame.f_width;
	cr->bounds.height	= ctx->d_frame.f_height;
	cr->defrect		= cr->bounds;

	return 0;
}

static int gsc_capture_enum_input(struct file *file, void *priv,
			       struct v4l2_input *i)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct exynos_platform_gscaler *pdata = gsc->pdata;
	struct exynos_gscaler_isp_info *isp_info;

	if (i->index >= GSC_MAX_CAMIF_CLIENTS)
		return -EINVAL;

	isp_info = pdata->isp_info[i->index];
	if (isp_info == NULL)
		return -EINVAL;

	i->type = V4L2_INPUT_TYPE_CAMERA;

	strncpy(i->name, isp_info->board_info->type, 32);

	return 0;
}

static int gsc_capture_s_input(struct file *file, void *priv, unsigned int i)
{
	return i == 0 ? 0 : -EINVAL;
}

static int gsc_capture_g_input(struct file *file, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

int gsc_capture_ctrls_create(struct gsc_dev *gsc)
{
	int ret;

	if (WARN_ON(gsc->cap.ctx == NULL))
		return -ENXIO;
	if (gsc->cap.ctx->ctrls_rdy)
		return 0;
	ret = gsc_ctrls_create(gsc->cap.ctx);
	if (ret)
		return ret;

	return 0;
}

void gsc_cap_pipeline_prepare(struct gsc_dev *gsc, struct media_entity *me)
{
	struct media_entity_graph graph;
	struct v4l2_subdev *sd;

	media_entity_graph_walk_start(&graph, me);

	while((me = media_entity_graph_walk_next(&graph))) {
		if (media_entity_type(me) != MEDIA_ENT_T_V4L2_SUBDEV) {
			gsc_warn("entity name : %s\n", me->name);
			continue;
		}
		sd = media_entity_to_v4l2_subdev(me);

		if (sd->grp_id == FLITE_GRP_ID)
			gsc->pipeline.flite = sd;
		else if (sd->grp_id == SENSOR_GRP_ID)
			gsc->pipeline.sensor = sd;
		else if (sd->grp_id == CSIS_GRP_ID)
			gsc->pipeline.csis = sd;
	}
}

static int __subdev_set_power(struct v4l2_subdev *sd, int on)
{
	int *use_count;
	int ret;

	if (sd == NULL)
		return -ENXIO;

	use_count = &sd->entity.use_count;
	if (on && (*use_count)++ > 0)
		return 0;
	else if (!on && (*use_count == 0 || --(*use_count) > 0))
		return 0;
	ret = v4l2_subdev_call(sd, core, s_power, on);

	return ret != -ENOIOCTLCMD ? ret : 0;
}

int gsc_cap_pipeline_s_power(struct gsc_dev *gsc, int state)
{
	int ret = 0;

	if (!gsc->pipeline.sensor || !gsc->pipeline.flite)
		return -ENXIO;

	if (state) {
		ret = __subdev_set_power(gsc->pipeline.flite, 1);
		if (ret && ret != -ENXIO)
			return ret;
		ret = __subdev_set_power(gsc->pipeline.csis, 1);
		if (ret && ret != -ENXIO)
			return ret;
		ret = __subdev_set_power(gsc->pipeline.sensor, 1);
	} else {
		ret = __subdev_set_power(gsc->pipeline.flite, 0);
		if (ret && ret != -ENXIO)
			return ret;
		ret = __subdev_set_power(gsc->pipeline.sensor, 0);
		if (ret && ret != -ENXIO)
			return ret;
		ret = __subdev_set_power(gsc->pipeline.csis, 0);
	}
	return ret == -ENXIO ? 0 : ret;
}

static int __gsc_cap_pipeline_initialize(struct gsc_dev *gsc,
					 struct media_entity *me, bool prep)
{
	//int ret;

	if (prep)
		gsc_cap_pipeline_prepare(gsc, me);
	if (!gsc->pipeline.sensor || !gsc->pipeline.flite)
		return -EINVAL;
/* Fix me : need to clock set for camera */
/*	ret = gsc_cap_set_camclk(gsc->pipeline.sensor, true);
	if (ret)
		return ret;
*/
	return gsc_cap_pipeline_s_power(gsc, 1);
}

int gsc_cap_pipeline_initialize(struct gsc_dev *gsc, struct media_entity *me,
				bool prep)
{
	int ret;

	mutex_lock(&me->parent->graph_mutex);
	ret =  __gsc_cap_pipeline_initialize(gsc, me, prep);
	mutex_unlock(&me->parent->graph_mutex);

	return ret;
}
static int gsc_capture_open(struct file *file)
{
	struct gsc_dev *gsc = video_drvdata(file);
	int ret = v4l2_fh_open(file);

	if (ret)
		return ret;

	gsc_dbg("pid: %d, state: 0x%lx", task_pid_nr(current), gsc->state);

	if (gsc_m2m_opened(gsc) || gsc_out_opened(gsc) || gsc_cap_opened(gsc)) {
		v4l2_fh_release(file);
		return -EBUSY;
	}

	set_bit(ST_CAPT_OPEN, &gsc->state);
	pm_runtime_get_sync(&gsc->pdev->dev);

	if (++gsc->cap.refcnt == 1) {
		ret = gsc_cap_pipeline_initialize(gsc, &gsc->cap.vfd->entity, true);
		if (ret < 0) {
			gsc_err("gsc pipeline initialization failed\n");
			goto err;
		}

		ret = gsc_capture_ctrls_create(gsc);
		if (ret) {
			gsc_err("failed to create controls\n");
			goto err;
		}
	}

	return 0;

err:
	pm_runtime_put_sync(&gsc->pdev->dev);
	v4l2_fh_release(file);
	clear_bit(ST_CAPT_OPEN, &gsc->state);
	return ret;
}

int __gsc_cap_pipeline_shutdown(struct gsc_dev *gsc)
{
	int ret = 0;

	if (gsc->pipeline.sensor && gsc->pipeline.flite) {
		ret = gsc_cap_pipeline_s_power(gsc, 0);
//Fix me	gsc_md_set_camclk(gsc->pipeline.sensor, false);
	}
	return ret == -ENXIO ? 0 : ret;
}

int gsc_cap_pipeline_shutdown(struct gsc_dev *gsc)
{
	struct media_entity *me = &gsc->cap.vfd->entity;
	int ret;

	mutex_lock(&me->parent->graph_mutex);
	ret = __gsc_cap_pipeline_shutdown(gsc);
	mutex_unlock(&me->parent->graph_mutex);

	return ret;
}
static int gsc_capture_close(struct file *file)
{
	struct gsc_dev *gsc = video_drvdata(file);

	gsc_dbg("pid: %d, state: 0x%lx", task_pid_nr(current), gsc->state);

	if (--gsc->cap.refcnt == 0) {
		clear_bit(ST_CAPT_OPEN, &gsc->state);
		gsc_hw_enable_control(gsc, false);
		gsc_cap_pipeline_shutdown(gsc);
		clear_bit(ST_CAPT_SUSPENDED, &gsc->state);
	}

	pm_runtime_put(&gsc->pdev->dev);

	if (gsc->cap.refcnt == 0) {
		vb2_queue_release(&gsc->cap.vbq);
		gsc_ctrls_delete(gsc->cap.ctx);
	}

	return v4l2_fh_release(file);
}

static unsigned int gsc_capture_poll(struct file *file,
				      struct poll_table_struct *wait)
{
	struct gsc_dev *gsc = video_drvdata(file);

	return vb2_poll(&gsc->cap.vbq, file, wait);
}

static int gsc_capture_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct gsc_dev *gsc = video_drvdata(file);

	return vb2_mmap(&gsc->cap.vbq, vma);
}

static int gsc_cap_link_validate(struct gsc_dev *gsc)
{
	struct gsc_capture_device *cap = &gsc->cap;
	struct v4l2_subdev_format sink_fmt, src_fmt;
	struct v4l2_subdev *sd;
	struct media_pad *pad;
	int ret;

	/* Get the source pad connected with gsc-video */
	pad =  media_entity_remote_source(&cap->vd_pad);
	if (pad == NULL)
		return -EPIPE;
	/* Get the subdev of source pad */
	sd = media_entity_to_v4l2_subdev(pad->entity);

	while(1) {
		/* Find sink pad of the subdev*/
		pad = &sd->entity.pads[0];
		if (!(pad->flags & MEDIA_PAD_FL_SINK))
			break;
		if (sd == cap->sd_cap) {
			struct gsc_frame *gf = &cap->ctx->s_frame;
			sink_fmt.format.width = gf->crop.width;
			sink_fmt.format.height = gf->crop.height;
			sink_fmt.format.code = gf->fmt ? gf->fmt->mbus_code : 0;
		} else {
			sink_fmt.pad = pad->index;
			sink_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &sink_fmt);
			if (ret < 0 && ret != -ENOIOCTLCMD)
				return -EPIPE;
		}
		/* Get the source pad connected with remote sink pad */
		pad = media_entity_remote_source(pad);
		if (pad == NULL ||
		    media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV)
			break;

		/* Get the subdev of source pad */
		sd = media_entity_to_v4l2_subdev(pad->entity);
		src_fmt.pad = pad->index;
		src_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &src_fmt);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EPIPE;

		if (src_fmt.format.width != sink_fmt.format.width ||
		    src_fmt.format.height != sink_fmt.format.height ||
		    src_fmt.format.code != sink_fmt.format.code)
			return -EPIPE;
	}

	return 0;
}

static int gsc_capture_streamon(struct file *file, void *priv,
				enum v4l2_buf_type type)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_pipeline *p = &gsc->pipeline;
	int ret;

	if (gsc_cap_active(gsc))
		return -EBUSY;

	media_entity_pipeline_start(&p->sensor->entity, p->pipe);

	ret = gsc_cap_link_validate(gsc);
	if (ret)
		return ret;

	return vb2_streamon(&gsc->cap.vbq, type);
}

static int gsc_capture_streamoff(struct file *file, void *priv,
			    enum v4l2_buf_type type)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct v4l2_subdev *sd = gsc->pipeline.sensor;
	int ret;

	ret = vb2_streamoff(&gsc->cap.vbq, type);
	if (ret == 0)
		media_entity_pipeline_stop(&sd->entity);
	return ret;
}

static struct v4l2_subdev * gsc_cap_remote_subdev(struct gsc_dev *gsc, u32 *pad)
{
	struct media_pad *remote;

	remote = media_entity_remote_source(&gsc->cap.vd_pad);

	if (remote == NULL ||
	    media_entity_type(remote->entity) != MEDIA_ENT_T_V4L2_SUBDEV)
		return NULL;

	if (pad)
		*pad = remote->index;

	return media_entity_to_v4l2_subdev(remote->entity);
}

static int gsc_capture_g_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct v4l2_subdev_format format;
	struct v4l2_subdev *subdev;
	u32 pad;
	int ret;

	subdev = gsc_cap_remote_subdev(gsc, &pad);
	if (subdev == NULL)
		return -EINVAL;

	/* Try the get crop operation first and fallback to get format if not
	 * implemented.
	 */
	ret = v4l2_subdev_call(subdev, video, g_crop, crop);
	if (ret != -ENOIOCTLCMD)
		return ret;

	format.pad = pad;
	format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(subdev, pad, get_fmt, NULL, &format);
	if (ret < 0)
		return ret == -ENOIOCTLCMD ? -EINVAL : ret;

	crop->c.left = 0;
	crop->c.top = 0;
	crop->c.width = format.format.width;
	crop->c.height = format.format.height;

	return 0;
}

static int gsc_capture_s_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct v4l2_subdev *subdev;
	int ret;

	subdev = gsc_cap_remote_subdev(gsc, NULL);
	if (subdev == NULL)
		return -EINVAL;

	ret = v4l2_subdev_call(subdev, video, s_crop, crop);

	return ret == -ENOIOCTLCMD ? -EINVAL : ret;
}


static const struct v4l2_ioctl_ops gsc_capture_ioctl_ops = {
	.vidioc_querycap		= gsc_vidioc_querycap_capture,

	.vidioc_enum_fmt_vid_cap_mplane	= gsc_capture_enum_fmt_mplane,
	.vidioc_try_fmt_vid_cap_mplane	= gsc_capture_try_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= gsc_capture_s_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= gsc_capture_g_fmt_mplane,

	.vidioc_reqbufs			= gsc_capture_reqbufs,
	.vidioc_querybuf		= gsc_capture_querybuf,

	.vidioc_qbuf			= gsc_capture_qbuf,
	.vidioc_dqbuf			= gsc_capture_dqbuf,

	.vidioc_streamon		= gsc_capture_streamon,
	.vidioc_streamoff		= gsc_capture_streamoff,

	.vidioc_g_crop			= gsc_capture_g_crop,
	.vidioc_s_crop			= gsc_capture_s_crop,
	.vidioc_cropcap			= gsc_capture_cropcap,

	.vidioc_enum_input		= gsc_capture_enum_input,
	.vidioc_s_input			= gsc_capture_s_input,
	.vidioc_g_input			= gsc_capture_g_input,
};

static const struct v4l2_file_operations gsc_capture_fops = {
	.owner		= THIS_MODULE,
	.open		= gsc_capture_open,
	.release	= gsc_capture_close,
	.poll		= gsc_capture_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= gsc_capture_mmap,
};

/*
 * __gsc_cap_get_format - helper function for getting gscaler format
 * @res   : pointer to resizer private structure
 * @pad   : pad number
 * @fh    : V4L2 subdev file handle
 * @which : wanted subdev format
 * return zero
 */
static struct v4l2_mbus_framefmt *__gsc_cap_get_format(struct gsc_dev *gsc,
				struct v4l2_subdev_fh *fh, unsigned int pad,
				enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);
	else
		return &gsc->cap.mbus_fmt[pad];
}
static void gsc_cap_check_limit_size(struct gsc_dev *gsc, unsigned int pad,
				   struct v4l2_mbus_framefmt *fmt)
{
	struct gsc_variant *variant = gsc->variant;
	struct gsc_ctx *ctx = gsc->cap.ctx;
	u32 min_w, min_h, max_w, max_h;

	switch(pad) {
		case GSC_PAD_SINK:
			if (ctx->ctrl_val.rot == 90 ||
			    ctx->ctrl_val.rot == 270) {
				min_w = variant->pix_min->real_w;
				min_h = variant->pix_min->real_h;
				max_w = variant->pix_max->real_rot_en_w;
				max_h = variant->pix_max->real_rot_en_h;
			} else {
				min_w = variant->pix_min->real_w;
				min_h = variant->pix_min->real_h;
				max_w = variant->pix_max->real_rot_dis_w;
				max_h = variant->pix_max->real_rot_dis_h;
			}
			break;

		case GSC_PAD_SOURCE:
			min_w = variant->pix_min->target_w;
			min_h = variant->pix_min->target_h;
			max_w = variant->pix_max->target_w;
			max_h = variant->pix_max->target_h;
			break;
	}

	fmt->width = clamp_t(u32, fmt->width, min_w, max_w);
	fmt->height = clamp_t(u32, fmt->width, min_h, max_h);
}
static void gsc_cap_try_format(struct gsc_dev *gsc,
			       struct v4l2_subdev_fh *fh, unsigned int pad,
			       struct v4l2_mbus_framefmt *fmt,
			       enum v4l2_subdev_format_whence which)
{
	struct gsc_fmt *gfmt;

	gfmt = find_format(NULL, &fmt->code, 0);
	WARN_ON(!gfmt);

	if (pad == GSC_PAD_SINK) {
		if (gfmt->flags != FMT_FLAGS_CAM)
			gsc_warn("Not supported format of gsc capture\n");
	}

	gsc_cap_check_limit_size(gsc, pad, fmt);

	fmt->colorspace = V4L2_COLORSPACE_JPEG;
	fmt->field = V4L2_FIELD_NONE;
}

static int gsc_capture_subdev_set_fmt(struct v4l2_subdev *sd,
				      struct v4l2_subdev_fh *fh,
				      struct v4l2_subdev_format *fmt)
{
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mf;
	struct gsc_ctx *ctx = gsc->cap.ctx;
	struct gsc_frame *gframe;

	mf = __gsc_cap_get_format(gsc, fh, fmt->pad, fmt->which);
	if (mf == NULL)
		return -EINVAL;

	gsc_cap_try_format(gsc, fh, fmt->pad, &fmt->format, fmt->which);
	*mf = fmt->format;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	if (fmt->pad == GSC_PAD_SINK)
		gframe = &ctx->s_frame;
	else
		gframe = &ctx->d_frame;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		gframe->crop.left = 0;
		gframe->crop.top = 0;
		gframe->f_width = mf->width;
		gframe->f_height = mf->height;
		gframe->crop.width = mf->width;
		gframe->crop.height = mf->height;
	}

	return 0;
}

static int gsc_capture_subdev_get_fmt(struct v4l2_subdev *sd,
				      struct v4l2_subdev_fh *fh,
				      struct v4l2_subdev_format *fmt)
{
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mf;

	mf = __gsc_cap_get_format(gsc, fh, fmt->pad, fmt->which);
	if (mf == NULL)
		return -EINVAL;

	fmt->format = *mf;

	return 0;
}

static struct v4l2_rect *__gsc_cap_get_crop(struct gsc_dev *gsc,
					    struct v4l2_subdev_fh *fh,
					    unsigned int pad,
					    enum v4l2_subdev_format_whence which,
					    struct v4l2_rect *crop)
{
	struct gsc_ctx *ctx = gsc->cap.ctx;
	struct gsc_frame *gframe = &ctx->d_frame;

	if (which == V4L2_SUBDEV_FORMAT_TRY) {
		return v4l2_subdev_get_try_crop(fh, pad);
	} else {
		crop->left = gframe->crop.left;
		crop->top = gframe->crop.top;
		crop->width = gframe->crop.width;
		crop->height = gframe->crop.height;

		return crop;
	}
}

static void gsc_cap_try_crop(struct gsc_dev *gsc, struct v4l2_rect *crop)
{
	struct gsc_variant *variant = gsc->variant;
	struct gsc_ctx *ctx = gsc->cap.ctx;
	struct gsc_frame *gframe = &ctx->d_frame;

	u32 crop_min_w = variant->pix_min->target_w;
	u32 crop_min_h = variant->pix_min->target_h;
	u32 crop_max_w = gframe->f_width;
	u32 crop_max_h = gframe->f_height;

	crop->left = clamp_t(u32, crop->left, 0, crop_max_w - crop_min_w);
	crop->top = clamp_t(u32, crop->top, 0, crop_max_h - crop_min_h);
	crop->width = clamp_t(u32, crop->width, crop_min_w, crop_max_w);
	crop->height = clamp_t(u32, crop->height, crop_min_h, crop_max_h);
}

static int gsc_capture_subdev_set_crop(struct v4l2_subdev *sd,
				       struct v4l2_subdev_fh *fh,
				       struct v4l2_subdev_crop *crop)
{
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct gsc_ctx *ctx = gsc->cap.ctx;
	struct gsc_frame *gframe = &ctx->d_frame;
	struct v4l2_rect *gcrop = NULL;

	if (crop->pad == GSC_PAD_SINK)
		return -EINVAL;

	gsc_cap_try_crop(gsc, &crop->rect);

	*__gsc_cap_get_crop(gsc, fh, crop->pad, crop->which, gcrop) = crop->rect;

	if (crop->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		gframe->crop.left = gcrop->left;
		gframe->crop.top = gcrop->top;
		gframe->crop.width = gcrop->width;
		gframe->crop.height = gcrop->height;
	}

	return 0;
}

static int gsc_capture_subdev_get_crop(struct v4l2_subdev *sd,
				       struct v4l2_subdev_fh *fh,
				       struct v4l2_subdev_crop *crop)
{
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct v4l2_rect *gcrop = NULL;

	if (crop->pad == GSC_PAD_SINK)
		return -EINVAL;

	crop->rect = *__gsc_cap_get_crop(gsc, fh, crop->pad, crop->which, gcrop);

	return 0;
}

static struct v4l2_subdev_pad_ops gsc_cap_subdev_pad_ops = {
	.get_fmt = gsc_capture_subdev_get_fmt,
	.set_fmt = gsc_capture_subdev_set_fmt,
	.get_crop = gsc_capture_subdev_get_crop,
	.set_crop = gsc_capture_subdev_set_crop,
};

static struct v4l2_subdev_video_ops gsc_cap_video_ops = {
};

static struct v4l2_subdev_ops gsc_cap_subdev_ops = {
	.pad = &gsc_cap_subdev_pad_ops,
	.video = &gsc_cap_video_ops,
};

static int gsc_capture_init_formats(struct v4l2_subdev *sd,
				    struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;

	memset(&format, 0, sizeof(format));
	format.pad = GSC_PAD_SINK;
	format.which = fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	format.format.code = V4L2_MBUS_FMT_YUYV8_2X8;
	format.format.width = DEFAULT_GSC_SINK_WIDTH;
	format.format.height = DEFAULT_GSC_SINK_HEIGHT;
	gsc_capture_subdev_set_fmt(sd, fh, &format);

	return 0;
}

static const struct v4l2_subdev_internal_ops gsc_cap_v4l2_internal_ops = {
	.open = gsc_capture_init_formats,
};

static int gsc_capture_link_setup(struct media_entity *entity,
				  const struct media_pad *local,
				  const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct gsc_capture_device *cap = &gsc->cap;
	struct media_pad *pad;

	switch (local->index | media_entity_type(remote->entity)) {
	case GSC_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (gsc->cap.input != 0)
				return -EBUSY;
			pad =
			media_entity_remote_source(&cap->sd_pads[GSC_PAD_SINK]);
			if (pad->index == FLITE_PAD_SOURCE_PREVIEW)
				gsc->cap.input |= GSC_IN_FLITE_PREVIEW;
			else
				gsc->cap.input |= GSC_IN_FLITE_CAMCORDING;
		} else {
			gsc->cap.input = GSC_IN_NONE;
		}
		break;
	case GSC_PAD_SOURCE | MEDIA_ENT_T_DEVNODE:
		/* gsc-cap always write to memory */
		break;
	}

	return 0;
}

static const struct media_entity_operations gsc_cap_media_ops = {
	.link_setup = gsc_capture_link_setup,
};

static int gsc_capture_create_subdev(struct gsc_dev *gsc)
{
	struct v4l2_device *v4l2_dev;
	struct v4l2_subdev *sd;
	int ret;

	sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!sd)
	       return -ENOMEM;

	v4l2_subdev_init(sd, &gsc_cap_subdev_ops);
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(sd->name, sizeof(sd->name), "GSC.%d", gsc->id);

	gsc->cap.sd_pads[GSC_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	gsc->cap.sd_pads[GSC_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&sd->entity, GSC_PADS_NUM,
				gsc->cap.sd_pads, 0);
	if (ret)
		goto error;

	sd->internal_ops = &gsc_cap_v4l2_internal_ops;
	sd->entity.ops = &gsc_cap_media_ops;
	v4l2_dev = &gsc->mdev[MDEV_CAPTURE]->v4l2_dev;

	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret) {
		media_entity_cleanup(&sd->entity);
		goto error;
	}

//	gsc->mdev[MDEV_CAPTURE]->gsc_cap_sd[gsc->id] = sd;
//	printk("%s : gsc_sd[%d] = 0x%08x\n", __func__, gsc->id,
//			(u32)gsc->mdev[MDEV_CAPTURE]->gsc_cap_sd[gsc->id]);
	gsc->cap.sd_cap = sd;
	v4l2_set_subdevdata(sd, gsc);

	return 0;
error:
	kfree(sd);
	return ret;
}

static int gsc_capture_create_link(struct gsc_dev *gsc)
{
	struct media_entity *source, *sink;
	struct exynos_platform_gscaler *pdata = gsc->pdata;
	struct exynos_gscaler_isp_info *isp_info;
	u32 num_clients = pdata->num_clients;
	int ret, i;
	u32 id;

	/* link sensor to mipi-csis */
	for (i = 0; i < num_clients; i++) {
		isp_info = pdata->isp_info[i];
		id = isp_info->mux_id;
		switch (isp_info->bus_type) {
		case GSC_ITU_601:
			/*	SENSOR ------> FIMC-LITE	*/
			source = &gsc->cap.sd_sensor[i]->entity;
			sink = &gsc->cap.sd_flite[id]->entity;
			if (source && sink) {
				ret = media_entity_create_link(source, 0,
					      sink, FLITE_PAD_SINK,
					      MEDIA_LNK_FL_IMMUTABLE |
					      MEDIA_LNK_FL_ENABLED);
				if (ret) {
					gsc_err("failed link sensor to csis\n");
					return ret;
				}
			}
			/*	FIMC-LITE ------> GSC-CAP	*/
			source = &gsc->cap.sd_flite[id]->entity;
			sink = &gsc->cap.sd_cap->entity;
			if (source && sink) {
				if (pdata->cam_preview)
					ret = media_entity_create_link(source,
							FLITE_PAD_SOURCE_PREVIEW,
							sink, GSC_PAD_SINK,
						      MEDIA_LNK_FL_IMMUTABLE |
						      MEDIA_LNK_FL_ENABLED);
				if (!ret && pdata->cam_camcording)
					ret = media_entity_create_link(source,
							FLITE_PAD_SOURCE_CAMCORDING,
							sink, GSC_PAD_SINK,
						      MEDIA_LNK_FL_IMMUTABLE |
						      MEDIA_LNK_FL_ENABLED);
				if (ret) {
					gsc_err("failed link flite to gsc\n");
					return ret;
				}
			}
			break;
		case GSC_MIPI_CSI2:
			/*	SENSOR ------> MIPI-CSI2	*/
			source = &gsc->cap.sd_sensor[i]->entity;
			sink = &gsc->cap.sd_csis[id]->entity;
			if (source && sink) {
				printk("sensor to mipi\n");
				printk("source : 0x%p\n", source);
				printk("sink : 0x%p\n", sink);
				ret = media_entity_create_link(source, 0,
					      sink, CSIS_PAD_SINK,
					      MEDIA_LNK_FL_IMMUTABLE |
					      MEDIA_LNK_FL_ENABLED);
				if (ret) {
					gsc_err("failed link sensor to csis\n");
					return ret;
				}
			}
			/*	MIPI-CSI2 ------> FIMC-LITE	*/
			source = &gsc->cap.sd_csis[id]->entity;
			sink = &gsc->cap.sd_flite[id]->entity;
			if (source && sink) {
				printk("mipi to flite\n");
				printk("source : 0x%p\n", source);
				printk("sink : 0x%p\n", sink);
				ret = media_entity_create_link(source,
						CSIS_PAD_SOURCE,
						sink, FLITE_PAD_SINK,
					      MEDIA_LNK_FL_IMMUTABLE |
					      MEDIA_LNK_FL_ENABLED);
				if (ret) {
					gsc_err("failed link csis to flite\n");
					return ret;
				}
			}
			/*	FIMC-LITE ------> GSC-CAP	*/
			source = &gsc->cap.sd_flite[id]->entity;
			sink = &gsc->cap.sd_cap->entity;
			if (source && sink) {
				printk("flite to gsc\n");
				printk("source : 0x%p\n", source);
				printk("sink : 0x%p\n", sink);
				if (pdata->cam_preview)
					ret = media_entity_create_link(source,
							FLITE_PAD_SOURCE_PREVIEW,
							sink, GSC_PAD_SINK,
						      MEDIA_LNK_FL_IMMUTABLE |
						      MEDIA_LNK_FL_ENABLED);
				if (!ret && pdata->cam_camcording)
					ret = media_entity_create_link(source,
							FLITE_PAD_SOURCE_CAMCORDING,
							sink, GSC_PAD_SINK,
						      MEDIA_LNK_FL_IMMUTABLE |
						      MEDIA_LNK_FL_ENABLED);
				if (ret) {
					gsc_err("failed link flite to gsc\n");
					return ret;
				}
			}
			break;
		case GSC_LCD_WB:
			break;
		}
	}

	return 0;
}

static int csis_register_callback(struct device *dev, void *p)
{
	struct v4l2_subdev **sd_list = p;
	struct v4l2_subdev *sd = dev_get_drvdata(dev);

	if (sd) {
		struct platform_device *pdev = v4l2_get_subdevdata(sd);
		if (pdev)
			gsc_dbg("pdev->id : %d\n", pdev->id);
		*(sd_list + pdev->id) = sd;
	}

	return 0;
}

static int flite_register_callback(struct device *dev, void *p)
{
	struct v4l2_subdev **sd_list = p;
	struct v4l2_subdev *sd = dev_get_drvdata(dev);

	if (sd) {
		struct platform_device *pdev = v4l2_get_subdevdata(sd);
		if (pdev)
			gsc_dbg("pdev->id : %d\n", pdev->id);
		*(sd_list + pdev->id) = sd;
	}

	return 0;
}

static struct v4l2_subdev *gsc_cap_register_sensor(struct gsc_dev *gsc, int i)
{
	struct exynos_platform_gscaler *pdata = gsc->pdata;
	struct exynos_gscaler_isp_info *isp_info = pdata->isp_info[i];
	struct exynos_md *mdev = gsc->mdev[MDEV_CAPTURE];
	struct i2c_adapter *adapter;
	struct v4l2_subdev *sd = NULL;

	adapter = i2c_get_adapter(isp_info->i2c_bus_num);
	if (!adapter)
		return NULL;
	sd = v4l2_i2c_new_subdev_board(&mdev->v4l2_dev, adapter,
				       isp_info->board_info, NULL);
	if (IS_ERR_OR_NULL(sd)) {
		v4l2_err(&mdev->v4l2_dev, "Failed to acquire subdev\n");
		return NULL;
	}
	v4l2_set_subdev_hostdata(sd, gsc->cap.sd_sensor[i]);
	sd->grp_id = SENSOR_GRP_ID;

	v4l2_info(&mdev->v4l2_dev, "Registered sensor subdevice %s\n",
		  isp_info->board_info->type);

	return sd;
}

static int gsc_cap_register_sensor_entities(struct gsc_dev *gsc)
{
	struct exynos_platform_gscaler *pdata = gsc->pdata;
	u32 num_clients = pdata->num_clients;
	int i;

	for (i = 0; i < num_clients; i++) {
		gsc->cap.sd_sensor[i] = gsc_cap_register_sensor(gsc, i);
		if (IS_ERR_OR_NULL(gsc->cap.sd_sensor[i]))
			return -EINVAL;
	}

	return 0;
}

static int gsc_cap_register_platform_entities(struct gsc_dev *gsc,
		struct exynos_gscaler_isp_info *isp_info)
{
	struct device_driver *driver;
	int ret;
	struct v4l2_subdev *sd[FLITE_MAX_ENTITIES] = {NULL,};
	u32 id = isp_info->mux_id;

	driver = driver_find(FLITE_MODULE_NAME, &platform_bus_type);
	if (!driver)
		return -ENODEV;
	ret = driver_for_each_device(driver, NULL, &sd[0], flite_register_callback);
	put_driver(driver);
	if (ret)
		return ret;

	gsc->cap.sd_flite[id] = sd[id];
	gsc->cap.sd_flite[id]->grp_id = FLITE_GRP_ID;
	printk("sd_flite[%d] : 0x%p\n", id, gsc->cap.sd_flite[id]);

	if (isp_info->bus_type == GSC_MIPI_CSI2) {
		driver = driver_find(CSIS_MODULE_NAME, &platform_bus_type);
		if (!driver)
			return -ENODEV;
		ret = driver_for_each_device(driver, NULL, &sd[0],
					csis_register_callback);
		put_driver(driver);
		if (ret)
			return ret;
		gsc->cap.sd_csis[id] = sd[id];
		gsc->cap.sd_csis[id]->grp_id = CSIS_GRP_ID;
		printk("sd_csis[%d] : 0x%p\n", id, gsc->cap.sd_csis[id]);
	}

	return ret;
}

int gsc_register_capture_device(struct gsc_dev *gsc)
{
	struct video_device *vfd;
	struct gsc_capture_device *gsc_cap;
	struct gsc_ctx *ctx;
	struct vb2_queue *q;
	struct exynos_platform_gscaler *pdata = gsc->pdata;
	struct exynos_gscaler_isp_info *isp_info[GSC_MAX_CAMIF_CLIENTS];
	int ret = -ENOMEM;
	int i;

	ctx = kzalloc(sizeof *ctx, GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->gsc_dev	 = gsc;
	ctx->s_frame.fmt = get_format(2);
	ctx->d_frame.fmt = get_format(10);
	ctx->in_path	 = GSC_CAMERA;
	ctx->out_path	 = GSC_DMA;
	ctx->state	 = GSC_CTX_CAP;

	vfd = video_device_alloc();
	if (!vfd) {
		printk("Failed to allocate video device\n");
		goto err_ctx_alloc;
	}

	snprintf(vfd->name, sizeof(vfd->name), "%s.capture",
		 dev_name(&gsc->pdev->dev));

	vfd->fops	= &gsc_capture_fops;
	vfd->ioctl_ops	= &gsc_capture_ioctl_ops;
	vfd->v4l2_dev	= &gsc->mdev[MDEV_CAPTURE]->v4l2_dev;
	vfd->minor	= -1;
	vfd->release	= video_device_release;
	vfd->lock	= &gsc->lock;
	video_set_drvdata(vfd, gsc);

	gsc_cap	= &gsc->cap;
	gsc_cap->vfd = vfd;
	gsc_cap->refcnt = 0;
	gsc_cap->active_buf_cnt = 0;
	gsc_cap->reqbufs_cnt  = 0;

	spin_lock_init(&ctx->slock);
	gsc_cap->ctx = ctx;

	q = &gsc->cap.vbq;
	memset(q, 0, sizeof(*q));
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->drv_priv = gsc->cap.ctx;
	q->ops = &gsc_capture_qops;
	q->mem_ops = gsc->vb2->ops;;

	vb2_queue_init(q);

	for (i = 0; i < pdata->num_clients; i++) {
		isp_info[i] = pdata->isp_info[i];
		ret = gsc_cap_register_platform_entities(gsc, isp_info[i]);
		if (ret)
			goto err_ctx_alloc;
	}

	if (pdata->cam_preview) {
		ret = gsc_cap_register_sensor_entities(gsc);
		if (ret)
			goto err_ctx_alloc;
	}

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, -1);
	if (ret) {
		printk("%s : Failed to register video device\n", __func__);
		goto err_ent;
	}

	gsc->cap.vd_pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&vfd->entity, 1, &gsc->cap.vd_pad, 0);
	if (ret)
		goto err_ent;

	ret = gsc_capture_create_subdev(gsc);
	if (ret)
		goto err_sd_reg;

	ret = gsc_capture_create_link(gsc);
	if (ret)
		goto err_sd_reg;

	vfd->ctrl_handler = &ctx->ctrl_handler;
//	printk("gsc capture ctx = 0x%08x\n", (u32)ctx);
	printk("gsc capture driver registered as /dev/video%d\n", vfd->num);

	return 0;

err_sd_reg:
	media_entity_cleanup(&vfd->entity);
err_ent:
	video_device_release(vfd);
err_ctx_alloc:
	kfree(ctx);
	printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@[ERROR]@@@@@@@@@@@@@@@@@@@@@@@@@\n");

	return ret;
}

static void gsc_capture_destroy_subdev(struct gsc_dev *gsc)
{
	struct v4l2_subdev *sd = gsc->cap.sd_cap;

	if (!sd)
		return;
	media_entity_cleanup(&sd->entity);
	v4l2_device_unregister_subdev(sd);
	kfree(sd);
	sd = NULL;
}

void gsc_unregister_capture_device(struct gsc_dev *gsc)
{
	struct video_device *vfd = gsc->cap.vfd;

	if (vfd) {
		media_entity_cleanup(&vfd->entity);
		/* Can also be called if video device was
		   not registered */
		video_unregister_device(vfd);
	}
	gsc_capture_destroy_subdev(gsc);
	kfree(gsc->cap.ctx);
	gsc->cap.ctx = NULL;
}
