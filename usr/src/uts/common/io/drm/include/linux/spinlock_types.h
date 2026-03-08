/* Public domain. */

#ifndef _LINUX_SPINLOCK_TYPES_H
#define _LINUX_SPINLOCK_TYPES_H

#include <sys/mutex.h>
#include <linux/rwlock_types.h>

typedef kmutex_t spinlock_t;
/* DEFINE_SPINLOCK: must call spin_lock_init() before use (no static initializer in illumos) */
#define DEFINE_SPINLOCK(x)	kmutex_t x

struct raw_spinlock {
};

#endif
