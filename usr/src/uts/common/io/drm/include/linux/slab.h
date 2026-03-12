/* Public domain. */

#ifndef _LINUX_SLAB_H
#define _LINUX_SLAB_H

#include <sys/kmem.h>
#include <sys/types.h>

/*
 * OpenBSD-style malloc/free with type tags and M_* flags.
 * Many DRM source files use malloc(size, M_DRM, M_WAITOK|M_ZERO) and
 * free(ptr, M_DRM, size) from OpenBSD.  Provide the compat wrappers here
 * so any file that includes slab.h gets them.
 * Protected by _DRM_OPENBSD_MALLOC_DEFINED to avoid multiple definitions
 * (sys/specdev.h and linux/pci.h use the same guard).
 */
#ifndef _DRM_OPENBSD_MALLOC_DEFINED
#define _DRM_OPENBSD_MALLOC_DEFINED

#ifndef M_DRM
#define M_DRM		1
#endif
#ifndef M_WAITOK
#define M_WAITOK	0x0001
#define M_NOWAIT	0x0002
#define M_ZERO		0x0008
#define M_CANFAIL	0x0004
#endif

static inline void *
_drm_openbsd_malloc(size_t size, int type, int flags)
{
	(void)type;
	if (flags & M_ZERO)
		return kmem_zalloc(size, (flags & M_NOWAIT) ? KM_NOSLEEP : KM_SLEEP);
	return kmem_alloc(size, (flags & M_NOWAIT) ? KM_NOSLEEP : KM_SLEEP);
}

#undef malloc
#undef free
#define malloc(size, type, flags)  _drm_openbsd_malloc((size), (type), (flags))
#define free(ptr, type, size)      do { if (ptr) kmem_free((ptr), (size)); } while (0)

#endif /* _DRM_OPENBSD_MALLOC_DEFINED */

/* illumos: MIN may not be defined in headers we include here */
#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/processor.h>	/* for CACHELINESIZE */

#define ARCH_KMALLOC_MINALIGN	CACHELINESIZE

#define ZERO_SIZE_PTR		NULL
#define ZERO_OR_NULL_PTR(x)	((x) == NULL)

/*
 * illumos kmem_free() requires exact size.
 * We prefix each allocation with its size so kfree() can recover it.
 */
static inline void *
kmalloc(size_t size, int flags)
{
	size_t *p;
	int kflags = (flags & KM_NOSLEEP) ? KM_NOSLEEP : KM_SLEEP;

	if (size == 0)
		return NULL;
	p = kmem_alloc(size + sizeof(size_t), kflags);
	if (p == NULL)
		return NULL;
	*p = size;
	return p + 1;
}

static inline void *
kzalloc(size_t size, int flags)
{
	size_t *p;
	int kflags = (flags & KM_NOSLEEP) ? KM_NOSLEEP : KM_SLEEP;

	if (size == 0)
		return NULL;
	p = kmem_zalloc(size + sizeof(size_t), kflags);
	if (p == NULL)
		return NULL;
	*p = size;
	return p + 1;
}

static inline void *
kmalloc_array(size_t n, size_t size, int flags)
{
	if (n != 0 && SIZE_MAX / n < size)
		return NULL;
	return kmalloc(n * size, flags);
}

static inline void *
kcalloc(size_t n, size_t size, int flags)
{
	if (n != 0 && SIZE_MAX / n < size)
		return NULL;
	return kzalloc(n * size, flags);
}

static inline void
kfree(const void *ptr)
{
	size_t *p;
	if (ptr == NULL)
		return;
	p = (size_t *)ptr - 1;
	kmem_free(p, *p + sizeof(size_t));
}

static inline void *
krealloc(const void *ptr, size_t new_size, int flags)
{
	void *new_ptr;
	size_t old_size;

	if (ptr == NULL)
		return kmalloc(new_size, flags);
	old_size = *((size_t *)ptr - 1);
	new_ptr = kmalloc(new_size, flags);
	if (new_ptr == NULL)
		return NULL;
	memcpy(new_ptr, ptr, MIN(old_size, new_size));
	kfree(ptr);
	return new_ptr;
}

static inline size_t
ksize(const void *ptr)
{
	if (ptr == NULL)
		return 0;
	return *((size_t *)ptr - 1);
}

static inline void *
kvmalloc(size_t size, int flags)
{
	return kmalloc(size, flags);
}

static inline void *
kvmalloc_array(size_t n, size_t size, int flags)
{
	return kmalloc_array(n, size, flags);
}

static inline void *
kvcalloc(size_t n, size_t size, int flags)
{
	return kcalloc(n, size, flags);
}

static inline void *
kvzalloc(size_t size, int flags)
{
	return kzalloc(size, flags);
}

static inline void
kvfree(const void *ptr)
{
	kfree(ptr);
}

/*
 * illumos: kmem_cache_create/alloc/free/destroy clash with illumos function
 * names (sys/kmem.h). Rename wrappers with drm_ prefix, then macro-define
 * the Linux names to call our wrappers.  The function bodies execute before
 * the #define macros are set, so internal calls refer to the illumos externs.
 */
static inline struct kmem_cache *
drm_kmem_cache_create(const char *name, size_t size, size_t align,
    unsigned long flags, void (*ctor)(void *))
{
	/*
	 * illumos kmem_cache_create: char*, size, align,
	 *   ctor(void*,void*,int), dtor(void*,void*),
	 *   reclaim(void*), privarg, vmem*, flags.
	 * Pass NULL for ctor/dtor/reclaim; DRM doesn't use illumos-style ctors.
	 */
	return (struct kmem_cache *)kmem_cache_create((char *)(uintptr_t)name,
	    size, align, NULL, NULL, NULL, NULL, NULL, 0);
}
#define kmem_cache_create	drm_kmem_cache_create

static inline void *
drm_kmem_cache_alloc(struct kmem_cache *cache, int flags)
{
	int kflags = (flags & KM_NOSLEEP) ? KM_NOSLEEP : KM_SLEEP;
	return kmem_cache_alloc((kmem_cache_t *)cache, kflags);
}
#define kmem_cache_alloc	drm_kmem_cache_alloc

static inline void
drm_kmem_cache_free(struct kmem_cache *cache, void *obj)
{
	kmem_cache_free((kmem_cache_t *)cache, obj);
}
#define kmem_cache_free		drm_kmem_cache_free

static inline void
drm_kmem_cache_destroy(struct kmem_cache *cache)
{
	kmem_cache_destroy((kmem_cache_t *)cache);
}
#define kmem_cache_destroy	drm_kmem_cache_destroy

#endif
