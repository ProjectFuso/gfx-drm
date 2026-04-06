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
 * illumos DMA-BUF kernel file-table helpers.
 *
 * This file provides fd ↔ dma-buf vnode translation using the illumos
 * kernel file table (falloc/setf/getf).
 *
 * NOTE: No Linux DRM compat headers (linux/XXX.h) are included here.
 * This avoids the struct file name conflict between the illumos kernel
 * "struct file" (file table entry, defined in sys/file.h) and the Linux
 * DRM compat "struct file" (from linux/fs.h).
 */

/* Must come first to block vm/page.h before system headers pull it in */
#include <linux/illumos_page_compat.h>

#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/file.h>

/*
 * drm_vnode_is_dmabuf — provided by drm_illumos_dmabuf.c.
 * Validates that a vnode is backed by the dma-buf vnodeops.
 */
extern int drm_vnode_is_dmabuf(struct vnode *vp);

/*
 * drm_fd_to_dmabuf — look up an fd and return the dma-buf pointer if the
 * fd is backed by a dma-buf vnode.
 *
 * On success returns the struct dma_buf * (as void *) with a VN_HOLD added;
 * the caller must eventually call dma_buf_put() (which does VN_RELE).
 * Returns NULL on failure (fd invalid, or not a dma-buf fd).
 */
void *
drm_fd_to_dmabuf(int fd)
{
	file_t *fp;
	vnode_t *vp;
	void *dmabuf;

	if ((fp = getf(fd)) == NULL)
		return (NULL);

	vp = fp->f_vnode;
	if (vp == NULL || !drm_vnode_is_dmabuf(vp)) {
		releasef(fd);
		return (NULL);
	}

	dmabuf = vp->v_data;
	VN_HOLD(vp);
	releasef(fd);
	return (dmabuf);
}

/*
 * drm_vnode_to_fd — install a dma-buf vnode into the fd table.
 *
 * Calls falloc(vp, …) which automatically sets f_vnode, f_ops, f_flag,
 * then calls setf() to make the fd visible.
 *
 * Returns the new fd number on success, or -EMFILE on failure.
 */
int
drm_vnode_to_fd(void *vp_opaque)
{
	vnode_t *vp = (vnode_t *)vp_opaque;
	file_t *fp;
	int fd;

	if (falloc(vp, FREAD | FWRITE, &fp, &fd) != 0)
		return (-EMFILE);

	setf(fd, fp);
	return (fd);
}
