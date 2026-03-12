/* Public domain. */

#ifndef _LINUX_RWLOCK_H
#define _LINUX_RWLOCK_H

#include <linux/mutex.h>

/*
 * Linux rwlock_t — non-sleeping reader/writer spinlock.
 * Defined as kmutex_t in rwlock_types.h.
 * Phase 1: no true reader parallelism; mutual exclusion is what matters.
 */
#define rwlock_init(l)			mutex_init_ll((kmutex_t *)(l))
#define rwlock_destroy(l)		mutex_destroy_ll((kmutex_t *)(l))

#define read_lock(l)			mutex_enter((kmutex_t *)(l))
#define read_unlock(l)			mutex_exit((kmutex_t *)(l))
#define write_lock(l)			mutex_enter((kmutex_t *)(l))
#define write_unlock(l)			mutex_exit((kmutex_t *)(l))

#define read_lock_irqsave(l, f)		do { (void)(f); mutex_enter((kmutex_t *)(l)); } while (0)
#define read_unlock_irqrestore(l, f)	do { (void)(f); mutex_exit((kmutex_t *)(l)); } while (0)
#define write_lock_irqsave(l, f)	do { (void)(f); mutex_enter((kmutex_t *)(l)); } while (0)
#define write_unlock_irqrestore(l, f)	do { (void)(f); mutex_exit((kmutex_t *)(l)); } while (0)

/*
 * struct rwlock — transitional OpenBSD-derived sleeping mutex wrapping
 * krwlock_t.  Will be removed once all DRM headers are reverted to struct mutex.
 */
struct rwlock {
	krwlock_t	rw;
};

static inline void
drm_rw_init(struct rwlock *l, const char *name)
{
	rw_init(&l->rw, (char *)name, RW_DEFAULT, NULL);
}

static inline void
drm_rw_destroy(struct rwlock *l)
{
	rw_destroy(&l->rw);
}

#define rwlock_is_locked(l)	RW_LOCK_HELD(&(l)->rw)
#define rwlock_is_wlocked(l)	RW_WRITE_HELD(&(l)->rw)
#define rw_assert_held(l)	ASSERT(RW_LOCK_HELD(&(l)->rw))
#define assert_rwlock_held(l)	ASSERT(RW_LOCK_HELD(&(l)->rw))

#endif
