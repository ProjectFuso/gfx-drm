/* Public domain. */

#ifndef _LINUX_NUMA_H
#define _LINUX_NUMA_H

#define NUMA_NO_NODE	(-1)

/* Forward declaration: struct device is defined in linux/device.h */
struct device;

static inline int
dev_to_node(struct device *dev)
{
	return NUMA_NO_NODE;
}

#endif
