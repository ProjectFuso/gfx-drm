/* Public domain. */

#ifndef _LINUX_COMPILER_H
#define _LINUX_COMPILER_H

#include <linux/kconfig.h>

/*
 * READ_ONCE / WRITE_ONCE: volatile-cast barrier semantics.
 * illumos sys/atomic.h doesn't define these; define them here.
 */
#ifndef READ_ONCE
#define READ_ONCE(x)		(*(volatile __typeof__(x) *)&(x))
#endif
#ifndef WRITE_ONCE
#define WRITE_ONCE(x, val)	(*(volatile __typeof__(x) *)&(x) = (val))
#endif

#define unlikely(x)	__builtin_expect(!!(x), 0)
#define likely(x)	__builtin_expect(!!(x), 1)

#define __force
#define __acquires(x)
#define __releases(x)
#define __read_mostly
#define __iomem
#define __must_check
#define __init
#define __exit
#define __deprecated
#define __nonstring
#define __always_unused	__attribute__((__unused__))
#ifndef __maybe_unused
#define __maybe_unused	__attribute__((__unused__))
#endif
#define __always_inline	inline __attribute__((__always_inline__))
#define noinline	__attribute__((__noinline__))
#define noinline_for_stack	 __attribute__((__noinline__))
#define fallthrough	do {} while (0)
#define __counted_by(x)
#define __cleanup(fn)	__attribute__((__cleanup__(fn)))

#define __PASTE(x,y) x##y

#ifndef __user
#define __user
#endif

#define barrier()	__asm volatile("" : : : "memory")

/*
 * illumos: -fno-asm (used by the kernel build system) disables the 'typeof'
 * keyword.  Map it to the always-available __typeof__ so upstream code that
 * uses typeof() compiles unmodified.
 */
#ifndef typeof
#define typeof	__typeof__
#endif
/* -fno-asm also disables the 'asm' keyword; map to __asm__ */
#ifndef asm
#define asm	__asm__
#endif

#define __malloc	__attribute__((__malloc__))
#define __printf(x, y)	__attribute__((__format__(__printf__,x,y)))

/* The Linux code doesn't meet our usual standards! */
#ifdef __clang__
#pragma clang diagnostic ignored "-Winitializer-overrides"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wgnu-variable-sized-type-not-at-end"
#else
#pragma GCC diagnostic ignored "-Wformat-zero-length"
#endif

#define __diag_push()
#define __diag_ignore_all(x, y)
#define __diag_pop()

#endif
