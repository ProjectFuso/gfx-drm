/* Public domain. */

#ifndef _LINUX_FCNTL_H
#define _LINUX_FCNTL_H

/*
 * O_CLOEXEC: close-on-exec flag, used in get_unused_fd_flags().
 * On Linux this is 02000000 (octal) = 0x80000.
 * On illumos it's also supported; use sys/fcntl.h definition if available,
 * otherwise use the Linux value.
 */
#ifndef O_CLOEXEC
#ifdef __sun
#include <sys/fcntl.h>
#ifndef O_CLOEXEC
#define O_CLOEXEC	0x800000
#endif
#else
#define O_CLOEXEC	02000000
#endif
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK	04000
#endif

#endif /* _LINUX_FCNTL_H */
