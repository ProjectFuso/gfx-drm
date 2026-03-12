/* Public domain. */
/*
 * illumos: OpenBSD <machine/bus.h> redirect.
 * DRM source files include <machine/bus.h> for bus_space / bus_dma types.
 * On illumos we provide these as opaque stubs in <sys/bus.h>.
 */
#include <sys/bus.h>
