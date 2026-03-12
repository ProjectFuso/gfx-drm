/* Public domain. */
/*
 * illumos: OpenBSD <dev/acpi/dsdt.h> stub.
 * DRM unconditionally includes this for struct aml_node / aml_register_notify.
 * The actual usage is guarded by #ifdef CONFIG_ACPI which we don't define.
 * These stubs satisfy the compiler.
 */
#ifndef _DEV_ACPI_DSDT_COMPAT_H
#define _DEV_ACPI_DSDT_COMPAT_H

struct aml_node {
	int	_pad;
};

/* aml_register_notify: Phase 1 stub (no ACPI on illumos DRM) */
#define ACPIDEV_NOPOLL	0

static inline int
aml_register_notify(struct aml_node *node, const char *pnpid,
    int (*notify)(struct aml_node *, int, void *), void *arg, int poll)
{
	(void)node; (void)pnpid; (void)notify; (void)arg; (void)poll;
	return 0;
}

#endif /* _DEV_ACPI_DSDT_COMPAT_H */
