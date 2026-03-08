/* Public domain. */

#ifndef _LINUX_SWAP_H
#define _LINUX_SWAP_H

/*
 * normally clock.h would be indirectly included via
 *
 * linux/memcontrol.h
 * linux/writeback.h
 * linux/blk-cgroup.h
 * linux/blkdev.h
 * linux/sched/clock.h
 */
#include <linux/sched/clock.h>

/*
 * normally module.h would be indirectly included via
 *
 * linux/memcontrol.h
 * linux/cgroup.h
 * linux/cgroup-defs.h
 * linux/bpf-cgroup.h
 * linux/bpf.h
 * linux/module.h
 */
#include <linux/module.h>

/* illumos: uvm/uvm_extern.h removed; stub swap accounting for Phase 1 */

static inline long
get_nr_swap_pages(void)
{
	/* Phase 1 stub: illumos swap info not wired yet */
	return 0;
}

/*
 * XXX For now, we don't want the shrinker to be too aggressive, so
 * pretend we're not called from the pagedaemon even if we are.
 */
static inline int
current_is_kswapd(void)
{
	return 0;
}

#endif
