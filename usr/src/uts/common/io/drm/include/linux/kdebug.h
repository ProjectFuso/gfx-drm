/* Public domain. */
/* illumos: kdebug.h stub — kernel debugger notifiers not available on illumos */

#ifndef _LINUX_KDEBUG_H
#define _LINUX_KDEBUG_H

#include <linux/notifier.h>

enum die_val {
	DIE_OOPS = 1,
	DIE_INT3,
	DIE_DEBUG,
	DIE_PANIC,
	DIE_NMI,
};

struct die_args {
	struct pt_regs	*regs;
	const char	*str;
	long		err;
	int		trapnr;
	int		signr;
};

static inline int
register_die_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int
unregister_die_notifier(struct notifier_block *nb)
{
	return 0;
}

#endif /* _LINUX_KDEBUG_H */
