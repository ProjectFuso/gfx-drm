/*
 * Copyright (c) 2015, 2018 Mark Kettenis
 * illumos port
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted.
 */

#ifndef _LINUX_COMPLETION_H
#define _LINUX_COMPLETION_H

#include <sys/mutex.h>
#include <sys/condvar.h>
#include <linux/wait.h>

struct completion {
	unsigned int	done;
	kmutex_t	lock;
	kcondvar_t	cv;
};

/* No static initializer in illumos; must call init_completion() */
#define DECLARE_COMPLETION_ONSTACK(name) \
	struct completion name = { 0 }  /* partial init; must call init_completion */

static inline void
init_completion(struct completion *x)
{
	x->done = 0;
	mutex_init(&x->lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&x->cv, NULL, CV_DRIVER, NULL);
}

static inline void
reinit_completion(struct completion *x)
{
	x->done = 0;
}

static inline void
destroy_completion(struct completion *x)
{
	cv_destroy(&x->cv);
	mutex_destroy(&x->lock);
}

static inline unsigned long
wait_for_completion_timeout(struct completion *x, unsigned long timo)
{
	clock_t deadline = ddi_get_lbolt() + (clock_t)timo;
	int rv;

	mutex_enter(&x->lock);
	while (x->done == 0) {
		rv = cv_timedwait(&x->cv, &x->lock, deadline);
		if (rv == -1) {
			mutex_exit(&x->lock);
			return 0;
		}
	}
	if (x->done != UINT_MAX)
		x->done--;
	mutex_exit(&x->lock);
	return 1;
}

static inline void
wait_for_completion(struct completion *x)
{
	mutex_enter(&x->lock);
	while (x->done == 0)
		cv_wait(&x->cv, &x->lock);
	if (x->done != UINT_MAX)
		x->done--;
	mutex_exit(&x->lock);
}

static inline long
wait_for_completion_interruptible(struct completion *x)
{
	int rv;

	mutex_enter(&x->lock);
	while (x->done == 0) {
		rv = cv_wait_sig(&x->cv, &x->lock);
		if (rv == 0) {
			mutex_exit(&x->lock);
			return -ERESTARTSYS;
		}
	}
	if (x->done != UINT_MAX)
		x->done--;
	mutex_exit(&x->lock);
	return 0;
}

static inline long
wait_for_completion_interruptible_timeout(struct completion *x,
    unsigned long timo)
{
	clock_t deadline = ddi_get_lbolt() + (clock_t)timo;
	int rv;

	mutex_enter(&x->lock);
	while (x->done == 0) {
		rv = cv_timedwait_sig(&x->cv, &x->lock, deadline);
		if (rv == 0) {
			mutex_exit(&x->lock);
			return -ERESTARTSYS;
		}
		if (rv == -1) {
			mutex_exit(&x->lock);
			return 0;
		}
	}
	if (x->done != UINT_MAX)
		x->done--;
	mutex_exit(&x->lock);
	return 1;
}

static inline void
complete(struct completion *x)
{
	mutex_enter(&x->lock);
	if (x->done != UINT_MAX)
		x->done++;
	cv_signal(&x->cv);
	mutex_exit(&x->lock);
}

static inline void
complete_all(struct completion *x)
{
	mutex_enter(&x->lock);
	x->done = UINT_MAX;
	cv_broadcast(&x->cv);
	mutex_exit(&x->lock);
}

static inline bool
try_wait_for_completion(struct completion *x)
{
	mutex_enter(&x->lock);
	if (x->done == 0) {
		mutex_exit(&x->lock);
		return false;
	}
	if (x->done != UINT_MAX)
		x->done--;
	mutex_exit(&x->lock);
	return true;
}

static inline bool
completion_done(struct completion *x)
{
	return x->done != 0;
}

#endif
