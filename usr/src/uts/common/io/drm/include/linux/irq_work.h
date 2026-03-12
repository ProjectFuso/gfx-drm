/*
 * Copyright (c) 2015 Mark Kettenis
 * illumos port: use illumos taskq for irq_work
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef _LINUX_IRQ_WORK_H
#define _LINUX_IRQ_WORK_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/taskq_impl.h>	/* taskq_ent_t, taskq_dispatch_ent */

#include <linux/llist.h>

struct workqueue_struct;

extern struct workqueue_struct *system_wq;

struct irq_node {
	struct llist_node llist;
};

struct irq_work {
	taskq_ent_t	task;
	taskq_t		*tq;
	struct irq_node	node;
	void (*func)(struct irq_work *);
};

typedef void (*irq_work_func_t)(struct irq_work *);

static void _irq_work_runner(void *arg);

static inline void
init_irq_work(struct irq_work *work, irq_work_func_t func)
{
	work->tq = (taskq_t *)system_wq;
	work->func = func;
	bzero(&work->task, sizeof(work->task));
}

static inline bool
irq_work_queue(struct irq_work *work)
{
	taskq_dispatch_ent(work->tq, _irq_work_runner, work, TQ_SLEEP,
	    &work->task);
	return true;
}

static inline void
irq_work_sync(struct irq_work *work)
{
	taskq_wait(work->tq);
}

/* runner shim: called by taskq, dispatches to irq_work->func */
static void
_irq_work_runner(void *arg)
{
	struct irq_work *work = arg;
	work->func(work);
}

#endif
