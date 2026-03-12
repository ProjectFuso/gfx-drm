/* Public domain. */

#ifndef _LINUX_RWLOCK_H
#define _LINUX_RWLOCK_H

#include <linux/mutex.h>

/*
 * illumos: struct rwlock — same layout as struct mutex (both wrap krwlock_t).
 *
 * OpenBSD DRM uses struct rwlock as its sleeping mutex (matching Linux's
 * struct mutex semantics). On illumos both map to krwlock_t.
 *
 * Fields declared as 'struct rwlock' in DRM private headers can be accessed
 * with the mutex_lock / mutex_unlock / mutex_init macros.
 */
struct rwlock {
	krwlock_t	rw;
};

#define rwlock_is_locked(l)	RW_LOCK_HELD(&(l)->rw)
#define rwlock_is_wlocked(l)	RW_WRITE_HELD(&(l)->rw)

/* Allow mutex_* macros to be used on struct rwlock fields directly */
#define rw_assert_held(l)	ASSERT(RW_LOCK_HELD(&(l)->rw))
#define assert_rwlock_held(l)	ASSERT(RW_LOCK_HELD(&(l)->rw))

/*
 * OpenBSD: rw_init(struct rwlock *, name) — 2-arg form.
 * illumos rw_init takes 4 args; define wrapper before the macro.
 */
static inline void
drm_rw_init(struct rwlock *l, const char *name)
{
	rw_init(&l->rw, (char *)name, RW_DEFAULT, NULL);
}
#define rwlock_init(l)		drm_rw_init(l, NULL)

/*
 * OpenBSD: rwlock_destroy(struct rwlock *) — wraps illumos rw_destroy.
 * Also used as mutex_destroy when the field is struct rwlock.
 */
static inline void
drm_rw_destroy(struct rwlock *l)
{
	rw_destroy(&l->rw);
}
#define rwlock_destroy(l)	drm_rw_destroy(l)


#endif
