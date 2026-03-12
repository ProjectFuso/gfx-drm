/* Public domain. */
/*
 * illumos: OpenBSD <sys/selinfo.h> stub.
 * OpenBSD DRM uses struct selinfo (select/poll notification) and kqueue
 * knote/klist types.  On illumos, poll notification uses pollwakeup().
 * For Phase 1, provide stub types that allow the OpenBSD code to compile.
 */
#ifndef _SYS_SELINFO_COMPAT_H
#define _SYS_SELINFO_COMPAT_H

#include <sys/types.h>

/*
 * struct knote / struct klist / struct filterops: OpenBSD kqueue types.
 * DRM uses these for device poll/kqueue support.  On illumos kqueue is not
 * available; stub out the types so the non-__linux__ code compiles.
 */
struct filterops;	/* forward for kn_fop */

struct knote {
	int			 kn_filter;	/* kqueue filter type (EVFILT_*) */
	int			 kn_flags;
	int			 kn_sfflags;	/* filter-specific requested flags */
	int			 kn_fflags;	/* filter-specific flags set */
	void			*kn_hook;
	const struct filterops	*kn_fop;	/* filter ops pointer */
};

struct klist {
	int	_pad;
};

struct filterops {
	int	f_flags;	/* FILTEROP_ISFD etc. */
	int	(*f_attach)(struct knote *);
	void	(*f_detach)(struct knote *);
	int	(*f_event)(struct knote *, long);
};

/* OpenBSD filterops flags */
#define FILTEROP_ISFD	0x0001	/* fd-based filter */

/* OpenBSD kqueue filter types */
#define EVFILT_READ	(-1)
#define EVFILT_WRITE	(-2)
#define EVFILT_DEVICE	(-13)	/* OpenBSD device event filter */

/* OpenBSD kqueue note flags */
#define NOTE_SUBMIT	0x0001	/* submitted from within filter callback */

/*
 * struct selinfo: OpenBSD select/poll info embedded in file descriptors.
 * On illumos, poll is handled via pollwakeup(pollhead_t *).
 * Stub: si_note is a klist that we never populate.
 */
struct selinfo {
	struct klist	si_note;
};

/* klist operations — stubs (no kqueue on illumos) */
static inline void
klist_insert_locked(struct klist *kl, struct knote *kn)
{
	(void)kl; (void)kn;
}

static inline void
klist_remove_locked(struct klist *kl, struct knote *kn)
{
	(void)kl; (void)kn;
}

static inline void
knote_locked(struct klist *kl, long hint)
{
	(void)kl; (void)hint;
}

/* selwakeup: stub (use pollwakeup in a later phase) */
static inline void
selwakeup(struct selinfo *sip)
{
	(void)sip;
}

/*
 * NOTE_CHANGE: kqueue change event constant (used in DRM hotplug).
 * Value matches OpenBSD/BSD kqueue.h.
 */
#define NOTE_CHANGE	0x0001

/*
 * OpenBSD sleep priority constants.
 * On illumos these are not used by cv_wait; define them for compilation.
 */
#ifndef PZERO
#define PZERO	22
#endif
#ifndef PWAIT
#define PWAIT	32
#endif
#ifndef PCATCH
#define PCATCH	0x100
#endif

/*
 * INFSLP: OpenBSD "sleep forever" timeout value (uint64_t, nanoseconds).
 */
#ifndef INFSLP
#define INFSLP	UINT64_MAX
#endif

/*
 * msleep_nsec / wakeup: OpenBSD channel-based sleep.
 *
 * msleep_nsec(ident, mtx, priority, wmesg, nsecs):
 *   Drops mtx, sleeps until wakeup(ident) is called or nsecs nanoseconds
 *   have elapsed (INFSLP = sleep forever).  Returns 0 on wakeup, EINTR if
 *   interrupted by a signal (when priority & PCATCH), EWOULDBLOCK on timeout.
 *
 * wakeup(ident): wake all threads sleeping on ident.
 *
 * Forward declaration of struct mutex to avoid pulling in linux/mutex.h here.
 */
struct mutex;	/* forward: struct mutex { krwlock_t rw; } in linux/mutex.h */
int  msleep_nsec(void *ident, struct mutex *mtx, int priority,
	const char *wmesg, uint64_t nsecs);
void wakeup(void *ident);

#endif /* _SYS_SELINFO_COMPAT_H */
