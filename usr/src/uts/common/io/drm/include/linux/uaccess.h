/*	$OpenBSD: uaccess.h,v 1.7 2022/02/01 04:09:14 jsg Exp $	*/
/*
 * Copyright (c) 2015 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _LINUX_UACCESS_H
#define _LINUX_UACCESS_H

#include <sys/param.h>
#include <sys/systm.h>
/* illumos: uvm/uvm_extern.h removed */

#include <linux/sched.h>

/*
 * copyout/copyin are available in the illumos kernel without extra includes.
 * illumos copyout(from_kva, to_uva, n) / copyin(from_uva, to_kva, n).
 */

static inline unsigned long
__copy_to_user(void *to, const void *from, unsigned long len)
{
	if (copyout(from, to, len))
		return len;
	return 0;
}

static inline unsigned long
copy_to_user(void *to, const void *from, unsigned long len)
{
	return __copy_to_user(to, from, len);
}

static inline unsigned long
__copy_from_user(void *to, const void *from, unsigned long len)
{
	if (copyin(from, to, len))
		return len;
	return 0;
}

static inline unsigned long
copy_from_user(void *to, const void *from, unsigned long len)
{
	return __copy_from_user(to, from, len);
}

#define get_user(x, ptr)	-copyin(ptr, &(x), sizeof(x))
#define put_user(x, ptr) ({				\
	__typeof((x)) __tmp = (x);			\
	-copyout(&(__tmp), ptr, sizeof(__tmp));		\
})
#define __get_user(x, ptr)	get_user((x), (ptr))
#define __put_user(x, ptr)	put_user((x), (ptr))

#define unsafe_put_user(x, ptr, err) ({				\
	__typeof((x)) __tmp = (x);				\
	if (copyout(&(__tmp), ptr, sizeof(__tmp)) != 0)		\
		goto err;					\
})

/*
 * illumos: access_ok() stub — always returns 1.
 * Proper user-address range checking deferred to Phase 2.
 */
static inline int
access_ok(const void *addr, unsigned long size)
{
	return 1;
}

#define user_access_begin(addr, size)	access_ok(addr, size)
#define user_access_end()

#define user_write_access_begin(addr, size)	access_ok(addr, size)
#define user_write_access_end()

#if defined(__i386__) || defined(__amd64__)

/*
 * illumos: pagefault_disable/enable are no-op stubs.
 * OpenBSD uses ci_inatomic; illumos has no direct equivalent.
 */
static inline void
pagefault_disable(void)
{
}

static inline void
pagefault_enable(void)
{
}

static inline int
pagefault_disabled(void)
{
	return 0;
}

static inline unsigned long
__copy_to_user_inatomic(void *to, const void *from, unsigned long len)
{
	if (copyout(from, to, len))
		return len;
	return 0;
}

static inline unsigned long
__copy_from_user_inatomic(void *to, const void *from, unsigned long len)
{
	if (copyin(from, to, len))
		return len;
	return 0;
}

static inline unsigned long
__copy_from_user_inatomic_nocache(void *to, const void *from, unsigned long len)
{
	return __copy_from_user_inatomic(to, from, len);
}

#endif /* defined(__i386__) || defined(__amd64__) */

#endif
