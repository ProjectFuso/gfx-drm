/* Public domain. */

#ifndef _LINUX_SPINLOCK_H
#define _LINUX_SPINLOCK_H

#include <linux/spinlock_types.h>
#include <linux/preempt.h>
#include <linux/bottom_half.h>
#include <linux/atomic.h>
#include <linux/lockdep.h>

/*
 * illumos: struct mutex = kmutex_t (from sys/mutex.h via spinlock_types.h).
 * Forward-declare so atomic_dec_and_lock can accept struct mutex * parameter
 * even when linux/mutex.h hasn't finished processing yet.
 */
struct mutex;

#define spin_lock_init(l)	mutex_init((l), NULL, MUTEX_DRIVER, NULL)
#define spin_lock_destroy(l)	mutex_destroy(l)

#define spin_lock_irqsave(_mtxp, _flags) do {		\
		_flags = 0;				\
		mutex_enter(_mtxp);			\
	} while (0)

#define spin_lock_irqsave_nested(_mtxp, _flags, _subclass) do {	\
		(void)(_subclass);			\
		_flags = 0;				\
		mutex_enter(_mtxp);			\
	} while (0)

#define spin_unlock_irqrestore(_mtxp, _flags) do {	\
		(void)(_flags);				\
		mutex_exit(_mtxp);			\
	} while (0)

#define spin_trylock(_mtxp)	(mutex_tryenter(_mtxp) ? 1 : 0)

#define spin_trylock_irqsave(_mtxp, _flags)		\
({							\
	(void)(_flags);					\
	mutex_tryenter(_mtxp) ? 1 : 0;			\
})

static inline int
atomic_dec_and_lock(volatile int *v, struct mutex *mtxp)
{
	if (*v != 1) {
		atomic_dec(v);
		return 0;
	}
	/* struct mutex == kmutex_t; use mutex_enter (available from sys/mutex.h) */
	mutex_enter((kmutex_t *)mtxp);
	atomic_dec(v);
	return 1;
}

#define atomic_dec_and_lock_irqsave(_a, _mtxp, _flags)	\
	atomic_dec_and_lock(_a, _mtxp)

#define spin_lock(mtxp)			mutex_enter(mtxp)
#define spin_lock_nested(mtxp, l)	mutex_enter(mtxp)
#define spin_unlock(mtxp)		mutex_exit(mtxp)
#define spin_lock_irq(mtxp)		mutex_enter(mtxp)
#define spin_unlock_irq(mtxp)		mutex_exit(mtxp)
#define assert_spin_locked(mtxp)	ASSERT(MUTEX_HELD(mtxp))
#define spin_trylock_irq(mtxp)		mutex_tryenter(mtxp)

#include <linux/rwlock.h>

#endif
