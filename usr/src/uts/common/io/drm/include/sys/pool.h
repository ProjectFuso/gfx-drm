/* Public domain. */
/*
 * illumos: OpenBSD sys/pool.h compat.
 * Implements struct pool + pool_* via illumos kmem_cache backing.
 * Must include linux/slab.h first to get kmem_cache_* wrappers.
 */
#ifndef _SYS_POOL_H
#define _SYS_POOL_H

#include <linux/slab.h>

/* OpenBSD pool allocation flags */
#ifndef PR_WAITOK
#define PR_WAITOK	KM_SLEEP
#endif
#ifndef PR_NOWAIT
#define PR_NOWAIT	KM_NOSLEEP
#endif
#ifndef PR_ZERO
#define PR_ZERO		0x0100	/* zero-fill allocation */
#endif
#ifndef IPL_NONE
#define IPL_NONE	0
#endif
#ifndef IPL_TTY
#define IPL_TTY		0
#endif
#ifndef IPL_HIGH
#define IPL_HIGH	0
#endif

struct pool {
	struct kmem_cache	*pl_cache;	/* illumos kmem_cache */
	size_t			 pl_size;
};

static inline void
pool_init(struct pool *p, size_t size, size_t align, int ipl,
    int flags, const char *name, void *allocator)
{
	(void)ipl; (void)flags; (void)allocator;
	p->pl_size = size;
	/* drm_kmem_cache_create is the 5-arg wrapper defined in slab.h */
	p->pl_cache = drm_kmem_cache_create(name, size,
	    align ? align : 1, 0, NULL);
}

static inline void *
pool_get(struct pool *p, int flags)
{
	int kflags = (flags & PR_NOWAIT) ? KM_NOSLEEP : KM_SLEEP;
	void *ptr = drm_kmem_cache_alloc(p->pl_cache, kflags);
	if (ptr && (flags & PR_ZERO))
		bzero(ptr, p->pl_size);
	return ptr;
}

static inline void
pool_put(struct pool *p, void *ptr)
{
	drm_kmem_cache_free(p->pl_cache, ptr);
}

static inline void
pool_destroy(struct pool *p)
{
	drm_kmem_cache_destroy(p->pl_cache);
	p->pl_cache = NULL;
}

#endif
