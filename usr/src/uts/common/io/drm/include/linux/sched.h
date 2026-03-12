/* illumos port of linux/sched.h */
/*
 * Copyright (c) 2013, 2014, 2015 Mark Kettenis
 * illumos port: scheduler/thread state compat
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef _LINUX_SCHED_H
#define _LINUX_SCHED_H

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/user.h>
#include <sys/ddi.h>
#include <linux/illumos_page_compat.h>
#include <sys/sunddi.h>
#include <linux/hrtimer.h>
#include <linux/sem.h>
#include <linux/mutex.h>
#include <asm/current.h>

/*
 * Task state constants.  On illumos these are just hints; we don't
 * actually manipulate per-thread state bits the way Linux does.
 */
#ifndef TASK_NORMAL
#define TASK_NORMAL		1
#endif
#define TASK_UNINTERRUPTIBLE	0
#define TASK_INTERRUPTIBLE	1	/* signals allowed (hint only) */
#define TASK_RUNNING		-1

#define MAX_SCHEDULE_TIMEOUT	(INT32_MAX)

#define TASK_COMM_LEN		MAXCOMLEN

/*
 * struct task_struct — minimal Linux task descriptor for DRM.
 *
 * On illumos we implement this via thread-specific data (TSD).
 * The group_leader pointer uses curproc as a stable per-process key so
 * that pointer comparisons (used by the GPU scheduler) are correct:
 * all threads of the same process share the same curproc value.
 *
 * Process flags subset used by DRM:
 */
#define PF_EXITING	0x00000004u	/* process is exiting (SEXITING) */

struct task_struct {
	pid_t			 pid;
	char			 comm[TASK_COMM_LEN + 1];
	unsigned int		 flags;
	int			 exit_code;
	struct task_struct	*group_leader;
};

static inline pid_t
task_pid_nr(struct task_struct *t)
{
	return t->pid;
}

/*
 * task_pgrp_vnr — return the process group ID of the given task.
 * On illumos, we use curproc->p_pgidp->pid_id.
 */
static inline pid_t
task_pgrp_vnr(struct task_struct *t)
{
	return (pid_t)curproc->p_pgidp->pid_id;
}

/*
 * drm_get_current() — return a per-thread task_struct reflecting the
 * calling thread's process.  Allocated lazily via TSD on first call.
 */
extern struct task_struct *drm_get_current(void);

/*
 * cond_resched: voluntary preemption point.
 * illumos: no-op for Phase 1 (kernel preemption handles this).
 */
#define cond_resched()		do { } while (0)
#define drm_need_resched()	0

static inline int
cond_resched_lock(struct mutex *mtxp)
{
	/* no-op: voluntary preempt while holding lock not easily done */
	return 0;
}

void set_current_state(int);
void __set_current_state(int);
void schedule(void);
long schedule_timeout(long);
long schedule_timeout_uninterruptible(long);

#define io_schedule_timeout(x)	schedule_timeout(x)

/* wake_up_process: no-op in our cv-based implementation */
int wake_up_process(struct task_struct *t);

#endif /* _LINUX_SCHED_H */
