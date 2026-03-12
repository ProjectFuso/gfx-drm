/* Public domain. */

#ifndef _ASM_CPUFEATURE_H
#define _ASM_CPUFEATURE_H

#include <stdbool.h>

#if defined(__amd64__) || defined(__i386__)

/*
 * illumos: CPU feature detection using safe static defaults.
 * OpenBSD used cpu_ecxfeature/curcpu()->ci_* which don't exist on illumos.
 * For Phase 1 use conservative values: amd64 always has CLFLUSH and PAT;
 * SSE4.1 and HYPERVISOR are left as false (DRM will use fallback paths).
 */

#define X86_FEATURE_CLFLUSH	1
#define X86_FEATURE_XMM4_1	2
#define X86_FEATURE_PAT		3
#define X86_FEATURE_HYPERVISOR	4

static inline bool
static_cpu_has(uint16_t f)
{
	switch (f) {
	case X86_FEATURE_XMM4_1:
		return false;		/* conservative: use SW fallback */
#ifdef __amd64__
	case X86_FEATURE_CLFLUSH:
	case X86_FEATURE_PAT:
		return true;		/* amd64 always has CLFLUSH and PAT */
#else
	case X86_FEATURE_CLFLUSH:
	case X86_FEATURE_PAT:
		return false;
#endif
	case X86_FEATURE_HYPERVISOR:
		return false;		/* conservative */
	default:
		return false;
	}
}

static inline bool
pat_enabled(void)
{
	return static_cpu_has(X86_FEATURE_PAT);
}

#define boot_cpu_has(x) static_cpu_has(x)

static inline void
clflushopt(volatile void *addr)
{
	/* illumos: always use clflush (safe fallback; no curcpu() available) */
	__asm volatile("clflush %0" : "+m" (*(volatile char *)addr));
}

#endif

#endif
