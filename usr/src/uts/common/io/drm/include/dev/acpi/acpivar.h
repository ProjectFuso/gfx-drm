/* Public domain. */
/*
 * illumos: OpenBSD <dev/acpi/acpivar.h> stub.
 * Provides struct acpi_softc and ACPI power state constants.
 * Actual ACPI usage in DRM is guarded by #ifdef CONFIG_ACPI.
 */
#ifndef _DEV_ACPI_ACPIVAR_COMPAT_H
#define _DEV_ACPI_ACPIVAR_COMPAT_H

/* ACPI global state machine states (OpenBSD naming) */
#define ACPI_STATE_S0	0	/* running */
#define ACPI_STATE_S1	1	/* sleeping, CPU and RAM powered */
#define ACPI_STATE_S3	3	/* suspend-to-RAM */
#define ACPI_STATE_S4	4	/* suspend-to-disk */
#define ACPI_STATE_S5	5	/* soft off */

struct acpi_softc {
	int	sc_state;	/* current ACPI state */
};

/* acpi_softc: global pointer to the ACPI subsystem (NULL on illumos) */
extern struct acpi_softc *acpi_softc;

#endif /* _DEV_ACPI_ACPIVAR_COMPAT_H */
