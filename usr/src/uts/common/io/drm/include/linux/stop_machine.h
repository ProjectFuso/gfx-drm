/* Public domain. */

#ifndef _LINUX_STOP_MACHINE_H
#define _LINUX_STOP_MACHINE_H

#include <sys/types.h>

typedef int (*cpu_stop_fn_t)(void *arg);

/*
 * illumos Phase 1: stop_machine() runs fn directly without stopping CPUs.
 * True multi-CPU quiescing can be added via xc_call() when needed.
 */
static inline int
stop_machine(cpu_stop_fn_t fn, void *arg, void *cpus)
{
	return (*fn)(arg);
}

#endif
