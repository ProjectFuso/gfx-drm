/* Public domain. */

#ifndef _ASM_CACHEFLUSH_H
#define _ASM_CACHEFLUSH_H

#if defined(__i386__) || defined(__amd64__)
/*
 * illumos: clflush_cache_range — Phase 1 stub.
 * Full implementation deferred to Phase 2 (HAT-level cache attribute mgmt).
 */
#define clflush_cache_range(va, len)	do { } while (0)

#endif

#endif
