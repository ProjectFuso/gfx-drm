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

/*
 * illumos DMA-BUF vnode operations.
 *
 * This file provides the vnode ops and vnode lifecycle management for
 * dma-buf file descriptors on illumos.
 *
 * NOTE: sys/file.h is intentionally NOT included here to avoid a struct
 * name conflict between the illumos kernel "struct file" (file table entry)
 * and the Linux DRM compat "struct file" (from linux/fs.h).  The kernel
 * file-table operations (falloc/setf/getf) live in drm_illumos_kfile.c.
 */

/* Must come first to block vm/page.h before system headers pull it in */
#include <linux/illumos_page_compat.h>

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/vfs_opreg.h>

#include <linux/dma-buf.h>
#include <drm/drm_gem.h>

static vnodeops_t *dma_buf_vnodeops;

/*
 * dma_buf_vop_getattr — return vnode attributes for a dma-buf fd.
 *
 * Provides the size from the underlying GEM object so that callers
 * (e.g. fstat(2)) get meaningful information.
 */
static int
dma_buf_vop_getattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cr)
{
	struct dma_buf *dmabuf = vp->v_data;
	struct drm_gem_object *obj = dmabuf->priv;

	(void)flags; (void)cr;

	if (obj == NULL)
		return (ENXIO);

	bzero(vap, sizeof (*vap));
	vap->va_type = VREG;
	vap->va_mode = 0666;
	vap->va_size = obj->size;
	vap->va_nodeid = (ino64_t)(uintptr_t)dmabuf;
	return (0);
}

/*
 * dma_buf_vop_inactive — called when the last reference to the vnode is
 * dropped.  Invokes the driver's release hook and frees all allocations.
 */
static void
dma_buf_vop_inactive(vnode_t *vp, cred_t *cr, caller_context_t *ct)
{
	struct dma_buf *dmabuf = vp->v_data;

	(void)cr; (void)ct;

	if (dmabuf->ops->release)
		dmabuf->ops->release(dmabuf);

	if (dmabuf->file)
		kmem_free(dmabuf->file, sizeof (struct file));

	kmem_free(dmabuf, sizeof (struct dma_buf));
	vn_free(vp);
}

/*
 * NOTE: dma_buf_vop_map / dma_buf_vop_devmap are intentionally absent.
 * Direct mmap() on a dma-buf fd is not needed for the DRI3 handle-passing
 * flow (export handle→fd, pass fd, import fd→handle, mmap via DRM device).
 */
static const fs_operation_def_t dma_buf_vnodeops_template[] = {
	{ VOPNAME_GETATTR,	{ .vop_getattr = dma_buf_vop_getattr } },
	{ VOPNAME_INACTIVE,	{ .vop_inactive = dma_buf_vop_inactive } },
	{ NULL,			{ NULL } }
};

/*
 * drm_illumos_dmabuf_alloc — allocate and set up the illumos vnode backing
 * the given dma_buf.  Stores the vnode in dmabuf->illumos_vnode.
 *
 * Returns 0 on success, -ENOMEM on failure.
 */
int
drm_illumos_dmabuf_alloc(struct dma_buf *dmabuf)
{
	vnode_t *vp;

	if (dma_buf_vnodeops == NULL) {
		if (vn_make_ops("dma_buf", dma_buf_vnodeops_template,
		    &dma_buf_vnodeops) != 0)
			return (-ENOMEM);
	}

	vp = vn_alloc(KM_SLEEP);
	vn_setops(vp, dma_buf_vnodeops);
	vp->v_data = dmabuf;
	vp->v_type = VREG;
	dmabuf->illumos_vnode = vp;
	return (0);
}

/*
 * drm_vnode_is_dmabuf — return non-zero if vp is a dma-buf vnode.
 * Used by drm_illumos_kfile.c to validate fd lookups.
 */
int
drm_vnode_is_dmabuf(struct vnode *vp)
{
	return (vp != NULL && dma_buf_vnodeops != NULL &&
	    vn_getops(vp) == dma_buf_vnodeops);
}
