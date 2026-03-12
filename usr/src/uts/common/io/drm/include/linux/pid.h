/* Public domain. */

#ifndef _LINUX_PID_H
#define _LINUX_PID_H

#include <linux/rculist.h>

struct task_struct;

/*
 * illumos: Linux's struct pid is already defined by illumos sys/proc.h with a
 * different layout.  Use drm_pid_t as an opaque handle to avoid conflicts.
 * The drm_file.h member uses struct drm_linux_pid instead.
 */
struct drm_linux_pid {
	pid_t value;
};

static inline struct drm_linux_pid *
get_pid(struct drm_linux_pid *pid)
{
	return pid;
}

#define put_pid(x)	do { } while (0)

static inline pid_t
pid_vnr(struct drm_linux_pid *pid)
{
	return pid ? pid->value : (pid_t)0;
}

static inline struct drm_linux_pid *
task_pid(struct task_struct *task)
{
	(void)task;
	return NULL;
}

#endif
