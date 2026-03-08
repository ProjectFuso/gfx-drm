/* Public domain. */

#ifndef _LINUX_MUTEX_H
#define _LINUX_MUTEX_H

#include <sys/rwlock.h>
#include <linux/list.h>
#include <linux/spinlock_types.h>
#include <linux/lockdep.h>
#include <linux/rwlock.h>	/* struct rwlock (same layout as struct mutex) */

/*
 * Linux struct mutex maps to illumos krwlock_t wrapped in a struct.
 * illumos has no static initializer for rwlocks; callers must use
 * mutex_init() (which calls rw_init internally) before use.
 * DEFINE_MUTEX instances used at file scope must be explicitly
 * initialized in the module _init() function.
 */
struct mutex {
	krwlock_t	rw;
};

#define DEFINE_MUTEX(x)		struct mutex x

static inline void
mutex_init_ll(struct mutex *m)
{
	rw_init(&m->rw, NULL, RW_DEFAULT, NULL);
}

static inline void
mutex_destroy(struct mutex *m)
{
	rw_destroy(&m->rw);
}

static inline void
mutex_lock(struct mutex *m)
{
	rw_enter(&m->rw, RW_WRITER);
}

#define mutex_lock_nest_lock(m, sub)	mutex_lock(m)
#define mutex_lock_nested(m, sub)	mutex_lock(m)
#define mutex_lock_interruptible_nested(m, subc) \
					mutex_lock_interruptible(m)

static inline void
mutex_unlock(struct mutex *m)
{
	rw_exit(&m->rw);
}

static inline int
mutex_trylock(struct mutex *m)
{
	return rw_tryenter(&m->rw, RW_WRITER);
}

static inline int
mutex_is_locked(struct mutex *m)
{
	return RW_LOCK_HELD(&m->rw);
}

static inline int
mutex_lock_interruptible(struct mutex *m)
{
	/* illumos has no interruptible rw_enter; use regular writer lock */
	rw_enter(&m->rw, RW_WRITER);
	return 0;
}

enum mutex_trylock_recursive_result {
	MUTEX_TRYLOCK_FAILED,
	MUTEX_TRYLOCK_SUCCESS,
	MUTEX_TRYLOCK_RECURSIVE
};

static inline enum mutex_trylock_recursive_result
mutex_trylock_recursive(struct mutex *m)
{
	if (RW_WRITE_HELD(&m->rw))
		return MUTEX_TRYLOCK_RECURSIVE;
	if (mutex_trylock(m))
		return MUTEX_TRYLOCK_SUCCESS;
	return MUTEX_TRYLOCK_FAILED;
}

int atomic_dec_and_mutex_lock(volatile int *, struct mutex *);

/* Override mutex_init macro to call our wrapper */
#define mutex_init(m, ...)	mutex_init_ll(m)

#endif
