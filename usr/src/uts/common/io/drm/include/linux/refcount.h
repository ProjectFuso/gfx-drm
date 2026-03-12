/* Public domain. */

#ifndef _LINUX_REFCOUNT_H
#define _LINUX_REFCOUNT_H

#include <sys/types.h>
#include <linux/atomic.h>
#include <linux/mutex.h>		/* struct mutex, mutex_lock, mutex_unlock */

typedef atomic_t refcount_t;

static inline bool
refcount_dec_and_test(uint32_t *p)
{
	return atomic_dec_and_test(p);
}

static inline bool
refcount_inc_not_zero(uint32_t *p)
{
	/* atomic_inc_not_zero expects volatile int *; cast is safe */
	return atomic_inc_not_zero((volatile int *)p);
}

static inline void
refcount_set(uint32_t *p, int v)
{
	atomic_set(p, v);
}

static inline bool
refcount_dec_and_lock_irqsave(volatile int *v, struct mutex *lock,
    unsigned long *flags)
{
	if (atomic_add_unless(v, -1, 1))
		return false;

	mutex_lock(lock);
	if (atomic_dec_return(v) == 0)
		return true;
	mutex_unlock(lock);
	return false;
}

static inline uint32_t
refcount_read(uint32_t *p)
{
	return atomic_read(p);
}

#endif
