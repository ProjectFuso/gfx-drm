/* Public domain. */

#ifndef _LINUX_TIMEKEEPING_H
#define _LINUX_TIMEKEEPING_H

static inline time_t
ktime_get_real_seconds(void)
{
	/* illumos: gethrestime_sec() returns current wall-clock time in seconds */
	return gethrestime_sec();
}

static inline ktime_t
ktime_get_real(void)
{
	/* illumos: gethrestime() fills a timespec with wall-clock time */
	timespec_t ts;
	gethrestime(&ts);
	return (ktime_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

static inline uint64_t
ktime_get_ns(void)
{
	return ktime_get();
}

static inline ktime_t
ktime_get_boottime(void)
{
	return ktime_get();
}

static inline uint64_t
ktime_get_boottime_ns(void)
{
	return ktime_get_ns();
}

#endif
