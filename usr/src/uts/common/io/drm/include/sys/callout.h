/* Public domain. */
/*
 * illumos: callout_t and callout_* API using the public timeout(9F) interface.
 *
 * OpenBSD DRM code uses callout_t (a simple struct) with callout_init/reset/
 * stop/drain/active operations.  On illumos, the equivalent public API is
 * timeout(9F) / untimeout(9F), declared in <sys/systm.h>.
 */
#ifndef _SYS_CALLOUT_COMPAT_H
#define _SYS_CALLOUT_COMPAT_H

#include <sys/types.h>		/* timeout_id_t */
#include <sys/systm.h>		/* timeout(), untimeout() */

/*
 * callout_t: wrapper around the opaque timeout_id_t handle.
 */
typedef struct _drm_callout {
	timeout_id_t	id;		/* 0 when idle/expired */
} callout_t;

static inline void
callout_init(callout_t *c, int mpsafe)
{
	(void)mpsafe;
	c->id = 0;
}

static inline void
callout_reset(callout_t *c, clock_t ticks, void (*func)(void *), void *arg)
{
	if (c->id != 0) {
		(void) untimeout(c->id);
		c->id = 0;
	}
	if (ticks <= 0)
		ticks = 1;
	c->id = timeout(func, arg, ticks);
}

static inline int
callout_stop(callout_t *c)
{
	if (c->id != 0) {
		(void) untimeout(c->id);
		c->id = 0;
		return 1;
	}
	return 0;
}

static inline void
callout_drain(callout_t *c)
{
	(void) callout_stop(c);
}

static inline int
callout_active(callout_t *c)
{
	return c->id != 0;
}

#endif /* _SYS_CALLOUT_COMPAT_H */
