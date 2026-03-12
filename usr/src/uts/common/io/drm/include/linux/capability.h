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

static inline bool
capable(int cap)
{
	/* illumos: check privileges using DDI credential check */
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
