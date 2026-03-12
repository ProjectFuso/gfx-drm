/* Public domain. */

#ifndef _LINUX_VERSION_H
#define _LINUX_VERSION_H

/* Report Linux 6.12 for vmwgfx guest driver identification */
#define LINUX_VERSION_MAJOR	6
#define LINUX_VERSION_PATCHLEVEL	12
#define LINUX_VERSION_SUBLEVEL	0

#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + \
				 ((c) > 255 ? 255 : (c)))
#define LINUX_VERSION_CODE \
	KERNEL_VERSION(LINUX_VERSION_MAJOR, LINUX_VERSION_PATCHLEVEL, \
		       LINUX_VERSION_SUBLEVEL)

#endif /* _LINUX_VERSION_H */
