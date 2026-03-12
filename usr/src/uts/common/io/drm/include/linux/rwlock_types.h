/* Public domain. */

#ifndef _LINUX_RWLOCK_TYPES_H
#define _LINUX_RWLOCK_TYPES_H

#include <sys/rwlock.h>

/*
 * Linux rwlock_t — non-sleeping reader/writer spinlock.
 * On illumos we implement it as kmutex_t (no reader-side parallelism, but
 * correct mutual exclusion semantics needed for Phase 1).
 */
typedef kmutex_t rwlock_t;

/*
 * Linux rw_semaphore — sleeping reader/writer lock.
 * Maps to illumos krwlock_t, wrapped to avoid namespace collisions.
 */
struct rw_semaphore {
	krwlock_t	rw;
};

#endif
