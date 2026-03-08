/* Public domain. */

#ifndef _LINUX_PROCESSOR_H
#define _LINUX_PROCESSOR_H

#include <sys/param.h>
#include <linux/jiffies.h>

/*
 * CPU_BUSY_CYCLE: on x86, issue a PAUSE instruction (spin-loop hint).
 * On other ISAs it's a no-op.
 */
#if defined(__i386) || defined(__amd64)
#define CPU_BUSY_CYCLE()	__asm __volatile("pause" ::: "memory")
#else
#define CPU_BUSY_CYCLE()	__asm __volatile("" ::: "memory")
#endif

static inline void
cpu_relax(void)
{
	CPU_BUSY_CYCLE();
}

#ifndef CACHELINESIZE
#define CACHELINESIZE 64
#endif

#endif
