/* Public domain. */
/* illumos: linux_logo.h stub — Linux boot logo not available on illumos */

#ifndef _LINUX_LINUX_LOGO_H
#define _LINUX_LINUX_LOGO_H

#include <linux/types.h>

struct linux_logo {
	int		type;
	unsigned int	width;
	unsigned int	height;
	unsigned int	clutsize;
	const unsigned char *clut;
	const unsigned char *data;
};

#define LINUX_LOGO_MONO		1
#define LINUX_LOGO_VGA16	2
#define LINUX_LOGO_CLUT224	3
#define LINUX_LOGO_GRAY256	4

#endif /* _LINUX_LINUX_LOGO_H */
