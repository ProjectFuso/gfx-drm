/* Public domain. */
/*
 * illumos: linux/compat.h — 32-bit compat layer stubs.
 *
 * On Linux, this header provides compat_ptr() (convert 32-bit uptr to 64-bit
 * pointer) and clear_user() (zero userspace memory).  On illumos, 32-bit
 * compat is handled via _SYSCALL32; provide minimal stubs so drm_ioc32.c
 * compiles for Phase 1.
 */
#ifndef _LINUX_COMPAT_H
#define _LINUX_COMPAT_H

#include <linux/compiler.h>	/* for __user (defined as empty) */
#include <linux/types.h>

/*
 * compat_uptr_t: 32-bit userspace pointer type.
 */
typedef uint32_t compat_uptr_t;
typedef uint32_t compat_uint_t;
typedef int32_t  compat_int_t;
typedef uint64_t compat_u64;
typedef int64_t  compat_s64;
typedef uint32_t compat_ulong_t;
typedef int32_t  compat_long_t;

/*
 * compat_ptr: zero-extend a 32-bit userspace pointer to a native pointer.
 */
static inline void *
compat_ptr(compat_uptr_t uptr)
{
	return (void *)(uintptr_t)uptr;
}

static inline compat_uptr_t
ptr_to_compat(void *uptr)
{
	return (compat_uptr_t)(uintptr_t)uptr;
}

/*
 * clear_user: zero nbytes of userspace memory at [to, to+nbytes).
 * Returns 0 on success, number of bytes not cleared on fault.
 * Phase 1 stub: use copyout with zeros.
 */
#ifdef _KERNEL
#include <sys/types.h>
#include <sys/systm.h>
static inline unsigned long
clear_user(void *to, unsigned long nbytes)
{
	static const char _zero[64];
	unsigned long left = nbytes;
	while (left > 0) {
		unsigned long n = left < sizeof(_zero) ? left : sizeof(_zero);
		if (copyout(_zero, to, n) != 0)
			return left;
		to = (char *)to + n;
		left -= n;
	}
	return 0;
}
#endif /* _KERNEL */

#endif /* _LINUX_COMPAT_H */
