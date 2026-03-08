/*
 * Copyright (c) 2013, 2014, 2015 Mark Kettenis
 * illumos port
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted.
 */

#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#include <sys/types.h>
#include <sys/callout.h>
#include <linux/ktime.h>

/*
 * Linux timer_list maps to illumos callout_t.
 * illumos callout API: callout_init, callout_reset, callout_stop, callout_active.
 */
struct timer_list {
	callout_t		co;
	void			(*function)(struct timer_list *);
	unsigned long		expires;
};

static inline void
timer_setup(struct timer_list *t, void (*fn)(struct timer_list *),
    unsigned int flags)
{
	t->function = fn;
	t->expires = 0;
	callout_init(&t->co, 0);
}

static void
_timer_callout(void *arg)
{
	struct timer_list *t = arg;
	t->function(t);
}

static inline int
mod_timer(struct timer_list *t, unsigned long expires)
{
	clock_t ticks;
	t->expires = expires;
	ticks = (clock_t)expires - ddi_get_lbolt();
	if (ticks <= 0)
		ticks = 1;
	callout_reset(&t->co, ticks, _timer_callout, t);
	return 0;
}

static inline void
add_timer(struct timer_list *t)
{
	mod_timer(t, t->expires);
}

static inline int
del_timer(struct timer_list *t)
{
	return callout_stop(&t->co);
}

static inline int
del_timer_sync(struct timer_list *t)
{
	callout_drain(&t->co);
	return 0;
}

#define timer_shutdown_sync(t)	del_timer_sync(t)

static inline int
timer_pending(struct timer_list *t)
{
	return callout_active(&t->co);
}

static inline unsigned long
round_jiffies_up(unsigned long j)
{
	clock_t hz = drv_usectohz(1000000);
	return ((j + hz - 1) / hz) * hz;
}

static inline unsigned long
round_jiffies_up_relative(unsigned long j)
{
	return round_jiffies_up(j);
}

#endif
