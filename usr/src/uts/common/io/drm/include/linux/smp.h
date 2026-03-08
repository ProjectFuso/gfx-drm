/* Public domain. */

#ifndef _LINUX_SMP_H
#define _LINUX_SMP_H

#include <sys/types.h>
#include <sys/cpuvar.h>
#include <linux/cpumask.h>

/* illumos: CPU number from DDI */
#define smp_processor_id()	(CPU->cpu_id)

#define get_cpu()		(CPU->cpu_id)
#define put_cpu()

#endif
