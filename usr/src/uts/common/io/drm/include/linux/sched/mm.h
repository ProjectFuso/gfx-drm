/* Public domain. */

#ifndef _LINUX_SCHED_MM_H
#define _LINUX_SCHED_MM_H

#include <sys/types.h>
#include <sys/systm.h>

/*
 * might_alloc: Linux allocation context check — no-op on illumos.
 * On Linux this asserts that the calling context allows sleeping allocations.
 * On illumos the kernel allocation subsystem handles context checks internally.
 */
static inline void
might_alloc(const unsigned int flags)
{
}

#endif
