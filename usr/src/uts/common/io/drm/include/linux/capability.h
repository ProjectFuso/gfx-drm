/* Public domain. */

#ifndef _LINUX_CAPABILITY_H
#define _LINUX_CAPABILITY_H

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <linux/illumos_page_compat.h>
#include <sys/sunddi.h>
#include <sys/cred.h>

#define CAP_SYS_ADMIN	0x1
#define CAP_SYS_NICE	0x2

/*
 * capable — illumos equivalent of Linux's capable(cap).
 *
 * CAP_SYS_ADMIN and CAP_SYS_NICE map to drv_priv(ddi_get_cred()) == 0,
 * which checks that the calling thread has the PRIV_SYS_DEVICES privilege
 * (effectively: the process is running as root or has been granted the
 * privilege explicitly).  This is the correct illumos equivalent of
 * CAP_SYS_ADMIN for kernel driver permission checks.
 *
 * All other capabilities default to false (deny), which is safe.
 */
static inline bool
capable(int cap)
{
	switch (cap) {
	case CAP_SYS_ADMIN:
	case CAP_SYS_NICE:
		return (drv_priv(ddi_get_cred()) == 0);
	default:
		return false;
	}
}

static inline bool
perfmon_capable(void)
{
	return false;
}

#endif
