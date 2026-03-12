/* Public domain. */

#ifndef _LINUX_FILE_H
#define _LINUX_FILE_H

/*
 * linux/file.h — illumos stub.
 * fd_install, fput, get_unused_fd_flags, put_unused_fd are inline stubs
 * in linux/fs.h. We include that rather than pulling in sys/systm.h
 * which would drag in sys/file.h (illumos struct file) and conflict
 * with our Linux-compat struct file definition.
 */
#include <linux/fs.h>

#endif /* _LINUX_FILE_H */
