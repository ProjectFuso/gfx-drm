/* Public domain. */

#ifndef _LINUX_HRTIMER_H
#define _LINUX_HRTIMER_H

#include <linux/ktime.h>
#include <linux/rbtree.h>
#include <sys/callout.h>	/* illumos callout_t wrapping timeout(9F) */

enum hrtimer_restart { HRTIMER_NORESTART, HRTIMER_RESTART };

#define HRTIMER_MODE_REL	1
#define HRTIMER_MODE_ABS	0

/* timerqueue_node — Linux keeps the expiry here; vmwgfx_vkms reads node.expires */
struct timerqueue_node {
	struct rb_node	node;
	ktime_t		expires;
};

struct hrtimer {
	struct timerqueue_node	node;		/* node.expires = next expiry */
	ktime_t			_softexpires;
	enum hrtimer_restart	(*function)(struct hrtimer *);
	callout_t		co;	/* illumos: callout for actual scheduling */
};

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

/*
 * hrtimer_forward_now - advance timer expiry to now + interval.
 * Returns number of overruns (always 1 in Phase 1 stub).
 */
static inline u64
hrtimer_forward_now(struct hrtimer *timer, ktime_t interval)
{
	ktime_t now = ktime_get();
	timer->node.expires = now + interval;
	return 1;
}

#endif
