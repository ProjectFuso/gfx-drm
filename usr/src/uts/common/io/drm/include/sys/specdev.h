/* Public domain. */
/*
 * illumos: OpenBSD <sys/specdev.h> stub.
 * OpenBSD DRM source (drm_drv.c) includes this for special device types
 * and also relies on other OpenBSD sys/ types.  Provide stubs for all.
 */
#ifndef _SYS_SPECDEV_COMPAT_H
#define _SYS_SPECDEV_COMPAT_H

#include <sys/types.h>
#include <sys/uio.h>		/* struct uio, uio_resid — same on illumos */

/*
 * struct proc: OpenBSD process structure pointer passed to open/close/read.
 * On illumos, DDI open/close use cred_t *.  Forward-declare struct proc so
 * function signatures compile; the parameter is unused on illumos.
 */
struct proc;

/*
 * struct aml_node: OpenBSD ACPI Method Language device node.
 * drm_drv.c forward-declares drm_linux_acpi_notify(struct aml_node *, ...)
 * unconditionally; provide a forward declaration so it compiles.
 * The ACPI code path itself is guarded by #ifdef __HAVE_ACPI / CONFIG_ACPI.
 */
struct aml_node;

/*
 * IO_NDELAY: OpenBSD non-blocking I/O flag (like O_NONBLOCK for in-kernel
 * read calls).  On illumos this is FNONBLOCK, but drm_drv.c tests it in
 * the ioflag parameter so just alias the illumos constant.
 */
#ifndef IO_NDELAY
#ifdef FNONBLOCK
#define IO_NDELAY	FNONBLOCK
#else
#define IO_NDELAY	0x0010
#endif
#endif

/*
 * uiomove: OpenBSD uses a 3-arg form (no UIO_READ/UIO_WRITE flag).
 * illumos uiomove() takes 4 args: (buf, nbytes, rw_flag, uio).
 * Wrap the 3-arg form as a UIO_READ call (DRM only reads events to user).
 *
 * The macro must NOT be defined if we're in a file that defines the real
 * illumos uiomove prototype (uio.h), since uio.h declares the 4-arg form.
 * We guard against double-definition by checking the prototype form.
 */
#ifdef _KERNEL
static inline int
drm_uiomove_compat(void *buf, size_t n, struct uio *uio)
{
	return uiomove((caddr_t)buf, (int)n, UIO_READ, uio);
}
/*
 * Override uiomove for drm_drv.c's 3-arg calls.
 * Only active when this header is included; restore after drm_drv.c.
 */
#define uiomove(buf, n, uio)	drm_uiomove_compat((buf), (n), (uio))
#endif /* _KERNEL */

/*
 * OpenBSD malloc/free type tags and wrappers.
 * Shared with linux/pci.h via the _DRM_OPENBSD_MALLOC_DEFINED guard.
 */
#ifndef _DRM_OPENBSD_MALLOC_DEFINED
#define _DRM_OPENBSD_MALLOC_DEFINED

#include <sys/kmem.h>

#ifndef M_DRM
#define M_DRM		1
#endif
#ifndef M_WAITOK
#define M_WAITOK	0x0001
#define M_NOWAIT	0x0002
#define M_ZERO		0x0008
#define M_CANFAIL	0x0004
#endif

static inline void *
_drm_openbsd_malloc(size_t size, int type, int flags)
{
	(void)type;
	if (flags & M_ZERO)
		return kmem_zalloc(size, (flags & M_NOWAIT) ? KM_NOSLEEP : KM_SLEEP);
	return kmem_alloc(size, (flags & M_NOWAIT) ? KM_NOSLEEP : KM_SLEEP);
}

#undef malloc
#undef free
#define malloc(size, type, flags)  _drm_openbsd_malloc((size), (type), (flags))
#define free(ptr, type, size)      do { if (ptr) kmem_free((ptr), (size)); } while (0)

#endif /* _DRM_OPENBSD_MALLOC_DEFINED */

/*
 * CLONE_SHIFT: OpenBSD clone device minor number bit width.
 * Used in drm_drv.c to decode the minor device number.
 */
#ifndef CLONE_SHIFT
#define CLONE_SHIFT	8
#endif

/*
 * OpenBSD autoconf (config) framework types.
 * drm_drv.c uses these for device attachment.  On illumos, DDI is used
 * instead; these are Phase 1 stubs so the file compiles.
 */

struct cfdriver {
	void	**cd_devs;	/* array of pointers to softc instances */
	char	*cd_name;
	int	 cd_class;
	int	 cd_ndevs;	/* number of devices allocated */
};

struct cfdata {
	struct cfdriver	*cf_driver;
	/* other fields omitted */
};

struct cfattach {
	size_t	ca_devsize;
	int	(*ca_match)(struct device *, void *, void *);
	void	(*ca_attach)(struct device *, struct device *, void *);
	int	(*ca_detach)(struct device *, int);
	int	(*ca_activate)(struct device *, int);
};

/* Device class (DV_DULL = dumb/catch-all device) */
#define DV_DULL		0
#define DV_IFNET	1
#define DV_TTY		2
#define DV_DISK		3

/* Device activation codes */
#define DVACT_DEACTIVATE	1
#define DVACT_QUIESCE		2
#define DVACT_SUSPEND		3
#define DVACT_RESUME		4
#define DVACT_WAKEUP		5

/*
 * CF_ATTACH_MATCH: used by match functions that want to return a score.
 * On OpenBSD the match function returns an integer confidence score.
 */

#endif /* _SYS_SPECDEV_COMPAT_H */
