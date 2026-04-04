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

/* drm_open_helper is not in a public header; declare it here */
extern int drm_open_helper(struct file *filp, struct drm_minor *minor);

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

/*
 * vmwgfx_read_pci_bars - Read PCI BAR addresses and sizes from config space.
 *
 * Populates pdev->resource[] by reading each BAR register pair, detecting
 * whether each BAR is I/O or MMIO (32- or 64-bit), and using the standard
 * write-all-ones size-detection technique.
 */
static void
vmwgfx_read_pci_bars(struct pci_dev *pdev, ddi_acc_handle_t cfg_handle)
{
	static const off_t bar_offsets[6] = {
		PCI_CONF_BASE0, PCI_CONF_BASE1, PCI_CONF_BASE2,
		PCI_CONF_BASE3, PCI_CONF_BASE4, PCI_CONF_BASE5,
	};
	int i;

	for (i = 0; i < 6; i++) {
		uint32_t bar_lo = pci_config_get32(cfg_handle, bar_offsets[i]);

		if (bar_lo == 0 || bar_lo == 0xFFFFFFFFU)
			continue;

		if (bar_lo & 1) {
			/* I/O space BAR */
			uint32_t saved, mask;
			resource_size_t base, size;

			base = (resource_size_t)(bar_lo & ~0x3U);

			saved = bar_lo;
			pci_config_put32(cfg_handle, bar_offsets[i], 0xFFFFFFFFU);
			mask = pci_config_get32(cfg_handle, bar_offsets[i]);
			pci_config_put32(cfg_handle, bar_offsets[i], saved);

			size = (resource_size_t)((~(mask & ~0x3U) + 1U) & 0xFFFFU);
			if (size == 0)
				size = 256;

			pdev->resource[i].start = base;
			pdev->resource[i].end   = base + size - 1;
			pdev->resource[i].flags = IORESOURCE_IO;
		} else {
			uint8_t type = (uint8_t)((bar_lo >> 1) & 0x3);

			if (type == 2 && i < 5) {
				/* 64-bit MMIO BAR — low half at [i], high at [i+1] */
				uint32_t bar_hi, saved_lo, saved_hi;
				uint32_t mask_lo, mask_hi;
				uint64_t full_base, full_mask, size;

				bar_hi   = pci_config_get32(cfg_handle, bar_offsets[i + 1]);
				full_base = ((uint64_t)bar_hi << 32) |
				            (uint64_t)(bar_lo & ~0xFU);

				saved_lo = bar_lo;
				saved_hi = bar_hi;
				pci_config_put32(cfg_handle, bar_offsets[i],     0xFFFFFFFFU);
				pci_config_put32(cfg_handle, bar_offsets[i + 1], 0xFFFFFFFFU);
				mask_lo = pci_config_get32(cfg_handle, bar_offsets[i]);
				mask_hi = pci_config_get32(cfg_handle, bar_offsets[i + 1]);
				pci_config_put32(cfg_handle, bar_offsets[i],     saved_lo);
				pci_config_put32(cfg_handle, bar_offsets[i + 1], saved_hi);

				full_mask = ((uint64_t)mask_hi << 32) |
				            (uint64_t)(mask_lo & ~0xFU);
				size = ~full_mask + 1ULL;
				if (size == 0)
					size = 4096;

				pdev->resource[i].start = (resource_size_t)full_base;
				pdev->resource[i].end   = (resource_size_t)(full_base + size - 1);
				pdev->resource[i].flags = IORESOURCE_MEM | IORESOURCE_MEM_64;

				/* The next config register belongs to this BAR. */
				i++;
			} else {
				/* 32-bit MMIO BAR */
				uint32_t saved, mask;
				resource_size_t base, size;

				base = (resource_size_t)(bar_lo & ~0xFU);

				saved = bar_lo;
				pci_config_put32(cfg_handle, bar_offsets[i], 0xFFFFFFFFU);
				mask = pci_config_get32(cfg_handle, bar_offsets[i]);
				pci_config_put32(cfg_handle, bar_offsets[i], saved);

				size = (resource_size_t)((~(mask & ~0xFU) + 1U) & 0xFFFFFFFFU);
				if (size == 0)
					size = 4096;

				pdev->resource[i].start = base;
				pdev->resource[i].end   = base + size - 1;
				pdev->resource[i].flags = IORESOURCE_MEM;
			}
		}
	}
}

static ddi_device_acc_attr_t ioremap_acc_attr = {
	DDI_DEVICE_ATTR_V0,
	DDI_NEVERSWAP_ACC,
	DDI_STRICTORDER_ACC,
};

/*
 * MMIO mapping tracking.
 *
 * ddi_regs_map_setup returns a base address and an acc_handle.
 * We need to track these so memunmap/iounmap can call ddi_regs_map_free.
 * Up to 6 PCI BARs can be mapped simultaneously.
 */
#define	IOREMAP_MAX	6

struct ioremap_entry {
	caddr_t		base;		/* mapped kernel virtual address */
	size_t		size;		/* mapping size */
	ddi_acc_handle_t handle;	/* DDI access handle for unmap */
	int		bar;		/* BAR index (0-5) */
	boolean_t	in_use;
};

/*
 * IRQ state — tracks the DDI interrupt handle and Linux handler pointers
 * so the DDI hard-IRQ callback can dispatch to the driver's handlers.
 */
struct vmwgfx_irq_state {
	ddi_intr_handle_t	intr_hdl;	/* DDI interrupt handle */
	irq_handler_t		handler;	/* hard-IRQ handler (vmw_irq_handler) */
	irq_handler_t		thread_fn;	/* deferred handler (vmw_thread_fn) */
	void			*dev_id;	/* opaque arg passed to handlers */
	taskq_t			*tq;		/* taskq for thread_fn dispatch */
	boolean_t		registered;
};

/* Maximum simultaneous DRM file opens */
#define	VMWGFX_MAX_OPENS	64

/* Per-open file descriptor */
struct vmwgfx_open {
	struct file		filp;		/* Linux compat file handle */
	struct drm_minor	*minor;		/* DRM minor for this open */
	boolean_t		in_use;
};

/* Minor number macros — slot fits in 6 bits (0..63) */
#define	VMWGFX_MINOR_SLOT(m)		((int)((m) & 0x3f))

/* VIS identifier string returned to the terminal emulator */
static const struct vis_identifier vmwgfx_vis_ident = {
	"ILLUMOSvmwgfx"
};

/* Per-instance state */
struct vmwgfx_state {
	struct pci_dev		pci_dev;	/* Linux compat PCI device */
	struct ioremap_entry	maps[IOREMAP_MAX];
	struct vmwgfx_open	opens[VMWGFX_MAX_OPENS];

	/* VIS framebuffer console state */
	caddr_t			vram_va;	/* kernel VA of VRAM (BAR1) */
	ddi_acc_handle_t	vram_handle;	/* DDI acc handle for VRAM */
	uint32_t		fb_width;	/* display width in pixels */
	uint32_t		fb_height;	/* display height in pixels */
	uint32_t		fb_stride;	/* bytes per scan line */
	struct vis_polledio	vis_polledio;	/* polled I/O callbacks */

	/* IRQ state */
	struct vmwgfx_irq_state	irq;
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

/*
 * illumos_ioremap — Map a PCI BAR region into kernel virtual address space.
 *
 * Matches phys_addr against pdev->resource[] to find the BAR index, then
 * calls ddi_regs_map_setup with rnumber = bar_index + 1 (rnumber 0 is
 * PCI config space).  The mapping is tracked in the vmwgfx_state so
 * illumos_iounmap can free it later.
 */
void *
illumos_ioremap(struct pci_dev *pdev, resource_size_t phys_addr, size_t size)
{
	struct vmwgfx_state *state;
	int bar, ret, i;
	off_t bar_offset;
	caddr_t base;
	ddi_acc_handle_t handle;

	if (pdev == NULL || pdev->dip == NULL)
		return (NULL);

	state = ddi_get_driver_private(pdev->dip);
	if (state == NULL)
		return (NULL);

	/* Find which BAR contains phys_addr */
	bar = -1;
	bar_offset = 0;
	for (i = 0; i < 6; i++) {
		resource_size_t start = pdev->resource[i].start;
		resource_size_t end = pdev->resource[i].end;

		if (start == 0)
			continue;
		if (phys_addr >= start && phys_addr <= end) {
			bar = i;
			bar_offset = (off_t)(phys_addr - start);
			break;
		}
	}

	if (bar < 0) {
		cmn_err(CE_WARN,
		    "illumos_ioremap: phys 0x%llx does not match any BAR",
		    (unsigned long long)phys_addr);
		return (NULL);
	}

	/* Find a free tracking slot */
	for (i = 0; i < IOREMAP_MAX; i++) {
		if (!state->maps[i].in_use)
			break;
	}
	if (i >= IOREMAP_MAX) {
		cmn_err(CE_WARN, "illumos_ioremap: no free mapping slots");
		return (NULL);
	}

	/*
	 * rnumber = bar + 1 because rnumber 0 is PCI config space.
	 * For 64-bit BARs that consume two slots, the DDI still
	 * uses a single rnumber for the 64-bit region.
	 */
	ret = ddi_regs_map_setup(pdev->dip, bar + 1,
	    &base, bar_offset, (off_t)size,
	    &ioremap_acc_attr, &handle);

	if (ret != DDI_SUCCESS) {
		cmn_err(CE_WARN,
		    "illumos_ioremap: ddi_regs_map_setup(bar=%d, off=0x%lx, "
		    "size=0x%lx) failed: %d",
		    bar, (unsigned long)bar_offset, (unsigned long)size, ret);
		return (NULL);
	}

	state->maps[i].base = base;
	state->maps[i].size = size;
	state->maps[i].handle = handle;
	state->maps[i].bar = bar;
	state->maps[i].in_use = B_TRUE;

	cmn_err(CE_CONT,
	    "?illumos_ioremap: BAR%d phys 0x%llx+0x%lx -> VA %p\n",
	    bar, (unsigned long long)phys_addr,
	    (unsigned long)size, (void *)base);

	return ((void *)base);
}

/*
 * illumos_iounmap — Unmap a previously mapped BAR region.
 */
void
illumos_iounmap(struct pci_dev *pdev, void *addr)
{
	struct vmwgfx_state *state;
	int i;

	if (pdev == NULL || addr == NULL)
		return;

	state = ddi_get_driver_private(pdev->dip);
	if (state == NULL)
		return;

	for (i = 0; i < IOREMAP_MAX; i++) {
		if (state->maps[i].in_use &&
		    state->maps[i].base == (caddr_t)addr) {
			ddi_regs_map_free(&state->maps[i].handle);
			state->maps[i].in_use = B_FALSE;
			return;
		}
	}

	cmn_err(CE_WARN, "illumos_iounmap: addr %p not found", addr);
}

/* ------------------------------------------------------------------ */
/* IRQ implementation                                                  */
/* ------------------------------------------------------------------ */

/*
 * vmwgfx_irq_thread — taskq worker that runs the deferred thread_fn.
 * Called from a non-interrupt context so it can use mutex/cv safely.
 */
static void
vmwgfx_irq_thread(void *arg)
{
	struct vmwgfx_state *state = (struct vmwgfx_state *)arg;
	state->irq.thread_fn(0, state->irq.dev_id);
}

/*
 * vmwgfx_ddi_intr_handler — DDI hard-interrupt callback.
 *
 * Calls the Linux-style hard-IRQ handler.  If it returns IRQ_WAKE_THREAD
 * we dispatch the deferred thread_fn via the taskq.
 */
static uint_t
vmwgfx_ddi_intr_handler(caddr_t arg1, caddr_t arg2)
{
	struct vmwgfx_state *state = (struct vmwgfx_state *)(void *)arg1;
	irqreturn_t ret;

	(void)arg2;

	ret = state->irq.handler(0, state->irq.dev_id);

	if (ret == IRQ_NONE)
		return (DDI_INTR_UNCLAIMED);

	if (ret == IRQ_WAKE_THREAD && state->irq.thread_fn != NULL)
		(void) taskq_dispatch(state->irq.tq, vmwgfx_irq_thread,
		    state, TQ_NOSLEEP);

	return (DDI_INTR_CLAIMED);
}

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
	int nintrs = 0, actual = 0, ret;

	state = ddi_get_driver_private(pdev->dip);
	if (state == NULL)
		return (-ENODEV);

	if (state->irq.registered) {
		cmn_err(CE_WARN, "vmwgfx: IRQ already registered");
		return (0);
	}

	/* Check how many fixed (INTx) interrupts the device has */
	ret = ddi_intr_get_nintrs(pdev->dip, DDI_INTR_TYPE_FIXED, &nintrs);
	if (ret != DDI_SUCCESS || nintrs < 1) {
		cmn_err(CE_WARN,
		    "vmwgfx: ddi_intr_get_nintrs failed or no fixed IRQs "
		    "(ret=%d nintrs=%d)", ret, nintrs);
		return (-ENODEV);
	}

	/* Create a single-thread taskq for the deferred handler */
	state->irq.tq = taskq_create("vmwgfx_irqthr", 1, maxclsyspri,
	    1, 1, TASKQ_PREPOPULATE);
	if (state->irq.tq == NULL) {
		cmn_err(CE_WARN, "vmwgfx: taskq_create for IRQ thread failed");
		return (-ENOMEM);
	}

	/* Allocate one fixed interrupt */
	ret = ddi_intr_alloc(pdev->dip, &state->irq.intr_hdl,
	    DDI_INTR_TYPE_FIXED, 0, 1, &actual, DDI_INTR_ALLOC_NORMAL);
	if (ret != DDI_SUCCESS || actual < 1) {
		cmn_err(CE_WARN,
		    "vmwgfx: ddi_intr_alloc failed (ret=%d actual=%d)",
		    ret, actual);
		taskq_destroy(state->irq.tq);
		state->irq.tq = NULL;
		return (-ENODEV);
	}

	/* Store Linux handler pointers */
	state->irq.handler   = vmw_irq_handler;
	state->irq.thread_fn = vmw_thread_fn;
	state->irq.dev_id    = &dev_priv->drm;

	/* Add the DDI interrupt handler */
	ret = ddi_intr_add_handler(state->irq.intr_hdl,
	    vmwgfx_ddi_intr_handler, (caddr_t)state, NULL);
	if (ret != DDI_SUCCESS) {
		cmn_err(CE_WARN,
		    "vmwgfx: ddi_intr_add_handler failed (ret=%d)", ret);
		ddi_intr_free(state->irq.intr_hdl);
		taskq_destroy(state->irq.tq);
		state->irq.tq = NULL;
		return (-ENODEV);
	}

	/* Enable the interrupt */
	ret = ddi_intr_enable(state->irq.intr_hdl);
	if (ret != DDI_SUCCESS) {
		cmn_err(CE_WARN,
		    "vmwgfx: ddi_intr_enable failed (ret=%d)", ret);
		ddi_intr_remove_handler(state->irq.intr_hdl);
		ddi_intr_free(state->irq.intr_hdl);
		taskq_destroy(state->irq.tq);
		state->irq.tq = NULL;
		return (-ENODEV);
	}

	state->irq.registered = B_TRUE;
	dev_priv->irqs[0]        = 0;
	dev_priv->num_irq_vectors = 1;

	cmn_err(CE_CONT, "?vmwgfx: DDI fixed interrupt registered\n");
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

	ddi_intr_disable(state->irq.intr_hdl);
	ddi_intr_remove_handler(state->irq.intr_hdl);
	ddi_intr_free(state->irq.intr_hdl);

	taskq_destroy(state->irq.tq);
	state->irq.tq = NULL;
	state->irq.registered = B_FALSE;

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

	pdev->dip = dip;
	pdev->config_handle = cfg_handle;
	pdev->vendor = pci_config_get16(cfg_handle, PCI_CONF_VENID);
	pdev->device = pci_config_get16(cfg_handle, PCI_CONF_DEVID);
	pdev->subsystem_vendor = pci_config_get16(cfg_handle, PCI_CONF_SUBVENID);
	pdev->subsystem_device = pci_config_get16(cfg_handle, PCI_CONF_SUBSYSID);
	pdev->revision = pci_config_get8(cfg_handle, PCI_CONF_REVID);
	pdev->class = (uint32_t)pci_config_get8(cfg_handle, PCI_CONF_BASCLASS) << 16 |
	              (uint32_t)pci_config_get8(cfg_handle, PCI_CONF_SUBCLASS) << 8 |
	              (uint32_t)pci_config_get8(cfg_handle, PCI_CONF_PROGCLASS);
	pdev->bus = &pdev->_bus;
	pdev->bus->config_handle = cfg_handle;

	/* Populate resource[] from PCI BAR registers */
	vmwgfx_read_pci_bars(pdev, cfg_handle);

	/* Set up embedded Linux device */
	pdev->dev.dip = dip;
	pdev->dev.pdev = pdev;

	/* Match against vmwgfx PCI ID table */
	match = pci_match_id(vmw_pci_driver.id_table, pdev);
	if (match == NULL) {
		cmn_err(CE_WARN, "vmwgfx: no matching PCI ID for %04x:%04x",
		    pdev->vendor, pdev->device);
		goto fail_match;
	}

	/* Save state */
	ddi_set_driver_private(dip, state);

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

	/* Expose /dev/dri/card0 character device node.
	 * Node type "ddi_display:drm" is what SUNW_drm_link_i386.so looks for;
	 * it creates the /dev/dri/card<N> symlink automatically. */
	if (ddi_create_minor_node(dip, "card0", S_IFCHR,
	    (minor_t)instance, "ddi_display:drm", 0) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "vmwgfx: ddi_create_minor_node failed");
		/* Non-fatal — driver still usable via kernel paths */
	}

	/* Publish global state pointer for cb_open/close/ioctl */
	vmwgfx_global_state = state;

	ddi_report_dev(dip);
	return DDI_SUCCESS;

fail_probe:
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

	/* Free DDI interrupt (should already be removed by vmw_irq_uninstall,
	 * but clean up defensively if detach is called out of order) */
	if (state->irq.registered) {
		ddi_intr_disable(state->irq.intr_hdl);
		ddi_intr_remove_handler(state->irq.intr_hdl);
		ddi_intr_free(state->irq.intr_hdl);
		taskq_destroy(state->irq.tq);
		state->irq.tq = NULL;
		state->irq.registered = B_FALSE;
	}

	/* Free VRAM console mapping */
	if (state->vram_va != NULL) {
		ddi_regs_map_free(&state->vram_handle);
		state->vram_va = NULL;
	}

	/* Free any active MMIO mappings */
	{
		int i;
		for (i = 0; i < IOREMAP_MAX; i++) {
			if (state->maps[i].in_use) {
				ddi_regs_map_free(&state->maps[i].handle);
				state->maps[i].in_use = B_FALSE;
			}
		}
	}

	pci_config_teardown(&pdev->config_handle);
	kmem_free(state, sizeof(*state));
	ddi_set_driver_private(dip, NULL);

	return DDI_SUCCESS;
}

/*
 * Fake file_operations used to satisfy drm_open_helper's
 * WARN_ON_ONCE(!(filp->f_op->fop_flags & FOP_UNSIGNED_OFFSET)).
 */
static const struct file_operations vmwgfx_fops_stub = {
	.fop_flags = FOP_UNSIGNED_OFFSET,
};

/*
 * vmwgfx_cb_open — illumos DDI cb_open entry point.
 *
 * Allocates a drm_file context and calls drm_open_helper().
 * The open slot index (0..VMWGFX_MAX_OPENS-1) is encoded as the minor
 * number returned to the DDI so subsequent close/ioctl can find it.
 */
static int
vmwgfx_cb_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	struct vmwgfx_state *state;
	struct drm_device *drm;
	struct drm_minor *minor;
	struct vmwgfx_open *op;
	int slot, ret;

	(void)flag; (void)otyp; (void)credp;

	state = vmwgfx_global_state;
	if (state == NULL)
		return (ENXIO);

	drm = pci_get_drvdata(&state->pci_dev);
	if (drm == NULL)
		return (ENXIO);

	/* Use primary minor */
	minor = drm->primary;
	if (minor == NULL)
		return (ENXIO);

	/* Find a free open slot */
	for (slot = 0; slot < VMWGFX_MAX_OPENS; slot++) {
		if (!state->opens[slot].in_use)
			break;
	}
	if (slot >= VMWGFX_MAX_OPENS)
		return (EBUSY);

	op = &state->opens[slot];
	bzero(&op->filp, sizeof (op->filp));
	op->filp.f_op = &vmwgfx_fops_stub;
	op->minor = minor;

	drm_dev_get(drm);
	atomic_inc(&drm->open_count);

	ret = drm_open_helper(&op->filp, minor);
	if (ret != 0) {
		atomic_dec(&drm->open_count);
		drm_dev_put(drm);
		return (-ret);
	}

	op->in_use = B_TRUE;

	/* Encode the open slot into the minor number returned to userspace */
	*devp = makedevice(getmajor(*devp), (minor_t)slot);
	return (0);
}

static int
vmwgfx_cb_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	struct vmwgfx_state *state;
	struct vmwgfx_open *op;
	struct drm_device *drm;
	int slot;

	(void)flag; (void)otyp; (void)credp;

	slot = VMWGFX_MINOR_SLOT(getminor(dev));
	state = vmwgfx_global_state;

	if (state == NULL || slot < 0 || slot >= VMWGFX_MAX_OPENS ||
	    !state->opens[slot].in_use)
		return (ENXIO);

	op = &state->opens[slot];
	drm = pci_get_drvdata(&state->pci_dev);
	if (drm == NULL)
		return (ENXIO);

	/*
	 * drm_release handles drm_close_helper, open_count decrement, and
	 * drm_minor_release (which calls drm_dev_put).  This balances the
	 * drm_dev_get we did in vmwgfx_cb_open.  Do NOT call drm_minor_release
	 * or drm_dev_put again — they would be double-releases.
	 */
	drm_release(NULL, &op->filp);

	op->in_use = B_FALSE;
	return (0);
}

static int
vmwgfx_cb_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
    cred_t *credp, int *rvalp)
{
	struct vmwgfx_state *state;
	struct vmwgfx_open *op;
	int slot, vis_ret;
	long ret;

	(void)credp; (void)rvalp;

	slot = VMWGFX_MINOR_SLOT(getminor(dev));
	state = vmwgfx_global_state;

	if (state == NULL || slot < 0 || slot >= VMWGFX_MAX_OPENS ||
	    !state->opens[slot].in_use)
		return (ENXIO);

	/* VIS ioctls are handled directly without going through DRM */
	vis_ret = vmwgfx_vis_ioctl(state, cmd, arg, mode);
	if (vis_ret != ENOTTY)
		return (vis_ret);

	op = &state->opens[slot];

	ret = drm_ioctl(&op->filp, (unsigned int)cmd, (unsigned long)arg);
	return (ret < 0 ? (int)-ret : 0);
}

// To Claude Code: I implemented this preliminary for you; please review
// and fix this.
static int
vmwgfx_cb_chpoll(dev_t dev, short events, int anyyet, short *reventsp,
		 struct pollhead **phpp)
{
  	struct vmwgfx_state *state;
	struct vmwgfx_open *op;
        struct drm_file *file_priv;
	int slot, vis_ret;
	short revents = 0;
	long ret;

	slot = VMWGFX_MINOR_SLOT(getminor(dev));
	state = vmwgfx_global_state;

	if (state == NULL || slot < 0 || slot >= VMWGFX_MAX_OPENS ||
	    !state->opens[slot].in_use)
		return (ENXIO);

	op = &state->opens[slot];
        file_priv = op->filp.private_data;
        if (!file_priv)
              return (EBADF);

        if (!anyyet) {
	  *phpp = &file_priv->drm_pollhead;
        }

        // *reventsp = drm_poll(file_priv, events); // signature mismatch

	/* 2. Xorg only polls for incoming data (Read events) */
	if (events & (POLLIN | POLLRDNORM)) {

          /* Lock the event list just like the read() syscall would */
          mutex_enter(&file_priv->event_read_lock);

          /* 3. If the list has events, we have data ready to be read! */
          if (!list_empty(&file_priv->event_list)) {
            revents |= events & (POLLIN | POLLRDNORM);
          }

          mutex_exit(&file_priv->event_read_lock);
	}

	*reventsp = revents;

        return (0);
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

/*
 * vmwgfx_cb_devmap — mmap GEM buffer objects to userspace.
 *
 * When userspace calls mmap(fd, offset, size) on /dev/dri/card0, the offset
 * comes from DRM_IOCTL_MODE_MAP_DUMB (or drm_gem_create_mmap_offset).  The
 * DRM VMA offset manager stores a mapping from that offset to the GEM object.
 *
 * We look up the GEM object, get its TTM backing store, and use the
 * ddi_umem_cookie from ttm_pool_alloc() to map the pages into userspace.
 */
static int
vmwgfx_cb_devmap(dev_t dev, devmap_cookie_t dhp, offset_t off,
    size_t len, size_t *maplen, uint_t model)
{
	struct vmwgfx_state *state;
	struct drm_device *drm;
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

	(void)model; (void)dev;

	state = vmwgfx_global_state;
	if (state == NULL)
		return (ENXIO);

	drm = pci_get_drvdata(&state->pci_dev);
	if (drm == NULL)
		return (ENXIO);

	mgr = drm->vma_offset_manager;
	if (mgr == NULL)
		return (ENXIO);

	pgoff  = (unsigned long)(off >> PAGE_SHIFT);
	npages = (unsigned long)((len + PAGE_SIZE - 1) >> PAGE_SHIFT);

	drm_vma_offset_lock_lookup(mgr);
	node = drm_vma_offset_lookup_locked(mgr, pgoff, npages);
	if (node == NULL) {
		drm_vma_offset_unlock_lookup(mgr);
		cmn_err(CE_WARN,
		    "vmwgfx_cb_devmap: no GEM object at pgoff 0x%lx", pgoff);
		return (EINVAL);
	}
	gem = container_of(node, struct drm_gem_object, vma_node);
	drm_gem_object_get(gem);
	drm_vma_offset_unlock_lookup(mgr);

	/* GEM object must be backed by a TTM BO */
	bo = container_of(gem, struct ttm_buffer_object, base);

	/*
	 * For TTM_PL_SYSTEM BOs, bo->ttm is NULL until first CPU access.
	 * We need the pages allocated now for devmap_umem_setup, so force
	 * TTM TT creation and population under the BO reservation lock.
	 */
	if (bo->ttm == NULL || bo->ttm->illumos_umem_cookie == NULL) {
		struct ttm_operation_ctx ctx = {
			.interruptible	= false,
			.no_wait_gpu	= false,
		};

		ret = ttm_bo_reserve(bo, false, false, NULL);
		if (ret != 0) {
			cmn_err(CE_WARN,
			    "vmwgfx_cb_devmap: ttm_bo_reserve failed: %d", ret);
			drm_gem_object_put(gem);
			return (EINVAL);
		}

		if (bo->ttm == NULL) {
			ret = ttm_tt_create(bo, false);
			if (ret != 0) {
				ttm_bo_unreserve(bo);
				cmn_err(CE_WARN,
				    "vmwgfx_cb_devmap: ttm_tt_create failed: %d",
				    ret);
				drm_gem_object_put(gem);
				return (EINVAL);
			}
		}

		if (!ttm_tt_is_populated(bo->ttm)) {
			ret = ttm_tt_populate(bo->bdev, bo->ttm, &ctx);
			if (ret != 0) {
				ttm_bo_unreserve(bo);
				cmn_err(CE_WARN,
				    "vmwgfx_cb_devmap: ttm_tt_populate failed: %d",
				    ret);
				drm_gem_object_put(gem);
				return (EINVAL);
			}
		}

		ttm_bo_unreserve(bo);
	}

	ttm = bo->ttm;

	if (ttm == NULL || ttm->illumos_umem_cookie == NULL) {
		cmn_err(CE_WARN,
		    "vmwgfx_cb_devmap: GEM obj %p has no TTM umem cookie after "
		    "populate", gem);
		drm_gem_object_put(gem);
		return (EINVAL);
	}

	/*
	 * The mmap offset may start at any page within the VMA node.
	 * Compute the byte offset into the umem region.
	 */
	node_pgoff = pgoff - drm_vma_node_start(node);
	map_off    = node_pgoff << PAGE_SHIFT;

	if (map_off + len > (size_t)gem->size) {
		drm_gem_object_put(gem);
		return (EINVAL);
	}

	ret = devmap_umem_setup(dhp, drm->dev->pdev->dip,
	    NULL,			/* devmap_callback_ctl — no fault handler */
	    ttm->illumos_umem_cookie,
	    map_off,			/* offset into umem region */
	    len,
	    PROT_READ | PROT_WRITE | PROT_USER,
	    DEVMAP_DEFAULTS,
	    NULL);			/* ddi_device_acc_attr — use default */

	drm_gem_object_put(gem);

	if (ret != 0) {
		cmn_err(CE_WARN,
		    "vmwgfx_cb_devmap: devmap_umem_setup failed: %d", ret);
		return (ret);
	}

	*maplen = len;
	return (0);
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
