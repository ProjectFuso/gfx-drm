/* Public domain. */
/*
 * illumos: stub for OpenBSD <sys/timeout.h>.
 * OpenBSD DRM uses struct timeout for vblank disable timer.
 * On illumos, map struct timeout to callout_t.
 */
#ifndef _SYS_TIMEOUT_COMPAT_H
#define _SYS_TIMEOUT_COMPAT_H

#include <sys/callout.h>	/* callout_t */

/*
 * struct timeout: OpenBSD kernel timeout.  On illumos we back it with a
 * callout_t so that code embedding struct timeout can be compiled.
 */
struct timeout {
	callout_t	to_callout;
};

static inline void
timeout_set(struct timeout *to,
    void (*fn)(void *), void *arg)
{
	callout_init(&to->to_callout, 0);
}

static inline void
timeout_del(struct timeout *to)
{
	callout_stop(&to->to_callout);
}

static inline void
timeout_del_barrier(struct timeout *to)
{
	callout_stop(&to->to_callout);
}

static inline int
timeout_pending(struct timeout *to)
{
	return callout_active(&to->to_callout);
}

#endif /* _SYS_TIMEOUT_COMPAT_H */
