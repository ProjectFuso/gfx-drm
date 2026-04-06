/* Public domain. */
/*
 * illumos: <sys/vnode.h> shim for DRM.
 *
 * This shim sits in the DRM linux-compat include path and ensures the real
 * illumos sys/vnode.h is also processed.  Without it, system headers that
 * transitively include <sys/vnode.h> (e.g. sys/vfs_opreg.h → sys/vfs.h)
 * would only find this shim and never see the full type definitions
 * (vopstats_t, vnodeops_t, caller_context_t, vattr_t …).
 *
 * We use #include_next so GCC continues searching past this directory and
 * picks up /usr/include/sys/vnode.h with the complete definitions.
 */
#ifndef _SYS_VNODE_COMPAT_H
#define _SYS_VNODE_COMPAT_H

#include_next <sys/vnode.h>

#endif /* _SYS_VNODE_COMPAT_H */
