/*
 * Copyright (c) 2019 Mark Kettenis
 * illumos port: ww_mutex using illumos kmutex_t + kcondvar_t
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef _LINUX_WW_MUTEX_H_
#define _LINUX_WW_MUTEX_H_

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/thread.h>
#include <linux/mutex.h>

struct ww_class {
	volatile u_long			stamp;
	const char			*name;
};

struct ww_acquire_ctx {
	u_long				stamp;
	struct ww_class			*ww_class;
};

struct ww_mutex {
	kmutex_t			base;	/* illumos mutex (replaces OpenBSD struct mutex) */
	kcondvar_t			cv;	/* condition variable for wakeup */
	volatile int			acquired;
	struct ww_acquire_ctx		*ctx;
	kthread_t			*owner;	/* illumos thread */
};

#define DEFINE_WW_CLASS(classname)	\
	struct ww_class classname = {	\
		.stamp = 0,		\
		.name = #classname	\
	}

#define DEFINE_WD_CLASS(classname)	\
	struct ww_class classname = {	\
		.stamp = 0,		\
		.name = #classname	\
	}

static inline void
ww_acquire_init(struct ww_acquire_ctx *ctx, struct ww_class *ww_class) {
	ctx->stamp = atomic_add_long_nv(&ww_class->stamp, 1) - 1;
	ctx->ww_class = ww_class;
}

static inline void
ww_acquire_done(__unused struct ww_acquire_ctx *ctx) {
}

static inline void
ww_acquire_fini(__unused struct ww_acquire_ctx *ctx) {
}

static inline void
ww_mutex_init(struct ww_mutex *lock, struct ww_class *ww_class) {
	mutex_init(&lock->base, NULL, MUTEX_DRIVER, NULL);
	cv_init(&lock->cv, NULL, CV_DEFAULT, NULL);
	lock->acquired = 0;
	lock->ctx = NULL;
	lock->owner = NULL;
}

static inline bool
ww_mutex_is_locked(struct ww_mutex *lock) {
	bool res;
	mutex_enter(&lock->base);
	res = (lock->acquired > 0);
	mutex_exit(&lock->base);
	return res;
}

/*
 * Return 1 if lock could be acquired, else 0 (contended).
 */
static inline int
ww_mutex_trylock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx) {
	int res = 0;

	mutex_enter(&lock->base);
	if (lock->acquired == 0) {
		ASSERT(lock->ctx == NULL);
		lock->acquired = 1;
		lock->owner = curthread;
		if (ctx != NULL)
			lock->ctx = ctx;
		res = 1;
	}
	mutex_exit(&lock->base);
	return res;
}

/*
 * When `slow` is `true`, it will always block if the ww_mutex is contended.
 * When `intr` is `true`, the sleep is interruptible.
 */
static inline int
__ww_mutex_lock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx, bool slow, bool intr) {
	int err = 0;

	mutex_enter(&lock->base);
	for (;;) {
		if (lock->acquired == 0) {
			ASSERT(lock->ctx == NULL);
			lock->acquired = 1;
			lock->ctx = ctx;
			lock->owner = curthread;
			err = 0;
			break;
		} else if (lock->owner == curthread) {
			err = -EALREADY;
			break;
		} else {
			if (slow || ctx == NULL ||
			    (lock->ctx && ctx->stamp < lock->ctx->stamp)) {
				if (intr) {
					int r = cv_wait_sig(&lock->cv, &lock->base);
					if (r == EINTR || r == ERESTART) {
						err = -EINTR;
						break;
					}
				} else {
					cv_wait(&lock->cv, &lock->base);
				}
			} else {
				err = -EDEADLK;
				break;
			}
		}
	}
	mutex_exit(&lock->base);
	return err;
}

static inline int
ww_mutex_lock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx) {
	return __ww_mutex_lock(lock, ctx, false, false);
}

static inline void
ww_mutex_lock_slow(struct ww_mutex *lock, struct ww_acquire_ctx *ctx) {
	(void)__ww_mutex_lock(lock, ctx, true, false);
}

static inline int
ww_mutex_lock_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx) {
	return __ww_mutex_lock(lock, ctx, false, true);
}

static inline int __must_check
ww_mutex_lock_slow_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx) {
	return __ww_mutex_lock(lock, ctx, true, true);
}

static inline void
ww_mutex_unlock(struct ww_mutex *lock) {
	mutex_enter(&lock->base);
	ASSERT(lock->owner == curthread);
	ASSERT(lock->acquired == 1);

	lock->acquired = 0;
	lock->ctx = NULL;
	lock->owner = NULL;
	cv_broadcast(&lock->cv);
	mutex_exit(&lock->base);
}

static inline void
ww_mutex_destroy(struct ww_mutex *lock) {
	ASSERT(lock->acquired == 0);
	ASSERT(lock->ctx == NULL);
	ASSERT(lock->owner == NULL);
	cv_destroy(&lock->cv);
	mutex_destroy(&lock->base);
}

#endif	/* _LINUX_WW_MUTEX_H_ */
