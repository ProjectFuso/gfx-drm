/* Public domain. */

#ifndef _LINUX_IRQFLAGS_H
#define _LINUX_IRQFLAGS_H

#include <sys/types.h>

/*
 * illumos: interrupt enable/disable is handled by the dispatcher;
 * there is no direct splhigh/splx equivalent exposed to driver code.
 * These macros are no-ops for DRM which runs in non-interrupt context.
 * The `flags` variable is unused but kept for API compatibility.
 */
#define local_irq_save(x)	do { (x) = 0; } while (0)
#define local_irq_restore(x)	do { (void)(x); } while (0)

#define local_irq_disable()	do { } while (0)
#define local_irq_enable()	do { } while (0)

static inline int
irqs_disabled(void)
{
	return (0);
}

#endif
