/* Public domain. */

#ifndef _LINUX_JIFFIES_H
#define _LINUX_JIFFIES_H

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/limits.h>
#include <sys/ddi.h>
#include <linux/illumos_page_compat.h>
#include <sys/sunddi.h>

/*
 * In illumos, there is no global jiffies variable.
 * ddi_get_lbolt() returns the current lbolt (clock_t, in ticks).
 */
#define jiffies		((unsigned long)ddi_get_lbolt())
#define jiffies_64	((uint64_t)ddi_get_lbolt())

#undef HZ
#define HZ	drv_usectohz(1000000)

#define MAX_JIFFY_OFFSET	((INT_MAX >> 1) - 1)

#define time_in_range(x, min, max) ((x) >= (min) && (x) <= (max))

static inline unsigned int
jiffies_to_msecs(const unsigned long x)
{
	return (unsigned int)(drv_hztousec(x) / 1000);
}

static inline unsigned int
jiffies_to_usecs(const unsigned long x)
{
	return (unsigned int)drv_hztousec(x);
}

static inline uint64_t
jiffies_to_nsecs(const unsigned long x)
{
	return (uint64_t)drv_hztousec(x) * 1000;
}

#define msecs_to_jiffies(x)	drv_usectohz((clock_t)(x) * 1000)
#define usecs_to_jiffies(x)	drv_usectohz((clock_t)(x))
#define nsecs_to_jiffies(x)	drv_usectohz((clock_t)((x) / 1000))
#define nsecs_to_jiffies64(x)	((uint64_t)drv_usectohz((clock_t)((x) / 1000)))

static inline uint64_t
get_jiffies_64(void)
{
	return (uint64_t)ddi_get_lbolt();
}

static inline int
time_after(const unsigned long a, const unsigned long b)
{
	return (long)(b - a) < 0;
}
#define time_before(a, b)	time_after(b, a)

static inline int
time_after_eq(const unsigned long a, const unsigned long b)
{
	return (long)(b - a) <= 0;
}

static inline int
time_after_eq64(const unsigned long long a, const unsigned long long b)
{
	return (long long)(b - a) <= 0;
}

#define time_after32(a, b)	((int32_t)((uint32_t)(b) - (uint32_t)(a)) < 0)

#endif
