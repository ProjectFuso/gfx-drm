/* Public domain. */

#ifndef _LINUX_PREEMPT_H
#define _LINUX_PREEMPT_H

#include <stdbool.h>
#include <asm/preempt.h>

static inline void
preempt_enable(void)
{
}

static inline void
preempt_disable(void)
{
}

static inline void
migrate_enable(void)
{
}

static inline void
migrate_disable(void)
{
}

static inline bool
in_irq(void)
{
	/* illumos: DRM does not run from interrupt context */
	return false;
}

static inline bool
in_interrupt(void)
{
	return in_irq();
}

static inline bool
in_task(void)
{
	return !in_irq();
}

static inline bool
in_atomic(void)
{
	return false;
}

#endif
