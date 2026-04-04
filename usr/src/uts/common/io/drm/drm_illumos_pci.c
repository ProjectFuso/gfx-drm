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

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/pci.h>

#include <drm/drm_illumos.h>
#include <linux/pci.h>

static void
drm_illumos_pci_read_bars(struct pci_dev *pdev, ddi_acc_handle_t cfg_handle)
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
			pdev->resource[i].end = base + size - 1;
			pdev->resource[i].flags = IORESOURCE_IO;
		} else {
			uint8_t type = (uint8_t)((bar_lo >> 1) & 0x3);

			if (type == 2 && i < 5) {
				uint32_t bar_hi, saved_lo, saved_hi;
				uint32_t mask_lo, mask_hi;
				uint64_t full_base, full_mask, size;

				bar_hi = pci_config_get32(cfg_handle, bar_offsets[i + 1]);
				full_base = ((uint64_t)bar_hi << 32) |
				    (uint64_t)(bar_lo & ~0xFU);

				saved_lo = bar_lo;
				saved_hi = bar_hi;
				pci_config_put32(cfg_handle, bar_offsets[i], 0xFFFFFFFFU);
				pci_config_put32(cfg_handle, bar_offsets[i + 1],
				    0xFFFFFFFFU);
				mask_lo = pci_config_get32(cfg_handle, bar_offsets[i]);
				mask_hi = pci_config_get32(cfg_handle, bar_offsets[i + 1]);
				pci_config_put32(cfg_handle, bar_offsets[i], saved_lo);
				pci_config_put32(cfg_handle, bar_offsets[i + 1], saved_hi);

				full_mask = ((uint64_t)mask_hi << 32) |
				    (uint64_t)(mask_lo & ~0xFU);
				size = ~full_mask + 1ULL;
				if (size == 0)
					size = 4096;

				pdev->resource[i].start = (resource_size_t)full_base;
				pdev->resource[i].end =
				    (resource_size_t)(full_base + size - 1);
				pdev->resource[i].flags = IORESOURCE_MEM | IORESOURCE_MEM_64;

				i++;
			} else {
				uint32_t saved, mask;
				resource_size_t base, size;

				base = (resource_size_t)(bar_lo & ~0xFU);

				saved = bar_lo;
				pci_config_put32(cfg_handle, bar_offsets[i], 0xFFFFFFFFU);
				mask = pci_config_get32(cfg_handle, bar_offsets[i]);
				pci_config_put32(cfg_handle, bar_offsets[i], saved);

				size = (resource_size_t)((~(mask & ~0xFU) + 1U) &
				    0xFFFFFFFFU);
				if (size == 0)
					size = 4096;

				pdev->resource[i].start = base;
				pdev->resource[i].end = base + size - 1;
				pdev->resource[i].flags = IORESOURCE_MEM;
			}
		}
	}
}

void
drm_illumos_pci_init_device(struct pci_dev *pdev, dev_info_t *dip,
    ddi_acc_handle_t cfg_handle)
{
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

	drm_illumos_pci_read_bars(pdev, cfg_handle);

	pdev->dev.dip = dip;
	pdev->dev.pdev = pdev;
}
