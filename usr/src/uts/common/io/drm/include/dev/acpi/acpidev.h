/* Public domain. */
/*
 * illumos: OpenBSD <dev/acpi/acpidev.h> stub.
 * Provides ACPI device constants used by DRM OpenBSD code.
 */
#ifndef _DEV_ACPI_ACPIDEV_COMPAT_H
#define _DEV_ACPI_ACPIDEV_COMPAT_H

#include <dev/acpi/dsdt.h>

/* ACPI find PCI function — stub, returns NULL */
struct pci_attach_args;
static inline struct aml_node *
acpi_find_pci(void *pc, int tag)
{
	(void)pc; (void)tag;
	return NULL;
}

#endif /* _DEV_ACPI_ACPIDEV_COMPAT_H */
