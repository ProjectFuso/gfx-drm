/*
 * Copyright (c) 2024, illumos contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/illumos_page_compat.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/sunddi.h>
#include <sys/visual_io.h>

#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_illumos.h>
#include <drm/drm_print.h>
#include <drm/drm_vma_manager.h>
#include <drm/ttm/ttm_bo.h>
#include <drm/ttm/ttm_tt.h>

static const struct file_operations drm_illumos_fops_stub = {
	.fop_flags = FOP_UNSIGNED_OFFSET,
};

static const struct vis_identifier drm_illumos_vis_ident = {
	"ILLUMOSdrmfb"
};

struct drm_illumos_vis_layout {
	uint8_t *fb;
	size_t fb_size;
	size_t line_length;
	uint32_t width;
	uint32_t height;
	uint32_t bpp;
};

static uint32_t
drm_illumos_vis_color_map(uint8_t color)
{
	static const uint32_t ansi_colors[16] = {
		0x00000000,
		0x00AA0000,
		0x0000AA00,
		0x00AA5500,
		0x000000AA,
		0x00AA00AA,
		0x0000AAAA,
		0x00AAAAAA,
		0x00555555,
		0x00FF5555,
		0x0055FF55,
		0x00FFFF55,
		0x005555FF,
		0x00FF55FF,
		0x0055FFFF,
		0x00FFFFFF,
	};

	return ansi_colors[color & 0xf];
}

static struct drm_fb_helper *
drm_illumos_vis_fb_helper(struct drm_illumos_open *op)
{
	struct drm_device *dev;

	if (op == NULL || op->minor == NULL || op->minor->type != DRM_MINOR_PRIMARY)
		return (NULL);

	dev = op->minor->dev;
	if (dev == NULL || dev->fb_helper == NULL || dev->fb_helper->info == NULL)
		return (NULL);

	return (dev->fb_helper);
}

static bool
drm_illumos_vis_get_layout(struct drm_fb_helper *fb_helper, const char *op,
    struct drm_illumos_vis_layout *layout)
{
	struct fb_info *info;
	size_t fb_size;
	uint32_t width, height, bpp;
	size_t line_length;

	if (fb_helper == NULL || fb_helper->info == NULL)
		return (false);

	info = fb_helper->info;
	line_length = info->fix.line_length;
	width = info->var.xres_virtual ? info->var.xres_virtual : info->var.xres;
	height = info->var.yres_virtual ? info->var.yres_virtual : info->var.yres;
	bpp = info->var.bits_per_pixel;
	fb_size = info->screen_size ? info->screen_size : info->fix.smem_len;

	if (fb_size == 0 && line_length != 0 && height != 0)
		fb_size = line_length * height;

	if (info->screen_buffer == NULL || line_length == 0 ||
	    width == 0 || height == 0 || fb_size == 0) {
		drm_err_once(fb_helper->dev,
		    "vis %s: invalid fb layout buf=%p size=%zu stride=%zu %ux%u bpp=%u\n",
		    op, info->screen_buffer, fb_size, line_length, width,
		    height, bpp);
		return (false);
	}

	if (bpp != 32) {
		drm_err_once(fb_helper->dev,
		    "vis %s: unsupported bpp=%u stride=%zu %ux%u\n", op,
		    bpp, line_length, width, height);
		return (false);
	}

	layout->fb = (uint8_t *)info->screen_buffer;
	layout->fb_size = fb_size;
	layout->line_length = line_length;
	layout->width = width;
	layout->height = height;
	layout->bpp = bpp;

	return (true);
}

static bool
drm_illumos_vis_rect_valid(struct drm_fb_helper *fb_helper, const char *op,
    struct drm_illumos_vis_layout *layout, int row, int col, int width, int height)
{
	size_t col_off;
	size_t width_bytes;
	size_t last_off;

	if (row < 0 || col < 0 || width <= 0 || height <= 0) {
		drm_err_once(fb_helper->dev,
		    "vis %s: invalid rect row=%d col=%d w=%d h=%d\n",
		    op, row, col, width, height);
		return (false);
	}

	if ((uint32_t)row + (uint32_t)height > layout->height ||
	    (uint32_t)col + (uint32_t)width > layout->width) {
		drm_err_once(fb_helper->dev,
		    "vis %s: rect row=%d col=%d w=%d h=%d exceeds %ux%u\n",
		    op, row, col, width, height, layout->width,
		    layout->height);
		return (false);
	}

	col_off = (size_t)col * 4;
	width_bytes = (size_t)width * 4;
	if (col_off + width_bytes > layout->line_length) {
		drm_err_once(fb_helper->dev,
		    "vis %s: rect row=%d col=%d w=%d h=%d exceeds stride=%zu\n",
		    op, row, col, width, height, layout->line_length);
		return (false);
	}

	last_off = ((size_t)(row + height - 1) * layout->line_length) + col_off;
	if (last_off > layout->fb_size || width_bytes > layout->fb_size - last_off) {
		drm_err_once(fb_helper->dev,
		    "vis %s: rect row=%d col=%d w=%d h=%d exceeds fb_size=%zu\n",
		    op, row, col, width, height, layout->fb_size);
		return (false);
	}

	return (true);
}

static bool
drm_illumos_vis_clip_rect(struct drm_fb_helper *fb_helper, const char *op,
    struct drm_illumos_vis_layout *layout, int *row, int *col, int *width,
    int *height)
{
	uint32_t max_width, max_height;

	if (*row < 0 || *col < 0 || *width <= 0 || *height <= 0) {
		drm_err_once(fb_helper->dev,
		    "vis %s: invalid rect row=%d col=%d w=%d h=%d\n",
		    op, *row, *col, *width, *height);
		return (false);
	}

	if ((uint32_t)*row >= layout->height || (uint32_t)*col >= layout->width) {
		drm_err_once(fb_helper->dev,
		    "vis %s: offscreen rect row=%d col=%d w=%d h=%d outside %ux%u\n",
		    op, *row, *col, *width, *height, layout->width,
		    layout->height);
		return (false);
	}

	max_width = layout->width - (uint32_t)*col;
	max_height = layout->height - (uint32_t)*row;
	if ((uint32_t)*width > max_width || (uint32_t)*height > max_height) {
		drm_warn_once(fb_helper->dev,
		    "vis %s: clipping rect row=%d col=%d w=%d h=%d to %ux%u\n",
		    op, *row, *col, *width, *height, max_width, max_height);
		if ((uint32_t)*width > max_width)
			*width = (int)max_width;
		if ((uint32_t)*height > max_height)
			*height = (int)max_height;
	}

	return drm_illumos_vis_rect_valid(fb_helper, op, layout, *row, *col,
	    *width, *height);
}

static void
drm_illumos_vis_damage(struct drm_fb_helper *fb_helper, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
	if (fb_helper == NULL || fb_helper->info == NULL)
		return;

	drm_fb_helper_damage_area(fb_helper->info, x, y, width, height);
}

static void
drm_illumos_vis_damage_clipped(struct drm_fb_helper *fb_helper, int row, int col,
    int width, int height)
{
	struct drm_illumos_vis_layout layout;

	if (!drm_illumos_vis_get_layout(fb_helper, "damage", &layout))
		return;
	if (!drm_illumos_vis_clip_rect(fb_helper, "damage", &layout, &row, &col,
	    &width, &height))
		return;

	drm_illumos_vis_damage(fb_helper, (uint32_t)col, (uint32_t)row,
	    (uint32_t)width, (uint32_t)height);
}

static bool
drm_illumos_vis_display_rect(struct drm_fb_helper *fb_helper,
    struct vis_consdisplay *dp)
{
	struct drm_illumos_vis_layout layout;
	int row, col, width, height;
	uint32_t y;
	size_t width_bytes;

	if (dp->data == NULL) {
		drm_err_once(fb_helper->dev,
		    "vis display: NULL source pointer\n");
		return (false);
	}

	if (!drm_illumos_vis_get_layout(fb_helper, "display", &layout))
		return (false);

	row = dp->row;
	col = dp->col;
	width = dp->width;
	height = dp->height;
	if (!drm_illumos_vis_clip_rect(fb_helper, "display", &layout, &row, &col,
	    &width, &height))
		return (false);

	width_bytes = (size_t)width * 4;

	for (y = 0; y < (uint32_t)height; y++) {
		uint8_t *dst = layout.fb +
		    ((uint32_t)row + y) * layout.line_length +
		    (uint32_t)col * 4;
		uint8_t *src = dp->data + y * (uint32_t)dp->width * 4;

		bcopy(src, dst, width_bytes);
	}

	return (true);
}

static bool
drm_illumos_vis_copy_rect(struct drm_fb_helper *fb_helper, struct vis_conscopy *cp)
{
	struct drm_illumos_vis_layout layout;
	uint8_t *fb;
	uint32_t height;
	uint32_t width;
	uint32_t width_bytes;
	int32_t i;

	if (!drm_illumos_vis_get_layout(fb_helper, "copy", &layout))
		return (false);
	if (cp->e_row < cp->s_row || cp->e_col < cp->s_col) {
		drm_err_once(fb_helper->dev,
		    "vis copy: invalid source (%d,%d)-(%d,%d)\n",
		    cp->s_row, cp->s_col, cp->e_row, cp->e_col);
		return (false);
	}

	width = (uint32_t)(cp->e_col - cp->s_col + 1);
	height = (uint32_t)(cp->e_row - cp->s_row + 1);
	if (!drm_illumos_vis_rect_valid(fb_helper, "copy-src", &layout,
	    cp->s_row, cp->s_col, width, height))
		return (false);
	if (!drm_illumos_vis_rect_valid(fb_helper, "copy-dst", &layout,
	    cp->t_row, cp->t_col, width, height))
		return (false);

	fb = layout.fb;
	width_bytes = width * 4;

	if (cp->t_row <= cp->s_row) {
		for (i = 0; i < (int32_t)height; i++) {
			uint8_t *src = fb +
			    ((uint32_t)cp->s_row + i) * layout.line_length +
			    (uint32_t)cp->s_col * 4;
			uint8_t *dst = fb +
			    ((uint32_t)cp->t_row + i) * layout.line_length +
			    (uint32_t)cp->t_col * 4;

			ovbcopy(src, dst, width_bytes);
		}
	} else {
		for (i = (int32_t)height - 1; i >= 0; i--) {
			uint8_t *src = fb +
			    ((uint32_t)cp->s_row + i) * layout.line_length +
			    (uint32_t)cp->s_col * 4;
			uint8_t *dst = fb +
			    ((uint32_t)cp->t_row + i) * layout.line_length +
			    (uint32_t)cp->t_col * 4;

			ovbcopy(src, dst, width_bytes);
		}
	}

	return (true);
}

static bool
drm_illumos_vis_cursor_rect(struct drm_fb_helper *fb_helper,
    struct vis_conscursor *cur)
{
	struct drm_illumos_vis_layout layout;
	int row, col, width, height;
	uint32_t x, y;

	if (cur->action == VIS_GET_CURSOR)
		return (false);

	if (!drm_illumos_vis_get_layout(fb_helper, "cursor", &layout))
		return (false);

	row = cur->row;
	col = cur->col;
	width = cur->width;
	height = cur->height;
	if (!drm_illumos_vis_clip_rect(fb_helper, "cursor", &layout, &row, &col,
	    &width, &height))
		return (false);

	for (y = 0; y < (uint32_t)height; y++) {
		uint32_t *pixrow = (uint32_t *)(layout.fb +
		    ((uint32_t)row + y) * layout.line_length +
		    (uint32_t)col * 4);

		for (x = 0; x < (uint32_t)width; x++)
			pixrow[x] ^= 0x00FFFFFF;
	}

	return (true);
}

static void
drm_illumos_vis_clear_rect(struct drm_fb_helper *fb_helper, struct vis_consclear *clp)
{
	struct drm_illumos_vis_layout layout;
	uint32_t *pixels;
	uint32_t npixels, i;
	uint32_t color;

	if (!drm_illumos_vis_get_layout(fb_helper, "clear", &layout))
		return;

	color = drm_illumos_vis_color_map(clp->bg_color.eight);
	pixels = (uint32_t *)layout.fb;
	npixels = layout.fb_size / sizeof (uint32_t);

	for (i = 0; i < npixels; i++)
		pixels[i] = color;
}

static void
drm_illumos_vis_polled_display(struct vis_polledio_arg *arg,
    struct vis_consdisplay *dp)
{
	struct drm_illumos_open *op = (struct drm_illumos_open *)arg;
	struct drm_fb_helper *fb_helper = drm_illumos_vis_fb_helper(op);

	if (fb_helper == NULL)
		return;

	if (drm_illumos_vis_display_rect(fb_helper, dp))
		drm_illumos_vis_damage_clipped(fb_helper, dp->row, dp->col,
		    dp->width, dp->height);
}

static void
drm_illumos_vis_polled_copy(struct vis_polledio_arg *arg, struct vis_conscopy *cp)
{
	struct drm_illumos_open *op = (struct drm_illumos_open *)arg;
	struct drm_fb_helper *fb_helper = drm_illumos_vis_fb_helper(op);

	if (fb_helper == NULL)
		return;

	if (drm_illumos_vis_copy_rect(fb_helper, cp))
		drm_illumos_vis_damage_clipped(fb_helper, cp->t_row, cp->t_col,
		    cp->e_col - cp->s_col + 1, cp->e_row - cp->s_row + 1);
}

static void
drm_illumos_vis_polled_cursor(struct vis_polledio_arg *arg,
    struct vis_conscursor *cur)
{
	struct drm_illumos_open *op = (struct drm_illumos_open *)arg;
	struct drm_fb_helper *fb_helper = drm_illumos_vis_fb_helper(op);

	if (fb_helper == NULL)
		return;

	if (drm_illumos_vis_cursor_rect(fb_helper, cur))
		drm_illumos_vis_damage_clipped(fb_helper, cur->row, cur->col,
		    cur->width, cur->height);
}

static int
drm_illumos_vis_ioctl(struct drm_illumos_open *op, int cmd, intptr_t arg, int mode)
{
	struct drm_fb_helper *fb_helper = drm_illumos_vis_fb_helper(op);
	struct fb_info *info;

	if (fb_helper == NULL)
		return (ENOTTY);

	info = fb_helper->info;

	switch (cmd) {
	case VIS_GETIDENTIFIER:
		if (ddi_copyout(&drm_illumos_vis_ident, (void *)arg,
		    sizeof (drm_illumos_vis_ident), mode) != 0)
			return (EFAULT);
		return (0);

	case VIS_DEVINIT: {
		struct vis_devinit init;

		if (ddi_copyin((void *)arg, &init, sizeof (init), mode) != 0)
			return (EFAULT);

		op->vis_polledio.arg = (struct vis_polledio_arg *)op;
		op->vis_polledio.display = drm_illumos_vis_polled_display;
		op->vis_polledio.copy = drm_illumos_vis_polled_copy;
		op->vis_polledio.cursor = drm_illumos_vis_polled_cursor;

		init.version = VIS_CONS_REV;
		init.width = (screen_size_t)info->var.xres;
		init.height = (screen_size_t)info->var.yres;
		init.linebytes = (screen_size_t)info->fix.line_length;
		init.depth = (screen_size_t)info->var.bits_per_pixel;
		init.mode = VIS_PIXEL;
		init.color_map = drm_illumos_vis_color_map;
		init.polledio = &op->vis_polledio;
		drm_info_once(fb_helper->dev,
		    "vis DEVINIT: %ux%u stride=%u bpp=%u screen=%zu shadow=%p\n",
		    info->var.xres, info->var.yres, info->fix.line_length,
		    info->var.bits_per_pixel,
		    info->screen_size ? info->screen_size : info->fix.smem_len,
		    info->screen_buffer);

		if (ddi_copyout(&init, (void *)arg, sizeof (init), mode) != 0)
			return (EFAULT);
		return (0);
	}

	case VIS_DEVFINI:
		return (0);

	case VIS_CONSDISPLAY: {
		struct vis_consdisplay disp;

		if (ddi_copyin((void *)arg, &disp, sizeof (disp), mode) != 0)
			return (EFAULT);

		if (drm_illumos_vis_display_rect(fb_helper, &disp))
			drm_illumos_vis_damage_clipped(fb_helper, disp.row,
			    disp.col, disp.width, disp.height);
		return (0);
	}

	case VIS_CONSCOPY: {
		struct vis_conscopy cp;

		if (ddi_copyin((void *)arg, &cp, sizeof (cp), mode) != 0)
			return (EFAULT);

		if (drm_illumos_vis_copy_rect(fb_helper, &cp))
			drm_illumos_vis_damage_clipped(fb_helper, cp.t_row,
			    cp.t_col, cp.e_col - cp.s_col + 1,
			    cp.e_row - cp.s_row + 1);
		return (0);
	}

	case VIS_CONSCURSOR: {
		struct vis_conscursor cur;

		if (ddi_copyin((void *)arg, &cur, sizeof (cur), mode) != 0)
			return (EFAULT);

		if (cur.action != VIS_GET_CURSOR &&
		    drm_illumos_vis_cursor_rect(fb_helper, &cur))
			drm_illumos_vis_damage_clipped(fb_helper, cur.row,
			    cur.col, cur.width, cur.height);
		if (cur.action == VIS_GET_CURSOR) {
			cur.row = 0;
			cur.col = 0;
			if (ddi_copyout(&cur, (void *)arg, sizeof (cur), mode) != 0)
				return (EFAULT);
		}
		return (0);
	}

	case VIS_CONSCLEAR: {
		struct vis_consclear clr;

		if (ddi_copyin((void *)arg, &clr, sizeof (clr), mode) != 0)
			return (EFAULT);

		drm_illumos_vis_clear_rect(fb_helper, &clr);
		drm_illumos_vis_damage_clipped(fb_helper, 0, 0, info->var.xres,
		    info->var.yres);
		return (0);
	}

	default:
		return (ENOTTY);
	}
}

static struct drm_illumos_open *
drm_illumos_lookup_open(struct drm_illumos_file_state *state, dev_t dev)
{
	int slot = drm_illumos_decode_slot(getminor(dev));

	ASSERT(MUTEX_HELD(&state->mutex));

	if (state == NULL || slot < 0 || slot >= DRM_ILUMOS_MAX_OPENS)
		return NULL;
	if (!state->opens[slot].in_use)
		return NULL;

	return &state->opens[slot];
}

void
drm_illumos_file_state_init(struct drm_illumos_file_state *state)
{
	bzero(state, sizeof(*state));
	mutex_init(&state->mutex, NULL, MUTEX_DRIVER, NULL);
}

void
drm_illumos_file_state_destroy(struct drm_illumos_file_state *state)
{
	int i;

	mutex_enter(&state->mutex);
	for (i = 0; i < DRM_ILUMOS_MAX_OPENS; i++) {
		if (state->opens[i].in_use) {
			drm_release(NULL, &state->opens[i].filp);
			state->opens[i].in_use = false;
		}
	}
	mutex_exit(&state->mutex);
	mutex_destroy(&state->mutex);
}

int
drm_illumos_open(struct drm_illumos_file_state *state,
    struct drm_device *drm, int instance, int kind, dev_t *devp)
{
	struct drm_minor *minor;
	struct drm_illumos_open *op;
	int ret, slot;

	if (state == NULL || drm == NULL || devp == NULL)
		return (ENXIO);

	if (kind == DRM_ILLUMOS_KIND_PRIMARY)
		minor = drm->primary;
	else if (kind == DRM_ILLUMOS_KIND_RENDER)
		minor = drm->render;
	else
		return (EINVAL);

	if (minor == NULL)
		return (ENXIO);

	mutex_enter(&state->mutex);
	for (slot = 0; slot < DRM_ILUMOS_MAX_OPENS; slot++) {
		if (!state->opens[slot].in_use)
			break;
	}
	if (slot >= DRM_ILUMOS_MAX_OPENS) {
		mutex_exit(&state->mutex);
		return (EBUSY);
	}

	op = &state->opens[slot];
	op->in_use = true;
	mutex_exit(&state->mutex);

	bzero(&op->filp, sizeof(op->filp));
	op->filp.f_op = &drm_illumos_fops_stub;
	op->minor = minor;
	bzero(&op->vis_polledio, sizeof (op->vis_polledio));

	drm_dev_get(drm);
	atomic_inc(&drm->open_count);

	ret = drm_open_helper(&op->filp, minor);
	if (ret != 0) {
		atomic_dec(&drm->open_count);
		drm_dev_put(drm);
		mutex_enter(&state->mutex);
		op->in_use = false;
		mutex_exit(&state->mutex);
		return (-ret);
	}

	*devp = makedevice(getmajor(*devp),
	    drm_illumos_encode_minor(instance, kind, slot));
	return (0);
}

int
drm_illumos_close(struct drm_illumos_file_state *state, dev_t dev)
{
	struct drm_illumos_open *op;

	mutex_enter(&state->mutex);
	op = drm_illumos_lookup_open(state, dev);
	if (op == NULL) {
		mutex_exit(&state->mutex);
		return (ENXIO);
	}

	drm_release(NULL, &op->filp);
	op->in_use = false;
	mutex_exit(&state->mutex);
	return (0);
}

int
drm_illumos_ioctl(struct drm_illumos_file_state *state, dev_t dev,
    int cmd, intptr_t arg, int mode)
{
	struct drm_illumos_open *op;
	long ret;
	int vis_ret;

	mutex_enter(&state->mutex);
	op = drm_illumos_lookup_open(state, dev);
	if (op == NULL) {
		mutex_exit(&state->mutex);
		return (ENXIO);
	}
	mutex_exit(&state->mutex);

	vis_ret = drm_illumos_vis_ioctl(op, cmd, arg, mode);
	if (vis_ret != ENOTTY)
		return (vis_ret);

	ret = drm_ioctl(&op->filp, (unsigned int)cmd, (unsigned long)arg);
	return (ret < 0 ? (int)-ret : 0);
}

int
drm_illumos_chpoll(struct drm_illumos_file_state *state, dev_t dev,
    short events, int anyyet, short *reventsp, struct pollhead **phpp)
{
	struct drm_illumos_open *op;
	struct drm_file *file_priv;
	short revents = 0;

	mutex_enter(&state->mutex);
	op = drm_illumos_lookup_open(state, dev);
	if (op == NULL) {
		mutex_exit(&state->mutex);
		return (ENXIO);
	}
	mutex_exit(&state->mutex);

	file_priv = op->filp.private_data;
	if (file_priv == NULL)
		return (EBADF);

	if (!anyyet)
		*phpp = &file_priv->drm_pollhead;

	if (events & (POLLIN | POLLRDNORM)) {
		mutex_enter(&file_priv->event_read_lock);
		if (!list_empty(&file_priv->event_list))
			revents |= events & (POLLIN | POLLRDNORM);
		mutex_exit(&file_priv->event_read_lock);
	}

	*reventsp = revents;
	return (0);
}

int
drm_illumos_gem_mmap_obj(struct drm_gem_object *gem, devmap_cookie_t dhp,
    offset_t off, size_t len, size_t *maplen)
{
	struct ttm_buffer_object *bo;
	struct ttm_tt *ttm;
	int ret;

	bo = container_of(gem, struct ttm_buffer_object, base);

	if (bo->ttm == NULL || bo->ttm->illumos_umem_cookie == NULL) {
		struct ttm_operation_ctx ctx = {
			.interruptible = false,
			.no_wait_gpu = false,
		};

		ret = ttm_bo_reserve(bo, false, false, NULL);
		if (ret != 0) {
			return (EINVAL);
		}

		if (bo->ttm == NULL) {
			ret = ttm_tt_create(bo, false);
			if (ret != 0) {
				ttm_bo_unreserve(bo);
				return (EINVAL);
			}
		}

		if (!ttm_tt_is_populated(bo->ttm)) {
			ret = ttm_tt_populate(bo->bdev, bo->ttm, &ctx);
			if (ret != 0) {
				ttm_bo_unreserve(bo);
				return (EINVAL);
			}
		}

		ttm_bo_unreserve(bo);
	}

	ttm = bo->ttm;
	if (ttm == NULL || ttm->illumos_umem_cookie == NULL) {
		return (EINVAL);
	}

	if ((size_t)off + len > (size_t)gem->size) {
		return (EINVAL);
	}

	ret = devmap_umem_setup(dhp, gem->dev->dev->dip, NULL,
	    ttm->illumos_umem_cookie, (size_t)off, len,
	    PROT_READ | PROT_WRITE | PROT_USER, DEVMAP_DEFAULTS, NULL);

	if (ret != 0)
		return (ret);

	*maplen = len;
	return (0);
}

int
drm_illumos_gem_ttm_devmap(struct drm_device *drm, devmap_cookie_t dhp,
    offset_t off, size_t len, size_t *maplen)
{
	struct drm_vma_offset_manager *mgr;
	struct drm_vma_offset_node *node;
	struct drm_gem_object *gem;
	unsigned long pgoff;
	unsigned long npages;
	unsigned long node_pgoff;
	size_t map_off;
	int ret;

	if (drm == NULL || drm->vma_offset_manager == NULL)
		return (ENXIO);

	mgr = drm->vma_offset_manager;
	pgoff = (unsigned long)(off >> PAGE_SHIFT);
	npages = (unsigned long)((len + PAGE_SIZE - 1) >> PAGE_SHIFT);

	drm_vma_offset_lock_lookup(mgr);
	node = drm_vma_offset_lookup_locked(mgr, pgoff, npages);
	if (node == NULL) {
		drm_vma_offset_unlock_lookup(mgr);
		return (EINVAL);
	}
	gem = container_of(node, struct drm_gem_object, vma_node);
	drm_gem_object_get(gem);
	drm_vma_offset_unlock_lookup(mgr);

	node_pgoff = pgoff - drm_vma_node_start(node);
	map_off = node_pgoff << PAGE_SHIFT;

	ret = drm_illumos_gem_mmap_obj(gem, dhp, (offset_t)map_off, len, maplen);

	drm_gem_object_put(gem);
	return (ret);
}
