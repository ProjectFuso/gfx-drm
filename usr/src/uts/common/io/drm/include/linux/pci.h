/*	$OpenBSD: pci.h,v 1.19 2025/02/07 03:03:31 jsg Exp $	*/
/*
 * Copyright (c) 2015 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * illumos port: OpenBSD PCI types replaced with illumos DDI interfaces.
 */

#ifndef _LINUX_PCI_H_
#define _LINUX_PCI_H_

/* Block vm/page.h before sys/sunddi.h can pull it in */
#include <linux/illumos_page_compat.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/pci.h>		/* illumos PCI config constants */

/* illumos: removed OpenBSD includes:
 *   dev/pci/pcireg.h, dev/pci/pcivar.h, dev/pci/pcidevs.h,
 *   uvm/uvm_extern.h, machine/cpu.h
 */

#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kobject.h>
#include <linux/dma-mapping.h>
#include <linux/mod_devicetable.h>
#include <linux/device.h>

struct pci_dev;

/*
 * PCI vendor IDs — standard values, previously from OpenBSD pcidevs.h
 */
#define PCI_VENDOR_ID_APPLE	0x106b
#define PCI_VENDOR_ID_ASUSTEK	0x1043
#define PCI_VENDOR_ID_ATI	0x1002
#define PCI_VENDOR_ID_DELL	0x1028
#define PCI_VENDOR_ID_HP	0x103c
#define PCI_VENDOR_ID_IBM	0x1014
#define PCI_VENDOR_ID_INTEL	0x8086
#define PCI_VENDOR_ID_SONY	0x104d
#define PCI_VENDOR_ID_VIA	0x1106
#define PCI_VENDOR_ID_VMWARE	0x15AD

/* ATI Radeon QY product ID */
#define PCI_DEVICE_ID_ATI_RADEON_QY	0x5159

#define PCI_SUBVENDOR_ID_REDHAT_QUMRANET	0x1af4
#define PCI_SUBDEVICE_ID_QEMU			0x1100

/*
 * PCI class / subclass codes — standard PCI spec values
 */
#ifndef PCI_CLASS_DISPLAY
#define PCI_CLASS_DISPLAY		0x03
#endif
#define PCI_SUBCLASS_DISPLAY_VGA	0x00
#define PCI_SUBCLASS_DISPLAY_MISC	0x80
#define PCI_CLASS_ACCELERATOR		0x12	/* Processing Accelerator */

#define PCI_CLASS_DISPLAY_VGA \
    ((PCI_CLASS_DISPLAY << 8) | PCI_SUBCLASS_DISPLAY_VGA)
#define PCI_CLASS_DISPLAY_OTHER \
    ((PCI_CLASS_DISPLAY << 8) | PCI_SUBCLASS_DISPLAY_MISC)
#define PCI_CLASS_ACCELERATOR_PROCESSING \
    (PCI_CLASS_ACCELERATOR << 8)

/*
 * Standard PCI register offsets — from PCI spec / formerly from pcireg.h
 */
#define PCI_COMMAND		0x04	/* Command register (2 bytes) */
#define PCI_COMMAND_MEMORY	0x0002	/* Memory Space Enable */
#define PCI_PRIMARY_BUS		0x18	/* Primary Bus Number (bridge) */

/*
 * PCIe Capability register offsets (relative to PCIe cap base)
 * formerly from OpenBSD pcireg.h
 */
#define PCI_PCIE_DCSR		0x08	/* Device Control/Status Register */
#define PCI_PCIE_DCSR_MPS	0x7000	/* Max Read Request Size [14:12] */
#define PCI_PCIE_LCSR2		0x30	/* Link Control 2 Register */
#define PCI_PCIE_LCSR2_TLS	0x000f	/* Target Link Speed [3:0] */
#define PCI_PCIE_LCSR2_TLS_2_5	0x1
#define PCI_PCIE_LCSR2_TLS_5	0x2
#define PCI_PCIE_LCSR2_TLS_8	0x3

/* PCIe Capability ID */
#ifndef PCI_CAP_ID_PCI_E
#define PCI_CAP_ID_PCI_E	0x10
#endif
/* Alias for callers that use Linux/BSD naming */
#define PCI_CAP_PCIEXPRESS	PCI_CAP_ID_PCI_E
#define PCI_CAP_ID_EXP		PCI_CAP_ID_PCI_E

/*
 * PCIe capability register offsets (relative to PCIe cap base)
 * BSD naming (PCI_PCIE_*) — used by drm_linux.c
 */
#define PCI_PCIE_XCAP		0x00	/* cap header: version/type/port */
#define PCI_PCIE_XCAP_VER(x)	((x) & 0xf)	/* extract version field */
#define PCI_PCIE_LCAP		0x0c	/* link capabilities */
#define PCI_PCIE_LCAP2		0x2c	/* link capabilities 2 */
#define PCI_PCIE_LCSR		0x10	/* link control/status */
#define PCI_PCIE_LCSR_ASPM_L0S	0x0001	/* ASPM L0s enable */
#define PCI_PCIE_LCSR_ASPM_L1	0x0002	/* ASPM L1 enable */
#define PCI_PCIE_ECAP		0x100	/* extended capability base offset */

/*
 * PCI_EXP_* aliases — map to our PCI_PCIE_* names
 */
#define PCI_EXP_DEVSTA		(PCI_PCIE_DCSR + 2)
#define PCI_EXP_DEVSTA_TRPND	(1 << 5)
#define PCI_EXP_LNKCAP		0x0c
#define PCI_EXP_LNKCAP_CLKPM	(1 << 18)
#define PCI_EXP_LNKCTL		0x10
#define PCI_EXP_LNKCTL_HAWD	(1 << 9)
#define PCI_EXP_LNKSTA		0x12
#define PCI_EXP_DEVCTL2		0x28
#define PCI_EXP_DEVCTL2_LTR_EN	(1 << 10)
#define PCI_EXP_LNKCTL2		PCI_PCIE_LCSR2
#define PCI_EXP_LNKCTL2_ENTER_COMP	(1 << 4)
#define PCI_EXP_LNKCTL2_TX_MARGIN	0x0380
#define PCI_EXP_LNKCTL2_TLS		PCI_PCIE_LCSR2_TLS
#define PCI_EXP_LNKCTL2_TLS_2_5GT	PCI_PCIE_LCSR2_TLS_2_5
#define PCI_EXP_LNKCTL2_TLS_5_0GT	PCI_PCIE_LCSR2_TLS_5
#define PCI_EXP_LNKCTL2_TLS_8_0GT	PCI_PCIE_LCSR2_TLS_8

/*
 * illumos struct pci_bus:
 * Holds DDI config handle for config-space access, plus bus topology info.
 */
struct pci_bus {
	ddi_acc_handle_t config_handle;	/* illumos DDI PCI config access */
	unsigned char	number;		/* bus number */
	int		domain_nr;	/* PCI domain */
	bool		is_root;	/* true if root (no bridge above) */
	struct pci_dev	*self;		/* bridge device (NULL if root) */
};

/*
 * illumos stub for ACPI device node.
 * OpenBSD used struct aml_node; illumos ACPI wiring is deferred.
 */
struct pci_acpi {
	void		*node;		/* acpi_handle stub (void *) */
};

/*
 * illumos: struct resource and IORESOURCE_* are defined in linux/ioport.h,
 * which is included above.
 */
#define PCI_NUM_RESOURCES	7

/*
 * illumos struct pci_dev:
 * Uses ddi_acc_handle_t for PCI config access and dev_info_t for DDI ops.
 */
struct pci_dev {
	struct pci_bus	_bus;		/* embedded bus struct */
	struct pci_bus	*bus;		/* pointer to _bus */

	unsigned int	devfn;		/* encoded device/function */
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subsystem_vendor;
	uint16_t	subsystem_device;
	uint8_t		revision;
	uint32_t	class;		/* class:subclass:interface */

	ddi_acc_handle_t config_handle;	/* illumos DDI PCI config access */
	dev_info_t	*dip;		/* illumos device info node */

	/* PCI BARs — filled in by DDI glue code */
	struct resource	resource[PCI_NUM_RESOURCES];

	int		irq;
	int		msi_enabled;
	uint8_t		no_64bit_msi;
	uint8_t		ltr_path;

	struct pci_acpi	acpi_dev;	/* ACPI stub (renamed: dev is Linux device) */
	struct device	dev;		/* embedded Linux-compat generic device */
};

/* BAR access macros — use resource[] array filled by DDI glue */
#define pci_resource_start(pdev, bar)	((pdev)->resource[(bar)].start)
#define pci_resource_end(pdev, bar)	((pdev)->resource[(bar)].end)
#define pci_resource_len(pdev, bar) \
	((pdev)->resource[(bar)].start == 0 ? 0 : \
	 (pdev)->resource[(bar)].end - (pdev)->resource[(bar)].start + 1)
#define pci_resource_flags(pdev, bar)	((pdev)->resource[(bar)].flags)

#define to_pci_dev(_d)		container_of(_d, struct pci_dev, dev)

#define PCI_ANY_ID (uint16_t) (~0U)

#define PCI_DEVICE(v, p)		\
	.vendor = (v),			\
	.device = (p),			\
	.subvendor = PCI_ANY_ID,	\
	.subdevice = PCI_ANY_ID

#ifndef PCI_MEM_START
#define PCI_MEM_START	0
#endif

#ifndef PCI_MEM_END
#define PCI_MEM_END	0xffffffff
#endif

#ifndef PCI_MEM64_END
#define PCI_MEM64_END	0xffffffffffffffff
#endif

#define PCI_DEVFN(slot, func)	((slot) << 3 | (func))
#define PCI_SLOT(devfn)		((devfn) >> 3)
#define PCI_FUNC(devfn)		((devfn) & 0x7)
#define PCI_BUS_NUM(devfn)	(((devfn) >> 8) & 0xff)

#define pci_dev_put(x)

/*
 * __pci_find_capability — walk PCI capability list using DDI config access.
 * Returns 1 and sets *offset if found, 0 otherwise.
 */
static inline int
__pci_find_capability(ddi_acc_handle_t hdl, int cap_id, int *offset)
{
	uint8_t ptr;
	int i;

	/* Check capability list support bit in Status register */
	if (!(pci_config_get16(hdl, PCI_CONF_STAT) & 0x10))
		return 0;

	ptr = pci_config_get8(hdl, PCI_CONF_CAP_PTR) & ~3;
	for (i = 0; ptr != 0 && i < 48; i++) {
		if (pci_config_get8(hdl, ptr) == (uint8_t)cap_id) {
			if (offset)
				*offset = (int)ptr;
			return 1;
		}
		ptr = pci_config_get8(hdl, ptr + 1) & ~3;
	}
	return 0;
}

static inline int
pci_read_config_dword(struct pci_dev *pdev, int reg, u32 *val)
{
	*val = pci_config_get32(pdev->config_handle, (off_t)reg);
	return 0;
}

static inline int
pci_read_config_word(struct pci_dev *pdev, int reg, u16 *val)
{
	*val = pci_config_get16(pdev->config_handle, (off_t)reg);
	return 0;
}

static inline int
pci_read_config_byte(struct pci_dev *pdev, int reg, u8 *val)
{
	*val = pci_config_get8(pdev->config_handle, (off_t)reg);
	return 0;
}

static inline int
pci_write_config_dword(struct pci_dev *pdev, int reg, u32 val)
{
	pci_config_put32(pdev->config_handle, (off_t)reg, val);
	return 0;
}

static inline int
pci_write_config_word(struct pci_dev *pdev, int reg, u16 val)
{
	pci_config_put16(pdev->config_handle, (off_t)reg, val);
	return 0;
}

static inline int
pci_write_config_byte(struct pci_dev *pdev, int reg, u8 val)
{
	pci_config_put8(pdev->config_handle, (off_t)reg, val);
	return 0;
}

/*
 * illumos Phase 1: bus-level config access (arbitrary bus/devfn) is stubbed.
 * These are used for bridge enumeration, not needed for Phase 1.
 */
static inline int
pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn,
    int reg, u16 *val)
{
	*val = 0;
	return -EINVAL;
}

static inline int
pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn,
    int reg, u8 *val)
{
	*val = 0;
	return -EINVAL;
}

static inline int
pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn,
    int reg, u8 val)
{
	return -EINVAL;
}

static inline int
pci_pcie_cap(struct pci_dev *pdev)
{
	int pos;
	if (!__pci_find_capability(pdev->config_handle, PCI_CAP_PCIEXPRESS,
	    &pos))
		return -EINVAL;
	return pos;
}

bool pcie_aspm_enabled(struct pci_dev *);

static inline bool
pci_is_pcie(struct pci_dev *pdev)
{
	return (pci_pcie_cap(pdev) > 0);
}

static inline bool
pci_is_root_bus(struct pci_bus *pbus)
{
	return pbus->is_root;
}

static inline struct pci_dev *
pci_upstream_bridge(struct pci_dev *pdev)
{
	if (pci_is_root_bus(pdev->bus))
		return NULL;
	return pdev->bus->self;
}

/* XXX check for ACPI _PR3 */
static inline bool
pci_pr3_present(struct pci_dev *pdev)
{
	return false;
}

static inline int
pcie_capability_read_dword(struct pci_dev *pdev, int off, u32 *val)
{
	int pos;
	if (!__pci_find_capability(pdev->config_handle, PCI_CAP_PCIEXPRESS,
	    &pos)) {
		*val = 0;
		return -EINVAL;
	}
	*val = pci_config_get32(pdev->config_handle, (off_t)(pos + off));
	return 0;
}

static inline int
pcie_capability_read_word(struct pci_dev *pdev, int off, u16 *val)
{
	int pos;
	if (!__pci_find_capability(pdev->config_handle, PCI_CAP_PCIEXPRESS,
	    &pos)) {
		*val = 0;
		return -EINVAL;
	}
	pci_read_config_word(pdev, pos + off, val);
	return 0;
}

static inline int
pcie_capability_write_word(struct pci_dev *pdev, int off, u16 val)
{
	int pos;
	if (!__pci_find_capability(pdev->config_handle, PCI_CAP_PCIEXPRESS,
	    &pos))
		return -EINVAL;
	pci_write_config_word(pdev, pos + off, val);
	return 0;
}

static inline int
pcie_capability_set_word(struct pci_dev *pdev, int off, u16 val)
{
	u16 r;
	pcie_capability_read_word(pdev, off, &r);
	r |= val;
	pcie_capability_write_word(pdev, off, r);
	return 0;
}

static inline int
pcie_capability_clear_word(struct pci_dev *pdev, int off, u16 c)
{
	u16 r;
	pcie_capability_read_word(pdev, off, &r);
	r &= ~c;
	pcie_capability_write_word(pdev, off, r);
	return 0;
}

static inline int
pcie_capability_clear_and_set_word(struct pci_dev *pdev, int off, u16 c, u16 s)
{
	u16 r;
	pcie_capability_read_word(pdev, off, &r);
	r &= ~c;
	r |= s;
	pcie_capability_write_word(pdev, off, r);
	return 0;
}

static inline int
pcie_get_readrq(struct pci_dev *pdev)
{
	uint16_t val;

	pcie_capability_read_word(pdev, PCI_PCIE_DCSR, &val);

	return 128 << ((val & PCI_PCIE_DCSR_MPS) >> 12);
}

static inline int
pcie_set_readrq(struct pci_dev *pdev, int rrq)
{
	uint16_t val;

	pcie_capability_read_word(pdev, PCI_PCIE_DCSR, &val);
	val &= ~PCI_PCIE_DCSR_MPS;
	val |= (ffs(rrq) - 8) << 12;
	return pcie_capability_write_word(pdev, PCI_PCIE_DCSR, val);
}

static inline void
pci_set_master(struct pci_dev *pdev)
{
}

static inline void
pci_clear_master(struct pci_dev *pdev)
{
}

static inline void
pci_save_state(struct pci_dev *pdev)
{
}

static inline void
pci_restore_state(struct pci_dev *pdev)
{
}

static inline int
pci_enable_msi(struct pci_dev *pdev)
{
	return 0;
}

static inline void
pci_disable_msi(struct pci_dev *pdev)
{
}

typedef enum {
	PCI_D0,
	PCI_D1,
	PCI_D2,
	PCI_D3hot,
	PCI_D3cold
} pci_power_t;

enum pci_bus_speed {
	PCIE_SPEED_2_5GT,
	PCIE_SPEED_5_0GT,
	PCIE_SPEED_8_0GT,
	PCIE_SPEED_16_0GT,
	PCIE_SPEED_32_0GT,
	PCIE_SPEED_64_0GT,
	PCI_SPEED_UNKNOWN
};

enum pcie_link_width {
	PCIE_LNK_X1	= 1,
	PCIE_LNK_X2	= 2,
	PCIE_LNK_X4	= 4,
	PCIE_LNK_X8	= 8,
	PCIE_LNK_X12	= 12,
	PCIE_LNK_X16	= 16,
	PCIE_LNK_X32	= 32,
	PCIE_LNK_WIDTH_UNKNOWN	= 0xff
};

typedef unsigned int pci_ers_result_t;
typedef unsigned int pci_channel_state_t;

#define PCI_ERS_RESULT_DISCONNECT	0
#define PCI_ERS_RESULT_RECOVERED	1

enum pci_bus_speed pcie_get_speed_cap(struct pci_dev *);
enum pcie_link_width pcie_get_width_cap(struct pci_dev *);
int pci_resize_resource(struct pci_dev *, int, int);

static inline void
pcie_bandwidth_available(struct pci_dev *pdev, struct pci_dev **ldev,
    enum pci_bus_speed *speed, enum pcie_link_width *width)
{
	struct pci_dev *bdev = pdev->bus->self;
	if (bdev == NULL)
		return;

	if (speed)
		*speed = pcie_get_speed_cap(bdev);
	if (width)
		*width = pcie_get_width_cap(bdev);
}

static inline int
pci_enable_device(struct pci_dev *pdev)
{
	return 0;
}

static inline void
pci_disable_device(struct pci_dev *pdev)
{
}

static inline int
pci_wait_for_pending_transaction(struct pci_dev *pdev)
{
	return 0;
}

static inline bool
pci_is_thunderbolt_attached(struct pci_dev *pdev)
{
	return false;
}

static inline void
pci_set_drvdata(struct pci_dev *pdev, void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}

static inline void *
pci_get_drvdata(struct pci_dev *pdev)
{
	return dev_get_drvdata(&pdev->dev);
}

static inline int
pci_domain_nr(struct pci_bus *pbus)
{
	return pbus->domain_nr;
}

static inline int
pci_irq_vector(struct pci_dev *pdev, unsigned int num)
{
	return pdev->irq;
}

static inline void
pci_free_irq_vectors(struct pci_dev *pdev)
{
}

/* PCI IRQ vector allocation flags */
#define PCI_IRQ_INTX		(1 << 0)
#define PCI_IRQ_MSI		(1 << 1)
#define PCI_IRQ_MSIX		(1 << 2)
#define PCI_IRQ_ALL_TYPES	(PCI_IRQ_INTX | PCI_IRQ_MSI | PCI_IRQ_MSIX)

static inline int
pci_alloc_irq_vectors(struct pci_dev *pdev, unsigned int min_vecs,
    unsigned int max_vecs, unsigned int flags)
{
	/* Phase 1 stub: single INTx vector */
	return 1;
}

static inline int
pci_set_power_state(struct pci_dev *dev, int state)
{
	return 0;
}

struct pci_driver;

static inline int
pci_register_driver(struct pci_driver *pci_drv)
{
	return 0;
}

static inline void
pci_unregister_driver(void *d)
{
}

static inline u16
pci_dev_id(struct pci_dev *dev)
{
	return dev->devfn | (dev->bus->number << 8);
}

static inline const struct pci_device_id *
pci_match_id(const struct pci_device_id *ids, struct pci_dev *pdev)
{
	int i = 0;

	for (i = 0; ids[i].vendor != 0; i++) {
		if ((ids[i].vendor == pdev->vendor) &&
		    (ids[i].device == pdev->device ||
		     ids[i].device == PCI_ANY_ID) &&
		    (ids[i].subvendor == PCI_ANY_ID) &&
		    (ids[i].subdevice == PCI_ANY_ID))
			return &ids[i];
	}
	return NULL;
}

static inline int
pci_device_is_present(struct pci_dev *pdev)
{
	return 1;
}

static inline int
dev_is_pci(struct device *dev)
{
	return 1;
}

struct pci_driver {
	const char *name;
	const struct pci_device_id *id_table;
	int (*probe)(struct pci_dev *, const struct pci_device_id *);
	void (*remove)(struct pci_dev *);
	int (*resume)(struct pci_dev *);
	int (*suspend)(struct pci_dev *);
	struct device_driver driver;
};

static inline int pcim_enable_device(struct pci_dev *pdev) { return 0; }

/*
 * Region management stubs — illumos DDI handles resource allocation.
 */
static inline int
pci_request_regions(struct pci_dev *pdev, const char *name)
{
	return 0;	/* illumos DDI manages resources */
}

static inline void
pci_release_regions(struct pci_dev *pdev)
{
}

static inline int
pci_request_region(struct pci_dev *pdev, int bar, const char *name)
{
	return 0;
}

static inline void
pci_release_region(struct pci_dev *pdev, int bar)
{
}

/*
 * devm_ioremap / devm_memremap — device-managed MMIO mapping.
 * On illumos we use memremap from linux/io.h (which is a stub for now).
 */
#include <linux/io.h>

static inline void *
devm_ioremap(struct device *dev, resource_size_t offset, resource_size_t size)
{
	return memremap(offset, size, MEMREMAP_WB);
}

static inline void *
devm_memremap(struct device *dev, resource_size_t offset, resource_size_t size,
    unsigned long flags)
{
	return memremap(offset, size, flags);
}

static inline void *
devm_ioremap_wc(struct device *dev, resource_size_t offset,
    resource_size_t size)
{
	return memremap(offset, size, MEMREMAP_WB);
}

static inline void
pci_iounmap(struct pci_dev *pdev, void __iomem *addr)
{
	memunmap(addr);
}

#endif /* _LINUX_PCI_H_ */
