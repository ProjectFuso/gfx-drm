/*
 * Copyright (c) 2015 Mark Kettenis
 * illumos port
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted.
 */

#ifndef _LINUX_WORKQUEUE_H
#define _LINUX_WORKQUEUE_H

#include <sys/taskq.h>
#include <sys/taskq_impl.h>

/*
 * illumos: taskq_cancel_id is not available in all illumos versions.
 * Use taskq_wait_id as a fallback (waits for the task instead of cancelling).
 */
#ifndef taskq_cancel_id
#define taskq_cancel_id(tq, id)		taskq_wait_id(tq, id)
#endif

/* Forward declaration needed before flush_delayed_work prototype below */
struct delayed_work;
#include <linux/container_of.h>
#include <linux/bitops.h>
#include <linux/atomic.h>
#include <linux/rcupdate.h>
#include <linux/lockdep.h>
#include <linux/timer.h>

struct workqueue_struct;

extern struct workqueue_struct *system_wq;
extern struct workqueue_struct *system_highpri_wq;
extern struct workqueue_struct *system_unbound_wq;
extern struct workqueue_struct *system_long_wq;

#define WQ_HIGHPRI	(1 << 1)
#define WQ_FREEZABLE	(1 << 2)
#define WQ_UNBOUND	(1 << 3)
#define WQ_MEM_RECLAIM	(1 << 4)

#define WQ_UNBOUND_MAX_ACTIVE	4

static inline struct workqueue_struct *
alloc_workqueue(const char *name, int flags, int max_active)
{
	taskq_t *tq = taskq_create(name, 1, minclsyspri, 1, INT_MAX,
	    TASKQ_PREPOPULATE);
	return (struct workqueue_struct *)tq;
}

static inline struct workqueue_struct *
alloc_ordered_workqueue(const char *name, int flags, ...)
{
	taskq_t *tq = taskq_create(name, 1, minclsyspri, 1, INT_MAX,
	    TASKQ_PREPOPULATE);
	return (struct workqueue_struct *)tq;
}

static inline struct workqueue_struct *
create_singlethread_workqueue(const char *name)
{
	taskq_t *tq = taskq_create(name, 1, minclsyspri, 1, INT_MAX,
	    TASKQ_PREPOPULATE);
	return (struct workqueue_struct *)tq;
}

static inline void
destroy_workqueue(struct workqueue_struct *wq)
{
	taskq_destroy((taskq_t *)wq);
}

struct work_struct {
	void		(*func)(struct work_struct *);
	taskq_t		*tq;
	taskqid_t	 id;
};

typedef void (*work_func_t)(struct work_struct *);

static void
_work_run(void *arg)
{
	struct work_struct *work = arg;
	work->func(work);
}

static inline void
INIT_WORK(struct work_struct *work, work_func_t func)
{
	work->func = func;
	work->tq = NULL;
	work->id = TASKQID_INVALID;
}

#define INIT_WORK_ONSTACK(x, y)	INIT_WORK((x), (y))

static inline bool
queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	work->tq = (taskq_t *)wq;
	work->id = taskq_dispatch(work->tq, _work_run, work, TQ_SLEEP);
	return work->id != TASKQID_INVALID;
}

static inline bool
queue_work_node(int node, struct workqueue_struct *wq, struct work_struct *work)
{
	return queue_work(wq, work);
}

static inline void
cancel_work(struct work_struct *work)
{
	if (work->tq != NULL && work->id != TASKQID_INVALID) {
		taskq_cancel_id(work->tq, work->id);
		work->id = TASKQID_INVALID;
	}
}

static inline bool
cancel_work_sync(struct work_struct *work)
{
	if (work->tq != NULL && work->id != TASKQID_INVALID) {
		taskq_cancel_id(work->tq, work->id);
		work->id = TASKQID_INVALID;
		return true;
	}
	return false;
}

#define work_pending(work)	((work)->id != TASKQID_INVALID)

void flush_workqueue(struct workqueue_struct *);
bool flush_work(struct work_struct *);
bool flush_delayed_work(struct delayed_work *);

struct delayed_work {
	struct work_struct work;
	callout_t	to;
	taskq_t		*tq;
};

#define system_power_efficient_wq	system_wq

static inline struct delayed_work *
to_delayed_work(struct work_struct *work)
{
	return container_of(work, struct delayed_work, work);
}

static void
__delayed_work_tick(void *arg)
{
	struct delayed_work *dwork = arg;

	dwork->work.id = taskq_dispatch(dwork->tq, _work_run,
	    &dwork->work, TQ_NOSLEEP);
}

static inline void
INIT_DELAYED_WORK(struct delayed_work *dwork, work_func_t func)
{
	INIT_WORK(&dwork->work, func);
	dwork->tq = NULL;
	callout_init(&dwork->to, 0);
}

#define INIT_DELAYED_WORK_ONSTACK(dwork, func) \
	INIT_DELAYED_WORK((dwork), (func))

static inline bool
schedule_work(struct work_struct *work)
{
	return queue_work(system_wq, work);
}

static inline bool
schedule_delayed_work(struct delayed_work *dwork, int delay_ticks)
{
	dwork->tq = (taskq_t *)system_wq;
	callout_reset(&dwork->to, delay_ticks, __delayed_work_tick, dwork);
	return true;
}

static inline bool
queue_delayed_work(struct workqueue_struct *wq,
    struct delayed_work *dwork, int delay_ticks)
{
	dwork->tq = (taskq_t *)wq;
	callout_reset(&dwork->to, delay_ticks, __delayed_work_tick, dwork);
	return true;
}

static inline bool
mod_delayed_work(struct workqueue_struct *wq,
    struct delayed_work *dwork, int delay_ticks)
{
	dwork->tq = (taskq_t *)wq;
	callout_reset(&dwork->to, delay_ticks, __delayed_work_tick, dwork);
	return true;
}

static inline bool
cancel_delayed_work(struct delayed_work *dwork)
{
	if (dwork->tq == NULL)
		return false;
	if (callout_stop(&dwork->to) > 0)
		return true;
	return cancel_work_sync(&dwork->work);
}

static inline bool
cancel_delayed_work_sync(struct delayed_work *dwork)
{
	return cancel_delayed_work(dwork);
}

static inline bool
delayed_work_pending(struct delayed_work *dwork)
{
	return callout_active(&dwork->to) || work_pending(&dwork->work);
}

static inline void
flush_scheduled_work(void)
{
	flush_workqueue(system_wq);
}

static inline void
drain_workqueue(struct workqueue_struct *wq)
{
	flush_workqueue(wq);
}

static inline void
destroy_work_on_stack(struct work_struct *work)
{
	cancel_work(work);
}

static inline void
destroy_delayed_work_on_stack(struct delayed_work *dwork)
{
}

struct rcu_work {
	struct work_struct work;
	struct rcu_head rcu;
};

static inline void
INIT_RCU_WORK(struct rcu_work *work, work_func_t func)
{
	INIT_WORK(&work->work, func);
}

static inline bool
queue_rcu_work(struct workqueue_struct *wq, struct rcu_work *work)
{
	return queue_work(wq, &work->work);
}

#endif
