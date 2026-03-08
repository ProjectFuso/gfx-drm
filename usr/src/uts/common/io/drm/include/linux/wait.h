/*
 * Copyright (c) 2013, 2014, 2015 Mark Kettenis
 * Copyright (c) 2017 Martin Pieuchot
 * illumos port: see porting notes
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _LINUX_WAIT_H
#define _LINUX_WAIT_H

#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/thread.h>

#include <linux/list.h>
#include <linux/errno.h>
#include <linux/spinlock.h>

struct wait_queue_entry {
	unsigned int flags;
	void *private;
	int (*func)(struct wait_queue_entry *, unsigned, int, void *);
	struct list_head entry;
};

#define WQ_FLAG_WOKEN	(1 << 1)

typedef struct wait_queue_entry wait_queue_entry_t;

struct wait_queue_head {
	kmutex_t	lock;
	kcondvar_t	cv;
	struct list_head head;
};
typedef struct wait_queue_head wait_queue_head_t;

void	prepare_to_wait(wait_queue_head_t *, wait_queue_entry_t *, int);
void	finish_wait(wait_queue_head_t *, wait_queue_entry_t *);

static inline void
init_waitqueue_head(wait_queue_head_t *wqh)
{
	mutex_init(&wqh->lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&wqh->cv, NULL, CV_DRIVER, NULL);
	INIT_LIST_HEAD(&wqh->head);
}

static inline void
destroy_waitqueue_head(wait_queue_head_t *wqh)
{
	cv_destroy(&wqh->cv);
	mutex_destroy(&wqh->lock);
}

#define __init_waitqueue_head(wqh, name, key)	init_waitqueue_head(wqh)

int autoremove_wake_function(struct wait_queue_entry *, unsigned int, int, void *);
int woken_wake_function(struct wait_queue_entry *, unsigned int, int, void *);

static inline void
init_wait_entry(wait_queue_entry_t *wqe, int flags)
{
	wqe->flags = flags;
	wqe->private = curthread;
	wqe->func = autoremove_wake_function;
	INIT_LIST_HEAD(&wqe->entry);
}

static inline void
__add_wait_queue(wait_queue_head_t *wqh, wait_queue_entry_t *wqe)
{
	list_add(&wqe->entry, &wqh->head);
}

static inline void
__add_wait_queue_entry_tail(wait_queue_head_t *wqh, wait_queue_entry_t *wqe)
{
	list_add_tail(&wqe->entry, &wqh->head);
}

static inline void
add_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *new)
{
	mutex_enter(&head->lock);
	__add_wait_queue(head, new);
	mutex_exit(&head->lock);
}

static inline void
__remove_wait_queue(wait_queue_head_t *wqh, wait_queue_entry_t *wqe)
{
	list_del(&wqe->entry);
}

static inline void
remove_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *old)
{
	mutex_enter(&head->lock);
	__remove_wait_queue(head, old);
	mutex_exit(&head->lock);
}

/*
 * Wait until condition is true (uninterruptible).
 * Uses wqh->lock + wqh->cv for sleeping.
 */
#define __wait_event(wqh, condition)					\
do {									\
	mutex_enter(&(wqh).lock);					\
	while (!(condition))						\
		cv_wait(&(wqh).cv, &(wqh).lock);			\
	mutex_exit(&(wqh).lock);					\
} while (0)

/*
 * Wait until condition is true or timeout expires.
 * Returns remaining jiffies (>=1) if condition true, 0 on timeout.
 */
#define __wait_event_timeout(wqh, condition, timo)			\
({									\
	long __ret = (timo);						\
	clock_t __deadline = ddi_get_lbolt() + __ret;			\
	int __cv_ret = 0;						\
	mutex_enter(&(wqh).lock);					\
	while (!(condition) && __ret > 0) {				\
		__cv_ret = cv_timedwait(&(wqh).cv, &(wqh).lock,	\
		    __deadline);					\
		if (__cv_ret == -1) {					\
			__ret = 0;					\
			break;						\
		}							\
		__ret = __deadline - ddi_get_lbolt();			\
	}								\
	if ((condition))						\
		__ret = MAX(__ret, 1);					\
	mutex_exit(&(wqh).lock);					\
	__ret;								\
})

/*
 * Wait until condition is true (interruptible by signals).
 */
#define __wait_event_intr(wqh, condition)				\
({									\
	int __ret = 0;							\
	mutex_enter(&(wqh).lock);					\
	while (!(condition)) {						\
		if (cv_wait_sig(&(wqh).cv, &(wqh).lock) == 0) {	\
			__ret = -ERESTARTSYS;				\
			break;						\
		}							\
	}								\
	mutex_exit(&(wqh).lock);					\
	__ret;								\
})

/*
 * Wait until condition is true, with timeout (interruptible).
 */
#define __wait_event_intr_timeout(wqh, condition, timo)			\
({									\
	long __ret = (timo);						\
	clock_t __deadline = ddi_get_lbolt() + __ret;			\
	int __cv_ret;							\
	mutex_enter(&(wqh).lock);					\
	while (!(condition) && __ret > 0) {				\
		__cv_ret = cv_timedwait_sig(&(wqh).cv, &(wqh).lock,	\
		    __deadline);					\
		if (__cv_ret == 0) {					\
			__ret = -ERESTARTSYS;				\
			break;						\
		}							\
		if (__cv_ret == -1) {					\
			__ret = 0;					\
			break;						\
		}							\
		__ret = __deadline - ddi_get_lbolt();			\
	}								\
	if (__ret > 0 && (condition))					\
		__ret = MAX(__ret, 1);					\
	mutex_exit(&(wqh).lock);					\
	__ret;								\
})

#define wait_event(wqh, condition)					\
do {									\
	if (!(condition))						\
		__wait_event(wqh, condition);				\
} while (0)

#define wait_event_killable(wqh, condition)				\
({									\
	int __ret = 0;							\
	if (!(condition))						\
		__ret = __wait_event_intr(wqh, condition);		\
	__ret;								\
})

#define wait_event_interruptible(wqh, condition)			\
({									\
	int __ret = 0;							\
	if (!(condition))						\
		__ret = __wait_event_intr(wqh, condition);		\
	__ret;								\
})

#define wait_event_timeout(wqh, condition, timo)			\
({									\
	long __ret = (timo);						\
	if (!(condition))						\
		__ret = __wait_event_timeout(wqh, condition, timo);	\
	__ret;								\
})

#define wait_event_interruptible_timeout(wqh, condition, timo)		\
({									\
	long __ret = (timo);						\
	if (!(condition))						\
		__ret = __wait_event_intr_timeout(wqh, condition, timo); \
	__ret;								\
})

#define wait_event_interruptible_locked(wqh, condition)			\
({									\
	int __ret = 0;							\
	while (!(condition)) {						\
		if (cv_wait_sig(&(wqh).cv, &(wqh).lock) == 0) {	\
			__ret = -ERESTARTSYS;				\
			break;						\
		}							\
	}								\
	__ret;								\
})

#define wait_event_lock_irq(wqh, condition, mtx)			\
do {									\
	while (!(condition)) {						\
		mutex_exit(&(mtx));					\
		mutex_enter(&(wqh).lock);				\
		cv_wait(&(wqh).cv, &(wqh).lock);			\
		mutex_exit(&(wqh).lock);				\
		mutex_enter(&(mtx));					\
	}								\
} while (0)

static inline void
wake_up(wait_queue_head_t *wqh)
{
	mutex_enter(&wqh->lock);
	cv_broadcast(&wqh->cv);
	{
		wait_queue_entry_t *wqe, *tmp;
		list_for_each_entry_safe(wqe, tmp, &wqh->head, entry) {
			if (wqe->func != NULL)
				wqe->func(wqe, 0, wqe->flags, NULL);
		}
	}
	mutex_exit(&wqh->lock);
}

#define wake_up_all(wqh)			wake_up(wqh)

static inline void
wake_up_all_locked(wait_queue_head_t *wqh)
{
	cv_broadcast(&wqh->cv);
	{
		wait_queue_entry_t *wqe, *tmp;
		list_for_each_entry_safe(wqe, tmp, &wqh->head, entry) {
			if (wqe->func != NULL)
				wqe->func(wqe, 0, wqe->flags, NULL);
		}
	}
}

#define wake_up_interruptible(wqh)		wake_up(wqh)
#define wake_up_interruptible_poll(wqh, flags)	wake_up(wqh)

#define	DEFINE_WAIT(name)				\
	struct wait_queue_entry name = {		\
		.private = curthread,			\
		.func = autoremove_wake_function,	\
		.entry = LIST_HEAD_INIT((name).entry),	\
	}

#define	DEFINE_WAIT_FUNC(name, fn)			\
	struct wait_queue_entry name = {		\
		.private = curthread,			\
		.func = fn,				\
		.entry = LIST_HEAD_INIT((name).entry),	\
	}

#endif
