/* Public domain. */

#ifndef _LINUX_KTHREAD_H
#define _LINUX_KTHREAD_H

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/taskq.h>
#include <linux/types.h>

struct kthread_work {
	taskqid_t	 id;
	taskq_t		*tq;
	void		(*func)(struct kthread_work *);
};

struct kthread_worker {
	taskq_t		*tq;
};

/* kthread_run: create a kernel thread */
kthread_t *kthread_run(void (*func)(void *), void *data, const char *name);
void kthread_stop(kthread_t *t);
int  kthread_should_stop(void);

struct kthread_worker *kthread_create_worker(unsigned int flags,
    const char *fmt, ...);
void kthread_destroy_worker(struct kthread_worker *worker);
void kthread_init_work(struct kthread_work *work,
    void (*fn)(struct kthread_work *));
bool kthread_queue_work(struct kthread_worker *worker,
    struct kthread_work *work);
bool kthread_cancel_work_sync(struct kthread_work *work);
void kthread_flush_work(struct kthread_work *work);
void kthread_flush_worker(struct kthread_worker *worker);

/* park/unpark - implemented in drm_linux.c */
void kthread_park(kthread_t *t);
void kthread_unpark(kthread_t *t);
int  kthread_should_park(void);
void kthread_parkme(void);

#endif
