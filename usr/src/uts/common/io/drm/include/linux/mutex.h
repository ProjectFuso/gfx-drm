/* Public domain. */

#ifndef _LINUX_MUTEX_H
#define _LINUX_MUTEX_H

/*
 * illumos: Linux struct mutex maps to illumos kmutex_t (struct mutex).
 *
 * illumos sys/mutex.h already defines:
 *   typedef struct mutex { ... } kmutex_t;
 *
 * We do NOT redefine struct mutex — that would conflict with illumos's
 * kmutex_t typedef.  Instead we use kmutex_t directly and map Linux
 * mutex API to the illumos mutex_enter/mutex_exit/mutex_tryenter API.
 *
 * For Linux code that uses struct mutex members, DEFINE_MUTEX creates a
 * kmutex_t.  All mutex_lock/mutex_unlock calls invoke mutex_enter/mutex_exit.
 */

#include <sys/rwlock.h>		/* krwlock_t for struct rwlock */
#include <linux/list.h>
#include <linux/spinlock_types.h>	/* includes sys/mutex.h transitively */
#include <linux/lockdep.h>
#include <linux/rwlock.h>		/* struct rwlock */

/*
 * DEFINE_MUTEX: creates a kmutex_t variable (= illumos struct mutex).
 * Must be initialized via mutex_init() before use.
 */
#define DEFINE_MUTEX(x)		kmutex_t x

/*
 * mutex_init_ll: wrapper calling the real 4-arg illumos mutex_init.
 * Must be defined BEFORE the #define mutex_init macro so the body
 * calls the real illumos function, not our macro.
 */
static inline void
mutex_init_ll(kmutex_t *m)
{
	mutex_init(m, NULL, MUTEX_DEFAULT, NULL);
}

static inline void
mutex_destroy_ll(kmutex_t *m)
{
	mutex_destroy(m);
}

static inline void
mutex_lock(kmutex_t *m)
{
	mutex_enter(m);
}

#define mutex_lock_nest_lock(m, sub)	mutex_lock(m)
#define mutex_lock_nested(m, sub)	mutex_lock(m)
#define mutex_lock_interruptible_nested(m, subc) \
					mutex_lock_interruptible(m)

static inline void
mutex_unlock(kmutex_t *m)
{
	mutex_exit(m);
}

static inline int
mutex_trylock(kmutex_t *m)
{
	return mutex_tryenter(m);
}

static inline int
mutex_is_locked(kmutex_t *m)
{
	return MUTEX_HELD(m);
}

static inline int
mutex_lock_interruptible(kmutex_t *m)
{
	/* illumos has no interruptible mutex_enter; use regular lock */
	mutex_enter(m);
	return 0;
}

enum mutex_trylock_recursive_result {
	MUTEX_TRYLOCK_FAILED,
	MUTEX_TRYLOCK_SUCCESS,
	MUTEX_TRYLOCK_RECURSIVE
};

static inline enum mutex_trylock_recursive_result
mutex_trylock_recursive(kmutex_t *m)
{
	if (mutex_trylock(m))
		return MUTEX_TRYLOCK_SUCCESS;
	return MUTEX_TRYLOCK_FAILED;
}

int atomic_dec_and_mutex_lock(volatile int *, kmutex_t *);

/* Override mutex_init / mutex_destroy macros to call our wrappers */
#define mutex_init(m, ...)	mutex_init_ll(m)

/* Polymorphic mutex_destroy: handles both kmutex_t * and struct rwlock * */
#undef mutex_destroy
#define mutex_destroy(m)						\
	__builtin_choose_expr(						\
		__builtin_types_compatible_p(__typeof__(*(m)), struct rwlock),\
		(drm_rw_destroy((struct rwlock *)(void *)(m))),		\
		(mutex_destroy_ll((kmutex_t *)(void *)(m))))

/*
 * Polymorphic mutex_lock / mutex_unlock / mutex_is_locked / mutex_trylock.
 *
 * OpenBSD DRM uses 'struct rwlock' (= { krwlock_t rw; }) for some fields
 * (e.g. drm_mode_config.mutex) but calls Linux mutex_lock/unlock on them.
 * On OpenBSD, rwlock and mutex APIs are unified; on illumos they are not.
 *
 * Use __builtin_types_compatible_p + __builtin_choose_expr to dispatch at
 * compile time: if the pointee is struct rwlock, use rw_enter/rw_exit;
 * otherwise use mutex_enter/mutex_exit on a kmutex_t.
 *
 * NOTE: struct rwlock must be defined (from <linux/rwlock.h>, included above).
 */
#undef mutex_lock
#define mutex_lock(m)							\
	__builtin_choose_expr(						\
		__builtin_types_compatible_p(__typeof__(*(m)), struct rwlock),\
		(rw_enter(&((struct rwlock *)(void *)(m))->rw, RW_WRITER)),\
		(mutex_enter((kmutex_t *)(void *)(m))))

#undef mutex_unlock
#define mutex_unlock(m)							\
	__builtin_choose_expr(						\
		__builtin_types_compatible_p(__typeof__(*(m)), struct rwlock),\
		(rw_exit(&((struct rwlock *)(void *)(m))->rw)),		\
		(mutex_exit((kmutex_t *)(void *)(m))))

#undef mutex_trylock
#define mutex_trylock(m)						\
	__builtin_choose_expr(						\
		__builtin_types_compatible_p(__typeof__(*(m)), struct rwlock),\
		(rw_tryenter(&((struct rwlock *)(void *)(m))->rw, RW_WRITER)),\
		(mutex_tryenter((kmutex_t *)(void *)(m))))

#undef mutex_is_locked
#define mutex_is_locked(m)						\
	__builtin_choose_expr(						\
		__builtin_types_compatible_p(__typeof__(*(m)), struct rwlock),\
		(RW_WRITE_HELD(&((struct rwlock *)(void *)(m))->rw)),	\
		(MUTEX_HELD((kmutex_t *)(void *)(m))))

#endif
