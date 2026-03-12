/* Public domain. */

#ifndef _LINUX_ERRNO_H
#define _LINUX_ERRNO_H

#include <sys/errno.h>

/*
 * illumos: many of these are already defined in sys/errno.h.
 * Guard each one with #ifndef to avoid -Werror redefinition warnings.
 */
#define ERESTARTSYS	EINTR
#ifndef ETIME
#define ETIME		ETIMEDOUT
#endif
#define EREMOTEIO	EIO
#ifndef ENOTSUPP
#define ENOTSUPP	ENOTSUP
#endif
#ifndef ENODATA
#define ENODATA		ENOTSUP
#endif
#ifndef ECHRNG
#define ECHRNG		EINVAL
#endif
#define EHWPOISON	EIO
#ifndef ENOPKG
#define ENOPKG		ENOENT
#endif
#ifndef EMULTIHOP
#define EMULTIHOP	EIPSEC
#endif
#ifndef EBADSLT
#define EBADSLT		EINVAL
#endif
#define ENOKEY		ENOENT
#define EPROBE_DEFER	EAGAIN
#ifndef ENOLINK
#define ENOLINK		EIO
#endif

/* illumos: BSD ELAST = highest errno value; ESTALE=151 is the last illumos one */
#ifndef ELAST
#define ELAST		151
#endif

#endif
