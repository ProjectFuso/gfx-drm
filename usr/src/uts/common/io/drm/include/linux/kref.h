/*	$OpenBSD: kref.h,v 1.6 2023/03/21 09:44:35 jsg Exp $	*/
/*
 * Copyright (c) 2015 Mark Kettenis
 * illumos port: use illumos mutex/rwlock primitives
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef _LINUX_KREF_H
#define _LINUX_KREF_H

#include <sys/types.h>
#include <linux/refcount.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>		/* struct mutex, mutex_lock, mutex_unlock */

struct kref {
	uint32_t refcount;
};

static inline void
kref_init(struct kref *ref)
{
	atomic_set(&ref->refcount, 1);
}

static inline unsigned int
kref_read(const struct kref *ref)
{
	return atomic_read(&ref->refcount);
}

static inline void
kref_get(struct kref *ref)
{
	atomic_inc_int(&ref->refcount);
}

static inline int
kref_get_unless_zero(struct kref *ref)
{
	if (ref->refcount != 0) {
		atomic_inc_int(&ref->refcount);
		return (1);
	} else {
		return (0);
	}
}

static inline int
kref_put(struct kref *ref, void (*release)(struct kref *ref))
{
	if (atomic_dec_int_nv(&ref->refcount) == 0) {
		release(ref);
		return 1;
	}
	return 0;
}

/*
 * kref_put_mutex: decrement ref; if this drops to zero, hold the mutex
 * and call release (which must drop the mutex) and return 1, else return 0.
 * On illumos, struct mutex wraps krwlock_t so we use mutex_lock/mutex_unlock.
 */
static inline int
kref_put_mutex(struct kref *kref, void (*release)(struct kref *kref),
    struct mutex *lock)
{
	if (!atomic_add_unless(&kref->refcount, -1, 1)) {
		mutex_lock(lock);
		if (likely(atomic_dec_and_test(&kref->refcount))) {
			release(kref);
			return 1;
		}
		mutex_unlock(lock);
		return 0;
	}

	return 0;
}

static inline int
kref_put_lock(struct kref *kref, void (*release)(struct kref *kref),
    struct mutex *lock)
{
	if (!atomic_add_unless(&kref->refcount, -1, 1)) {
		mutex_lock(lock);
		if (likely(atomic_dec_and_test(&kref->refcount))) {
			release(kref);
			return 1;
		}
		mutex_unlock(lock);
		return 0;
	}

	return 0;
}

#endif
