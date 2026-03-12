/* SPDX-License-Identifier: MIT */
/* illumos stub for linux/dmapool.h */
#ifndef _LINUX_DMAPOOL_H_
#define _LINUX_DMAPOOL_H_

#include <linux/types.h>
#include <linux/dma-mapping.h>

struct dma_pool;

/* On illumos, DMA pools are not used; allocate directly */
static inline struct dma_pool *
dma_pool_create(const char *name, struct device *dev,
    size_t size, size_t align, size_t boundary)
{
	return (struct dma_pool *)(uintptr_t)size; /* encode size as cookie */
}

static inline void
dma_pool_destroy(struct dma_pool *pool)
{
}

static inline void *
dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle)
{
	/* pool encodes size */
	size_t size = (size_t)(uintptr_t)pool;
	void *addr = kmalloc(size, mem_flags);
	if (addr)
		*handle = (dma_addr_t)(uintptr_t)addr;
	return addr;
}

static inline void
dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t addr)
{
	kfree(vaddr);
}

static inline void *
dma_pool_zalloc(struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle)
{
	/* pool encodes size */
	size_t size = (size_t)(uintptr_t)pool;
	void *addr = kzalloc(size, mem_flags);
	if (addr)
		*handle = (dma_addr_t)(uintptr_t)addr;
	return addr;
}

#endif /* _LINUX_DMAPOOL_H_ */
