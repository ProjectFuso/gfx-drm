/* Public domain. */
/*
 * illumos: OpenBSD <sys/vnode.h> stub for DRM.
 * drm_drv.c includes this but does not use any vnode types directly.
 * On illumos, real vnode access is not needed by the DRM core.
 */
#ifndef _SYS_VNODE_COMPAT_H
#define _SYS_VNODE_COMPAT_H

/*
 * Forward-declare vnode_t. The system headers (vm/page.h etc.)
 * pulled in transitively by sys/systm.h need this typedef.
 */
struct vnode;
typedef struct vnode vnode_t;

#endif /* _SYS_VNODE_COMPAT_H */
