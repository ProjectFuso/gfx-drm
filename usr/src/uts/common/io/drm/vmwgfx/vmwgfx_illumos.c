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

#include <linux/pci.h>
#include <linux/device.h>
#include <drm/drm_drv.h>

/* Forward declarations from vmwgfx_drv.c */
extern struct pci_driver vmw_pci_driver;

/* Per-instance state */
struct vmwgfx_state {
	struct pci_dev	pci_dev;	/* Linux compat PCI device */
};

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

	if (vmw_pci_driver.remove)
		vmw_pci_driver.remove(pdev);

	pci_config_teardown(&pdev->config_handle);
	kmem_free(state, sizeof(*state));
	ddi_set_driver_private(dip, NULL);

	return DDI_SUCCESS;
}

static int
vmwgfx_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	(void)dip; (void)arg; (void)result;
	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
	case DDI_INFO_DEVT2INSTANCE:
		return DDI_FAILURE;
	}
	return DDI_FAILURE;
}

static struct cb_ops vmwgfx_cb_ops = {
	.cb_open	= nulldev,
	.cb_close	= nulldev,
	.cb_strategy	= nodev,
	.cb_print	= nodev,
	.cb_dump	= nodev,
	.cb_read	= nodev,
	.cb_write	= nodev,
	.cb_ioctl	= nodev,
	.cb_devmap	= nodev,
	.cb_mmap	= nodev,
	.cb_segmap	= nodev,
	.cb_chpoll	= nochpoll,
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
