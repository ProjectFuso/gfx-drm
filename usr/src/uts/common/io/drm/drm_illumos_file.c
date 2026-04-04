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

#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_illumos.h>
#include <drm/drm_vma_manager.h>
#include <drm/ttm/ttm_bo.h>
#include <drm/ttm/ttm_tt.h>

static const struct file_operations drm_illumos_fops_stub = {
	.fop_flags = FOP_UNSIGNED_OFFSET,
};

static int
drm_illumos_minor_slot(dev_t dev)
{
	return (int)(getminor(dev) & 0x3f);
}

static struct drm_illumos_open *
drm_illumos_lookup_open(struct drm_illumos_file_state *state, dev_t dev)
{
	int slot = drm_illumos_minor_slot(dev);

	if (state == NULL || slot < 0 || slot >= DRM_ILUMOS_MAX_OPENS)
		return NULL;
	if (!state->opens[slot].in_use)
		return NULL;

	return &state->opens[slot];
}

int
drm_illumos_open(struct drm_illumos_file_state *state,
    struct drm_device *drm, dev_t *devp)
{
	struct drm_minor *minor;
	struct drm_illumos_open *op;
	int ret, slot;

	if (state == NULL || drm == NULL || devp == NULL)
		return (ENXIO);

	minor = drm->primary;
	if (minor == NULL)
		return (ENXIO);

	for (slot = 0; slot < DRM_ILUMOS_MAX_OPENS; slot++) {
		if (!state->opens[slot].in_use)
			break;
	}
	if (slot >= DRM_ILUMOS_MAX_OPENS)
		return (EBUSY);

	op = &state->opens[slot];
	bzero(&op->filp, sizeof(op->filp));
	op->filp.f_op = &drm_illumos_fops_stub;
	op->minor = minor;

	drm_dev_get(drm);
	atomic_inc(&drm->open_count);

	ret = drm_open_helper(&op->filp, minor);
	if (ret != 0) {
		atomic_dec(&drm->open_count);
		drm_dev_put(drm);
		return (-ret);
	}

	op->in_use = true;
	*devp = makedevice(getmajor(*devp), (minor_t)slot);
	return (0);
}

int
drm_illumos_close(struct drm_illumos_file_state *state, dev_t dev)
{
	struct drm_illumos_open *op;

	op = drm_illumos_lookup_open(state, dev);
	if (op == NULL)
		return (ENXIO);

	drm_release(NULL, &op->filp);
	op->in_use = false;
	return (0);
}

int
drm_illumos_ioctl(struct drm_illumos_file_state *state, dev_t dev,
    int cmd, intptr_t arg)
{
	struct drm_illumos_open *op;
	long ret;

	op = drm_illumos_lookup_open(state, dev);
	if (op == NULL)
		return (ENXIO);

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

	op = drm_illumos_lookup_open(state, dev);
	if (op == NULL)
		return (ENXIO);

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
drm_illumos_gem_ttm_devmap(struct drm_device *drm, devmap_cookie_t dhp,
    offset_t off, size_t len, size_t *maplen)
{
	struct drm_vma_offset_manager *mgr;
	struct drm_vma_offset_node *node;
	struct drm_gem_object *gem;
	struct ttm_buffer_object *bo;
	struct ttm_tt *ttm;
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

	bo = container_of(gem, struct ttm_buffer_object, base);

	if (bo->ttm == NULL || bo->ttm->illumos_umem_cookie == NULL) {
		struct ttm_operation_ctx ctx = {
			.interruptible = false,
			.no_wait_gpu = false,
		};

		ret = ttm_bo_reserve(bo, false, false, NULL);
		if (ret != 0) {
			drm_gem_object_put(gem);
			return (EINVAL);
		}

		if (bo->ttm == NULL) {
			ret = ttm_tt_create(bo, false);
			if (ret != 0) {
				ttm_bo_unreserve(bo);
				drm_gem_object_put(gem);
				return (EINVAL);
			}
		}

		if (!ttm_tt_is_populated(bo->ttm)) {
			ret = ttm_tt_populate(bo->bdev, bo->ttm, &ctx);
			if (ret != 0) {
				ttm_bo_unreserve(bo);
				drm_gem_object_put(gem);
				return (EINVAL);
			}
		}

		ttm_bo_unreserve(bo);
	}

	ttm = bo->ttm;
	if (ttm == NULL || ttm->illumos_umem_cookie == NULL) {
		drm_gem_object_put(gem);
		return (EINVAL);
	}

	node_pgoff = pgoff - drm_vma_node_start(node);
	map_off = node_pgoff << PAGE_SHIFT;

	if (map_off + len > (size_t)gem->size) {
		drm_gem_object_put(gem);
		return (EINVAL);
	}

	ret = devmap_umem_setup(dhp, drm->dev->pdev->dip, NULL,
	    ttm->illumos_umem_cookie, map_off, len,
	    PROT_READ | PROT_WRITE | PROT_USER, DEVMAP_DEFAULTS, NULL);

	drm_gem_object_put(gem);
	if (ret != 0)
		return (ret);

	*maplen = len;
	return (0);
}
