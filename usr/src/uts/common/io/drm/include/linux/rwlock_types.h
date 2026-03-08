/* Public domain. */

#ifndef _LINUX_RWLOCK_TYPES_H
#define _LINUX_RWLOCK_TYPES_H

#include <sys/rwlock.h>

/*
 * Linux rw_semaphore maps to illumos krwlock_t.
 * We wrap it to avoid namespace collisions.
 */
struct rw_semaphore {
	krwlock_t	rw;
};

#endif
