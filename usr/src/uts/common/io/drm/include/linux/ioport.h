/* Public domain. */

#ifndef _LINUX_IOPORT_H
#define _LINUX_IOPORT_H

#include <linux/types.h>

typedef uint64_t resource_size_t;
typedef uint64_t phys_addr_t;

#define IORESOURCE_IO		0x00000100
#define IORESOURCE_MEM		0x00000200
#define IORESOURCE_PREFETCH	0x00001000
#define IORESOURCE_MEM_64	0x00100000

struct resource {
	resource_size_t	start;
	resource_size_t	end;
	unsigned long	flags;
	const char	*name;
};

static inline resource_size_t
resource_size(const struct resource *r)
{
	return r->end - r->start + 1;
}

#define DEFINE_RES_MEM(_start, _size)		\
(struct resource) {				\
		.start = (_start),		\
		.end = (_start) + (_size) - 1,	\
		.flags = IORESOURCE_MEM,	\
		.name = NULL,			\
	}

#endif
