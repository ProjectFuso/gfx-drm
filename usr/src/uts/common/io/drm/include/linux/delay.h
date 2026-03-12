/* Public domain. */

#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

#include <sys/ddi.h>
#include <linux/illumos_page_compat.h>
#include <sys/sunddi.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysmacros.h>	/* MAX() */

static inline void
udelay(unsigned long usecs)
{
	drv_usecwait(usecs);
}

static inline void
ndelay(unsigned long nsecs)
{
	drv_usecwait(MAX(nsecs / 1000, 1));
}

static inline void
usleep_range(unsigned long min, unsigned long max)
{
	drv_usecwait((min + max) / 2);
}

static inline void
mdelay(unsigned long msecs)
{
	drv_usecwait(msecs * 1000);
}

#define drm_msleep(x)		mdelay(x)

static inline void
fsleep(unsigned long usecs)
{
	drv_usecwait(usecs);
}

static inline unsigned int
msleep_interruptible(unsigned int msecs)
{
	delay(drv_usectohz((clock_t)msecs * 1000));
	return 0;
}

#endif
