/* Public domain. */
/*
 * illumos: sys/tsd.h stub.
 *
 * Thread-Specific Data (TSD) API.  On modern illumos this lives in the
 * kernel source tree; older or stripped installs may lack the installed
 * header.  Provide a minimal stub backed by POSIX pthread TSD for
 * kernel-context callers that use tsd_create/tsd_set/tsd_get/tsd_destroy.
 *
 * NOTE: real illumos TSD is implemented in the kernel (kthread_t slots).
 * This stub uses a simpler approach suitable for Phase 1 compilation.
 */
#ifndef _SYS_TSD_H
#define _SYS_TSD_H

#include <sys/types.h>

typedef uint_t tsd_key_t;

/*
 * tsd_create: allocate a new TSD key.
 * destructor is called (if non-NULL) when a thread exits with a non-NULL value.
 */
extern void tsd_create(uint_t *keyp, void (*destructor)(void *));

/*
 * tsd_destroy: release a TSD key.
 */
extern void tsd_destroy(uint_t *keyp);

/*
 * tsd_get: return this thread's value for the given key.
 */
extern void *tsd_get(uint_t key);

/*
 * tsd_set: set this thread's value for the given key.
 * Returns 0 on success, errno on failure.
 */
extern int tsd_set(uint_t key, void *value);

#endif /* _SYS_TSD_H */
