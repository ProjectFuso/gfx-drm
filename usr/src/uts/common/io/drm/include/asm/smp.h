/* Public domain. */

#ifndef _ASM_SMP_H
#define _ASM_SMP_H

#if defined(__i386__) || defined(__amd64__)
/*
 * illumos: wbinvd_on_all_cpus() — write-back and invalidate CPU caches on
 * all CPUs.  On illumos there is no direct cross-call for WBINVD; use the
 * local instruction.  Full cross-CPU cache flush deferred to Phase 2.
 */
static inline void
wbinvd_on_all_cpus(void)
{
	__asm__ __volatile__("wbinvd" : : : "memory");
}
#endif /* __i386__ || __amd64__ */

#endif /* _ASM_SMP_H */
