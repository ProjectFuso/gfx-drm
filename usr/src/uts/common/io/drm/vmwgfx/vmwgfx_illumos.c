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

 /* Per-instance state */
 struct vmwgfx_state {

	struct pci_dev		pci_dev;	/* Linux compat PCI device */
	struct drm_illumos_file_state files;

	/* Full BAR1 kernel mapping used by TTM I/O memory helpers. */
	caddr_t			vram_va;
	ddi_acc_handle_t	vram_handle;

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
	 * size=0 means "map the entire BAR". The mapping is used by TTM
	 * buffer moves from system memory to VRAM.
	 *
	 * TTM's ttm_kmap_iter_linear_io_init checks mem->bus.addr first; if
	 * set it uses it directly without calling ioremap (which is Linux-only).
	 * vmw_ttm_io_mem_reserve populates bus.addr from vmwgfx_vram_kva().
	 */
	{
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

			cmn_err(CE_CONT,
			    "?vmwgfx: VRAM mapped at %p (full BAR)\n",
			    (void *)vram_base);
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

	/* Free VRAM BAR mapping */
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
	int slot;
	struct drm_file *file_priv;

	(void)credp; (void)rvalp;

	slot = drm_illumos_decode_slot(getminor(dev));
	state = vmwgfx_global_state;

	if (state == NULL || slot < 0 || slot >= DRM_ILUMOS_MAX_OPENS)
		return (ENXIO);

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

	return drm_illumos_ioctl(&state->files, dev, cmd, arg, mode);
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
