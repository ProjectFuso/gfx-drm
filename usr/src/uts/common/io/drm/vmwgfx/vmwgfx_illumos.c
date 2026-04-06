/*
 * Copyright (c) 2024, illumos vmwgfx port
 * SPDX-License-Identifier: GPL-2.0 OR MIT
 *
 * illumos DDI glue for vmwgfx DRM driver.
 * Bridges between illumos DDI and the Linux pci_driver model.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <linux/illumos_page_compat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/pci.h>

#include <sys/stat.h>		/* S_IFCHR for ddi_create_minor_node */
#include <sys/visual_io.h>	/* VIS_* ioctl interface for TEM */
/* devmap_umem_setup and devmap_cookie_t are declared in sys/sunddi.h above */

#include <linux/pci.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_device.h>
#include <drm/drm_illumos.h>

/* Forward declarations from vmwgfx_drv.c */
extern struct pci_driver vmw_pci_driver;

/*
 * Include vmwgfx private header for struct vmw_private.
 * We need access to initial_width, initial_height, vram_start, vram_size.
 */
#include "vmwgfx_drv.h"
#include <drm/drm_vma_manager.h>
#include <drm/ttm/ttm_bo.h>
#include <drm/ttm/ttm_tt.h>

/*
 * vmw_irq_handler and vmw_thread_fn are static in vmwgfx_irq.c.
 * Declare them extern here so illumos_vmw_irq_install can store them as
 * function pointers.  The linker resolves these within the vmwgfx module.
 */
extern irqreturn_t vmw_irq_handler(int irq, void *arg);
extern irqreturn_t vmw_thread_fn(int irq, void *arg);

/*
 * Global state pointer — there is at most one vmwgfx instance per system.
 * Set during attach, cleared during detach.
 */
static struct vmwgfx_state *vmwgfx_global_state;

/* VIS identifier string returned to the terminal emulator */
static const struct vis_identifier vmwgfx_vis_ident = {
	"ILLUMOSvmwgfx"
};

 /* Per-instance state */
 struct vmwgfx_state {

	struct pci_dev		pci_dev;	/* Linux compat PCI device */
	struct drm_illumos_file_state files;

	/* VIS framebuffer console state */
	caddr_t			vram_va;	/* kernel VA of VRAM (BAR1) */
	ddi_acc_handle_t	vram_handle;	/* DDI acc handle for VRAM */
	uint32_t		fb_width;	/* display width in pixels */
	uint32_t		fb_height;	/* display height in pixels */
	uint32_t		fb_stride;	/* bytes per scan line */
	struct vis_polledio	vis_polledio;	/* polled I/O callbacks */

	/* IRQ state */
	struct drm_illumos_irq_state irq;
};

/*
 * vmwgfx_vram_kva — Return the kernel VA for a byte offset into VRAM.
 *
 * Called by vmw_ttm_io_mem_reserve to populate mem->bus.addr so that
 * ttm_kmap_iter_linear_io_init can copy to/from VRAM without needing
 * Linux's ioremap (which is inside a #ifdef __linux__ block).
 *
 * Returns NULL if VRAM is not yet mapped (attach not complete).
 */
caddr_t
vmwgfx_vram_kva(size_t byte_offset)
{
	struct vmwgfx_state *state = vmwgfx_global_state;
	if (state == NULL || state->vram_va == NULL)
		return NULL;
	return state->vram_va + byte_offset;
}

/* ------------------------------------------------------------------ */
/* IRQ implementation                                                  */
/* ------------------------------------------------------------------ */

/*
 * illumos_vmw_irq_install — register a DDI fixed interrupt for vmwgfx.
 *
 * Called from vmw_irq_install in vmwgfx_irq.c (illumos path).
 * On success, sets dev_priv->irqs[0] = 0 and num_irq_vectors = 1.
 */
int
illumos_vmw_irq_install(struct vmw_private *dev_priv)
{
	struct pci_dev *pdev = to_pci_dev(dev_priv->drm.dev);
	struct vmwgfx_state *state;
	struct drm_illumos_irq_vector vectors[VMWGFX_MAX_NUM_IRQS];
	uint_t actual = 0;
	int ret, i;

	state = ddi_get_driver_private(pdev->dip);
	if (state == NULL)
		return (-ENODEV);

	for (i = 0; i < VMWGFX_MAX_NUM_IRQS; i++) {
		vectors[i].handler = vmw_irq_handler;
		vectors[i].thread_fn = vmw_thread_fn;
		vectors[i].dev_id = &dev_priv->drm;
		vectors[i].name = "vmwgfx_irqthr";
	}

	ret = drm_illumos_irq_install_multivector(pdev->dip, &state->irq,
	    "vmwgfx_irqthr", VMWGFX_MAX_NUM_IRQS, &actual, vectors);
	if (ret != 0)
		return (-ret);

	for (i = 0; i < (int)actual; i++) {
		dev_priv->irqs[i] = i;
	}
	dev_priv->num_irq_vectors = actual;
	return (0);
}


/*
 * illumos_vmw_irq_uninstall — tear down the DDI interrupt.
 * Called from vmw_irq_uninstall in vmwgfx_irq.c (illumos path).
 */
void
illumos_vmw_irq_uninstall(struct vmw_private *dev_priv)
{
	struct pci_dev *pdev = to_pci_dev(dev_priv->drm.dev);
	struct vmwgfx_state *state;

	state = ddi_get_driver_private(pdev->dip);
	if (state == NULL || !state->irq.registered)
		return;

	drm_illumos_irq_uninstall(&state->irq);
	dev_priv->num_irq_vectors = 0;
}

/* ------------------------------------------------------------------ */
/* VIS framebuffer console implementation                              */
/* ------------------------------------------------------------------ */

/*
 * Color map: 4-bit ANSI color index → 32-bit XRGB pixel.
 * The TEM passes 8-bit indices; we only need the low 4 bits for ANSI colors.
 */
static uint32_t
vmwgfx_color_map(uint8_t color)
{
	static const uint32_t ansi_colors[16] = {
		0x00000000,	/* 0 black   */
		0x00AA0000,	/* 1 red     */
		0x0000AA00,	/* 2 green   */
		0x00AA5500,	/* 3 brown   */
		0x000000AA,	/* 4 blue    */
		0x00AA00AA,	/* 5 magenta */
		0x0000AAAA,	/* 6 cyan    */
		0x00AAAAAA,	/* 7 white   */
		0x00555555,	/* 8 bright black */
		0x00FF5555,	/* 9 bright red   */
		0x0055FF55,	/* 10 bright green */
		0x00FFFF55,	/* 11 bright yellow */
		0x005555FF,	/* 12 bright blue  */
		0x00FF55FF,	/* 13 bright magenta */
		0x0055FFFF,	/* 14 bright cyan  */
		0x00FFFFFF,	/* 15 bright white */
	};
	return ansi_colors[color & 0xf];
}

/*
 * Blit pixel data to the framebuffer (normal context — may use bcopy).
 * data points to width*height pixels in XRGB 32-bit format.
 */
static void
vmwgfx_vis_blit(struct vmwgfx_state *state,
    screen_pos_t row, screen_pos_t col,
    screen_size_t width, screen_size_t height,
    uint8_t *data)
{
	uint32_t y;
	uint8_t *fb = (uint8_t *)state->vram_va;

	for (y = 0; y < (uint32_t)height; y++) {
		uint8_t *dst = fb +
		    ((uint32_t)row + y) * state->fb_stride +
		    (uint32_t)col * 4;
		uint8_t *src = data + y * (uint32_t)width * 4;
		bcopy(src, dst, (size_t)width * 4);
	}
}

/*
 * VIS_CONSDISPLAY — display pixel data at (row, col).
 */
static void
vmwgfx_vis_display(struct vmwgfx_state *state, struct vis_consdisplay *dp)
{
	if (state->vram_va == NULL)
		return;
	vmwgfx_vis_blit(state, dp->row, dp->col,
	    dp->width, dp->height, dp->data);
}

/*
 * VIS_CONSCOPY — copy a rectangle within the framebuffer (scroll support).
 * Must handle overlapping regions correctly; use memmove per-row.
 */
static void
vmwgfx_vis_copy(struct vmwgfx_state *state, struct vis_conscopy *mp)
{
	uint8_t *fb = (uint8_t *)state->vram_va;
	uint32_t stride = state->fb_stride;
	uint32_t height = (uint32_t)(mp->e_row - mp->s_row + 1);
	uint32_t width_bytes = (uint32_t)(mp->e_col - mp->s_col + 1) * 4;
	int32_t i;

	if (fb == NULL)
		return;

	if (mp->t_row <= mp->s_row) {
		/* Copy top-to-bottom */
		for (i = 0; i < (int32_t)height; i++) {
			uint8_t *src = fb + ((uint32_t)mp->s_row + i) * stride +
			    (uint32_t)mp->s_col * 4;
			uint8_t *dst = fb + ((uint32_t)mp->t_row + i) * stride +
			    (uint32_t)mp->t_col * 4;
			ovbcopy(src, dst, width_bytes);
		}
	} else {
		/* Copy bottom-to-top to handle downward overlap */
		for (i = (int32_t)height - 1; i >= 0; i--) {
			uint8_t *src = fb + ((uint32_t)mp->s_row + i) * stride +
			    (uint32_t)mp->s_col * 4;
			uint8_t *dst = fb + ((uint32_t)mp->t_row + i) * stride +
			    (uint32_t)mp->t_col * 4;
			ovbcopy(src, dst, width_bytes);
		}
	}
}

/*
 * VIS_CONSCURSOR — draw or hide a cursor block at (row, col).
 * We invert the pixels in the cursor rectangle.
 */
static void
vmwgfx_vis_cursor(struct vmwgfx_state *state, struct vis_conscursor *cp)
{
	uint8_t *fb = (uint8_t *)state->vram_va;
	uint32_t stride = state->fb_stride;
	uint32_t x, y;

	if (fb == NULL || cp->action == VIS_GET_CURSOR)
		return;

	for (y = 0; y < (uint32_t)cp->height; y++) {
		uint32_t *row = (uint32_t *)(fb +
		    ((uint32_t)cp->row + y) * stride +
		    (uint32_t)cp->col * 4);
		for (x = 0; x < (uint32_t)cp->width; x++)
			row[x] ^= 0x00FFFFFF;
	}
}

/*
 * VIS_CONSCLEAR — clear framebuffer with a background color.
 */
static void
vmwgfx_vis_clear(struct vmwgfx_state *state, struct vis_consclear *clp)
{
	uint8_t *fb = (uint8_t *)state->vram_va;
	uint32_t npixels, i;
	uint32_t color;
	uint32_t *p;

	if (fb == NULL)
		return;

	color = vmwgfx_color_map(clp->bg_color.eight);
	npixels = state->fb_width * state->fb_height;
	p = (uint32_t *)fb;
	for (i = 0; i < npixels; i++)
		p[i] = color;
}

/* ---- polled I/O callbacks (no DDI services available) ---- */

static void
vmwgfx_poll_display(struct vis_polledio_arg *arg, struct vis_consdisplay *dp)
{
	struct vmwgfx_state *state = (struct vmwgfx_state *)arg;
	vmwgfx_vis_display(state, dp);
}

static void
vmwgfx_poll_copy(struct vis_polledio_arg *arg, struct vis_conscopy *mp)
{
	struct vmwgfx_state *state = (struct vmwgfx_state *)arg;
	vmwgfx_vis_copy(state, mp);
}

static void
vmwgfx_poll_cursor(struct vis_polledio_arg *arg, struct vis_conscursor *cp)
{
	struct vmwgfx_state *state = (struct vmwgfx_state *)arg;
	vmwgfx_vis_cursor(state, cp);
}

/*
 * vmwgfx_vis_ioctl — handle VIS_* ioctls before passing to DRM.
 *
 * Returns 0 if handled, ENOTTY if not a VIS ioctl.
 */
static int
vmwgfx_vis_ioctl(struct vmwgfx_state *state, int cmd, intptr_t arg, int mode)
{
	switch (cmd) {
	case VIS_GETIDENTIFIER:
		if (ddi_copyout(&vmwgfx_vis_ident, (void *)arg,
		    sizeof (vmwgfx_vis_ident), mode) != 0)
			return (EFAULT);
		return (0);

	case VIS_DEVINIT: {
		struct vis_devinit init;

		if (ddi_copyin((void *)arg, &init, sizeof (init), mode) != 0)
			return (EFAULT);

		init.version   = VIS_CONS_REV;
		init.width     = (screen_size_t)state->fb_width;
		init.height    = (screen_size_t)state->fb_height;
		init.linebytes = (screen_size_t)state->fb_stride;
		init.depth     = 32;
		init.color_map = vmwgfx_color_map;
		init.mode      = VIS_PIXEL;
		init.polledio  = &state->vis_polledio;

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
		vmwgfx_vis_display(state, &disp);
		return (0);
	}

	case VIS_CONSCOPY: {
		struct vis_conscopy cp;

		if (ddi_copyin((void *)arg, &cp, sizeof (cp), mode) != 0)
			return (EFAULT);
		vmwgfx_vis_copy(state, &cp);
		return (0);
	}

	case VIS_CONSCURSOR: {
		struct vis_conscursor cur;

		if (ddi_copyin((void *)arg, &cur, sizeof (cur), mode) != 0)
			return (EFAULT);
		vmwgfx_vis_cursor(state, &cur);
		if (cur.action == VIS_GET_CURSOR) {
			cur.row = 0;
			cur.col = 0;
			if (ddi_copyout(&cur, (void *)arg,
			    sizeof (cur), mode) != 0)
				return (EFAULT);
		}
		return (0);
	}

	case VIS_CONSCLEAR: {
		struct vis_consclear clr;

		if (ddi_copyin((void *)arg, &clr, sizeof (clr), mode) != 0)
			return (EFAULT);
		vmwgfx_vis_clear(state, &clr);
		return (0);
	}

	default:
		return (ENOTTY);
	}
}

static int
vmwgfx_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct vmwgfx_state *state;
	struct pci_dev *pdev;
	ddi_acc_handle_t cfg_handle;
	const struct pci_device_id *match;
	int instance, ret;

	if (cmd != DDI_ATTACH)
		return DDI_FAILURE;

	instance = ddi_get_instance(dip);

	/* Open PCI config space */
	if (pci_config_setup(dip, &cfg_handle) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "vmwgfx: pci_config_setup failed");
		return DDI_FAILURE;
	}

	/* Allocate and fill Linux pci_dev wrapper */
	state = kmem_zalloc(sizeof(*state), KM_SLEEP);
	pdev = &state->pci_dev;
	drm_illumos_pci_init_device(pdev, dip, cfg_handle);

	/* Match against vmwgfx PCI ID table */
	match = pci_match_id(vmw_pci_driver.id_table, pdev);
	if (match == NULL) {
		cmn_err(CE_WARN, "vmwgfx: no matching PCI ID for %04x:%04x",
		    pdev->vendor, pdev->device);
		goto fail_match;
	}

	/* Save state */
	ddi_set_driver_private(dip, state);

	/* Initialize file state */
	drm_illumos_file_state_init(&state->files);

	/* Call the Linux probe function */
	ret = vmw_pci_driver.probe(pdev, match);
	if (ret != 0) {
		cmn_err(CE_WARN, "vmwgfx: probe failed: %d", ret);
		goto fail_probe;
	}

	/*
	 * Map the full VRAM (BAR1, rnumber=2) into kernel VA space.
	 *
	 * size=0 means "map the entire BAR".  The mapping is used both for:
	 *  - the VIS framebuffer console (offset 0, fb_size bytes)
	 *  - TTM buffer moves from system memory → VRAM (all of VRAM)
	 *
	 * TTM's ttm_kmap_iter_linear_io_init checks mem->bus.addr first; if
	 * set it uses it directly without calling ioremap (which is Linux-only).
	 * vmw_ttm_io_mem_reserve populates bus.addr from vmwgfx_vram_kva().
	 */
	{
		struct drm_device *drm_dev = pci_get_drvdata(pdev);
		struct vmw_private *vmw = vmw_priv(drm_dev);
		uint32_t fb_w = vmw->initial_width;
		uint32_t fb_h = vmw->initial_height;
		size_t fb_size = (size_t)fb_w * fb_h * 4;
		caddr_t vram_base = NULL;
		ddi_acc_handle_t vram_handle;
		ddi_device_acc_attr_t acc = {
			DDI_DEVICE_ATTR_V0,
			DDI_NEVERSWAP_ACC,
			DDI_MERGING_OK_ACC,
		};

		/* Map the full BAR (size=0). */
		if (ddi_regs_map_setup(dip, 2,
		    &vram_base, 0, 0,
		    &acc, &vram_handle) == DDI_SUCCESS) {
			state->vram_va     = vram_base;
			state->vram_handle = vram_handle;
			state->fb_width    = fb_w;
			state->fb_height   = fb_h;
			state->fb_stride   = fb_w * 4;

			/* Zero the visible framebuffer portion */
			if (fb_size > 0)
				bzero(vram_base, fb_size);

			/* Set up polled I/O callbacks */
			state->vis_polledio.arg     =
			    (struct vis_polledio_arg *)state;
			state->vis_polledio.display = vmwgfx_poll_display;
			state->vis_polledio.copy    = vmwgfx_poll_copy;
			state->vis_polledio.cursor  = vmwgfx_poll_cursor;

			cmn_err(CE_CONT,
			    "?vmwgfx: VRAM mapped at %p (full BAR, "
			    "VIS %ux%u %lu bytes)\n",
			    (void *)vram_base, fb_w, fb_h,
			    (unsigned long)fb_size);
		} else {
			cmn_err(CE_WARN,
			    "vmwgfx: failed to map VRAM BAR1");
		}
	}

	/* Expose /dev/dri/card0 and /dev/dri/renderD128 character device nodes.
	 * Node type "ddi_display:drm" is what SUNW_drm_link_i386.so looks for;
	 * it creates the /dev/dri/card<N> and /dev/dri/renderD<N+128> symlinks.
	 * Slot 0 is used for both nodes; the open handler encodes the slot. */
	if (ddi_create_minor_node(dip, "card0", S_IFCHR,
	    drm_illumos_encode_minor(instance, DRM_ILLUMOS_KIND_PRIMARY, 0),
	    "ddi_display:drm", 0) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "vmwgfx: ddi_create_minor_node(card0) failed");
	}
	if (ddi_create_minor_node(dip, "renderD128", S_IFCHR,
	    drm_illumos_encode_minor(instance, DRM_ILLUMOS_KIND_RENDER, 0),
	    "ddi_display:drm", 0) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "vmwgfx: ddi_create_minor_node(renderD128) failed");
	}

	/* Publish global state pointer for cb_open/close/ioctl */
	vmwgfx_global_state = state;

	ddi_report_dev(dip);
	return DDI_SUCCESS;

fail_probe:
	drm_illumos_file_state_destroy(&state->files);
	ddi_set_driver_private(dip, NULL);
fail_match:
	pci_config_teardown(&cfg_handle);
	kmem_free(state, sizeof(*state));
	return DDI_FAILURE;
}

static int
vmwgfx_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct vmwgfx_state *state;
	struct pci_dev *pdev;

	if (cmd != DDI_DETACH)
		return DDI_FAILURE;

	state = ddi_get_driver_private(dip);
	if (state == NULL)
		return DDI_FAILURE;

	pdev = &state->pci_dev;

	/* Clear global state pointer so cb_open/close/ioctl fail gracefully */
	vmwgfx_global_state = NULL;

	/* Remove /dev/dri/card0 node */
	ddi_remove_minor_node(dip, NULL);

	if (vmw_pci_driver.remove)
		vmw_pci_driver.remove(pdev);

	/* Tear down file state (closes all outstanding opens) */
	drm_illumos_file_state_destroy(&state->files);

	/* Free DDI interrupt (should already be removed by vmw_irq_uninstall,
	 * but clean up defensively if detach is called out of order) */
	drm_illumos_irq_uninstall(&state->irq);

	/* Free VRAM console mapping */
	if (state->vram_va != NULL) {
		ddi_regs_map_free(&state->vram_handle);
		state->vram_va = NULL;
	}

	pci_config_teardown(&pdev->config_handle);
	kmem_free(state, sizeof(*state));
	ddi_set_driver_private(dip, NULL);

	return DDI_SUCCESS;
}

static int
vmwgfx_cb_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	struct vmwgfx_state *state;
	struct drm_device *drm;
	int instance, ret, kind;

	(void)flag; (void)otyp; (void)credp;

	state = vmwgfx_global_state;
	if (state == NULL)
		return (ENXIO);

	drm = pci_get_drvdata(&state->pci_dev);
	if (drm == NULL)
		return (ENXIO);

	instance = ddi_get_instance(state->pci_dev.dip);

	/* Determine kind from the encoded minor (set in vmwgfx_attach). */
	kind = drm_illumos_decode_kind(getminor(*devp));

	ret = drm_illumos_open(&state->files, drm, instance, kind, devp);
	return (ret);
}

static int
vmwgfx_cb_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	struct vmwgfx_state *state;

	(void)flag; (void)otyp; (void)credp;
	state = vmwgfx_global_state;
	if (state == NULL)
		return (ENXIO);

	return drm_illumos_close(&state->files, dev);
}

static int
vmwgfx_cb_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
    cred_t *credp, int *rvalp)
{
	struct vmwgfx_state *state;
	int slot, vis_ret;
	struct drm_file *file_priv;

	(void)credp; (void)rvalp;

	slot = drm_illumos_decode_slot(getminor(dev));
	state = vmwgfx_global_state;

	if (state == NULL || slot < 0 || slot >= DRM_ILUMOS_MAX_OPENS)
		return (ENXIO);

	/* VIS ioctls are handled directly without going through DRM */
	vis_ret = vmwgfx_vis_ioctl(state, cmd, arg, mode);
	if (vis_ret != ENOTTY)
		return (vis_ret);

	/*
	 * vmwgfx-specific ioctl checks.  Emulate vmw_generic_ioctl from
	 * vmwgfx_drv.c.
	 */
	mutex_enter(&state->files.mutex);
	if (state->files.opens[slot].in_use) {
		file_priv = state->files.opens[slot].filp.private_data;
		if (file_priv != NULL) {
			unsigned int nr = DRM_IOCTL_NR(cmd);
			if (nr == DRM_COMMAND_BASE + DRM_VMW_UPDATE_LAYOUT) {
				if (!drm_is_current_master(file_priv) &&
				    !capable(CAP_SYS_ADMIN)) {
					mutex_exit(&state->files.mutex);
					return (EACCES);
				}
			}
		}
	}
	mutex_exit(&state->files.mutex);

	return drm_illumos_ioctl(&state->files, dev, cmd, arg);
}

static int
vmwgfx_cb_chpoll(dev_t dev, short events, int anyyet, short *reventsp,
		 struct pollhead **phpp)
{
	struct vmwgfx_state *state;

	state = vmwgfx_global_state;
	if (state == NULL)
		return (ENXIO);

	return drm_illumos_chpoll(&state->files, dev, events, anyyet,
	    reventsp, phpp);
}

static int
vmwgfx_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	(void)dip;

	if (vmwgfx_global_state == NULL)
		return DDI_FAILURE;

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)vmwgfx_global_state->pci_dev.dip;
		return DDI_SUCCESS;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;	/* only one instance */
		return DDI_SUCCESS;
	default:
		return DDI_FAILURE;
	}
}

/* vmwgfx maps GEM/TTM objects through the shared illumos DRM helper. */
static int
vmwgfx_cb_devmap(dev_t dev, devmap_cookie_t dhp, offset_t off,
    size_t len, size_t *maplen, uint_t model)
{
	struct vmwgfx_state *state;
	struct drm_device *drm;
	int ret;

	(void)model; (void)dev;

	state = vmwgfx_global_state;
	if (state == NULL)
		return (ENXIO);

	drm = pci_get_drvdata(&state->pci_dev);
	if (drm == NULL)
		return (ENXIO);

	ret = drm_illumos_gem_ttm_devmap(drm, dhp, off, len, maplen);
	if (ret != 0)
		cmn_err(CE_WARN, "vmwgfx_cb_devmap failed: %d", ret);
	return ret;
}

static struct cb_ops vmwgfx_cb_ops = {
	.cb_open	= vmwgfx_cb_open,
	.cb_close	= vmwgfx_cb_close,
	.cb_strategy	= nodev,
	.cb_print	= nodev,
	.cb_dump	= nodev,
	.cb_read	= nodev,
	.cb_write	= nodev,
	.cb_ioctl	= vmwgfx_cb_ioctl,
	.cb_devmap	= vmwgfx_cb_devmap,
	.cb_mmap	= nodev,
	.cb_segmap	= ddi_devmap_segmap,	/* enables mmap via cb_devmap */
	.cb_chpoll	= vmwgfx_cb_chpoll,
	.cb_prop_op	= ddi_prop_op,
	.cb_str		= NULL,
	.cb_flag	= D_NEW | D_MP,
	.cb_rev		= CB_REV,
	.cb_aread	= nodev,
	.cb_awrite	= nodev,
};

static struct dev_ops vmwgfx_dev_ops = {
	.devo_rev	= DEVO_REV,
	.devo_refcnt	= 0,
	.devo_getinfo	= vmwgfx_getinfo,
	.devo_identify	= nulldev,
	.devo_probe	= nulldev,
	.devo_attach	= vmwgfx_attach,
	.devo_detach	= vmwgfx_detach,
	.devo_reset	= nodev,
	.devo_cb_ops	= &vmwgfx_cb_ops,
	.devo_bus_ops	= NULL,
	.devo_power	= ddi_power,
	.devo_quiesce	= ddi_quiesce_not_needed,
};

static struct modldrv vmwgfx_modldrv = {
	.drv_modops	= &mod_driverops,
	.drv_linkinfo	= "VMware SVGA DRM driver",
	.drv_dev_ops	= &vmwgfx_dev_ops,
};

static struct modlinkage vmwgfx_modlinkage = {
	.ml_rev		= MODREV_1,
	.ml_linkage	= { &vmwgfx_modldrv, NULL },
};

int
_init(void)
{
	return mod_install(&vmwgfx_modlinkage);
}

int
_fini(void)
{
	return mod_remove(&vmwgfx_modlinkage);
}

int
_info(struct modinfo *modinfop)
{
	return mod_info(&vmwgfx_modlinkage, modinfop);
}
