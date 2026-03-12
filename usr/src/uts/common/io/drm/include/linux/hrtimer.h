/* Public domain. */

#ifndef _LINUX_HRTIMER_H
#define _LINUX_HRTIMER_H

#include <sys/types.h>
#include <sys/callout.h>	/* illumos callout_t wrapping timeout(9F) */
#include <linux/rbtree.h>

enum hrtimer_restart { HRTIMER_NORESTART, HRTIMER_RESTART };

struct hrtimer {
	enum hrtimer_restart	(*function)(struct hrtimer *);
	callout_t		co;	/* illumos: callout replaces OpenBSD timeout */
};

#define HRTIMER_MODE_REL	1

static inline void
hrtimer_cancel(struct hrtimer *t)
{
	callout_stop(&t->co);
}

static inline int
hrtimer_try_to_cancel(struct hrtimer *t)
{
	return callout_stop(&t->co);
}

static inline bool
hrtimer_active(struct hrtimer *t)
{
	return callout_active(&t->co);
}

static inline void
hrtimer_init(struct hrtimer *t, int clock_id, int mode)
{
	callout_init(&t->co, 0);
}

#define hrtimer_start(t, kt, mode) \
	callout_reset(&(t)->co, \
	    (clock_t)(MAX(1, (kt) / (1000000000LL / hz))), \
	    (void (*)(void *))(t)->function, (t))

#endif
