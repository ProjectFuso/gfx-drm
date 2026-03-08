/* Public domain. */

#ifndef _LINUX_SCHED_SIGNAL_H
#define _LINUX_SCHED_SIGNAL_H

/*
 * illumos: signal pending checks.
 *
 * On illumos, interruptible sleeps are handled via cv_wait_sig() in the
 * wait queue implementation.  The signal_pending() predicate is used in
 * outer retry loops; for Phase 1 we return 0 (no signals pending in the
 * polling sense) so that waits always complete unless woken by a cv.
 *
 * Phase 2: replace with issig(JUSTLOOKING) from <sys/klwp.h> if needed.
 */
#define signal_pending(y)		0
#define signal_pending_state(s, x)	0

#define send_sig(sig, task, priv)	do { } while (0)
#define allow_signal(sig)		do { } while (0)
#define recalc_sigpending()		do { } while (0)

#endif /* _LINUX_SCHED_SIGNAL_H */
