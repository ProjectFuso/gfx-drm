/* Public domain. */

#ifndef _LINUX_SLAB_H
#define _LINUX_SLAB_H

#include <sys/kmem.h>
#include <sys/types.h>

#include <linux/types.h>
#include <linux/workqueue.h>
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

struct kmem_cache;

static inline struct kmem_cache *
kmem_cache_create(const char *name, size_t size, size_t align,
    unsigned long flags, void (*ctor)(void *))
{
	return (struct kmem_cache *)kmem_cache_create(name, size, align,
	    (void (*)(void *, kmem_cache_t *, unsigned long))ctor, NULL, NULL,
	    NULL, NULL, 0);
}

static inline void *
kmem_cache_alloc(struct kmem_cache *cache, int flags)
{
	int kflags = (flags & KM_NOSLEEP) ? KM_NOSLEEP : KM_SLEEP;
	return kmem_cache_alloc((kmem_cache_t *)cache, kflags);
}

static inline void
kmem_cache_free(struct kmem_cache *cache, void *obj)
{
	kmem_cache_free((kmem_cache_t *)cache, obj);
}

static inline void
kmem_cache_destroy(struct kmem_cache *cache)
{
	kmem_cache_destroy((kmem_cache_t *)cache);
}

#endif
