/* Public domain. */

#ifndef _ASM_CURRENT_H
#define _ASM_CURRENT_H

#include <linux/sched.h>	/* struct task_struct, drm_get_current() */

/*
 * current — pointer to the calling thread's task_struct.
 *
 * Implemented via illumos TSD (thread-specific data) in drm_linux.c.
 * Each call refreshes the struct from curproc, so field values reflect
 * the current process at the time of access.  group_leader is set to
 * (struct task_struct *)curproc so that cross-thread comparisons are
 * stable and correct.
 */
#define current		drm_get_current()

#endif /* _ASM_CURRENT_H */
