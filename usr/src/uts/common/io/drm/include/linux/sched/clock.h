/* Public domain. */

#ifndef _LINUX_SCHED_CLOCK_H
#define _LINUX_SCHED_CLOCK_H

#include <sys/types.h>

#include <linux/time.h>
#include <linux/smp.h>

static inline uint64_t
local_clock(void)
{
	/* illumos: gethrtime() returns nanoseconds since boot */
	return (uint64_t)gethrtime();
}

#endif
