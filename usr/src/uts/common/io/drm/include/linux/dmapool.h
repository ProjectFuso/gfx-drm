/* SPDX-License-Identifier: MIT */
/* illumos stub for linux/dmapool.h */
#ifndef _LINUX_DMAPOOL_H_
#define _LINUX_DMAPOOL_H_

#include <linux/types.h>
#include <linux/dma-mapping.h>

/*
 * hat_getpfnum / kas — needed to convert kernel VA → physical address.
 * Forward-declare to avoid pulling in vm/hat.h which conflicts with
 * our Linux-compat struct page.
 */
#ifndef __linux__
struct hat;
struct as;
extern pfn_t hat_getpfnum(struct hat *, caddr_t);
extern struct as kas;
#endif

/*
 * illumos_va_to_pa — convert kernel virtual address to physical address.
 *
 * The device DMA-reads command buffer headers using physical (bus) addresses.
 * On illumos, kernel VA != PA, so we use hat_getpfnum to translate.
 */
static inline dma_addr_t
illumos_va_to_pa(void *addr)
{
	pfn_t pfn = hat_getpfnum(kas.a_hat, (caddr_t)addr);
	return ((dma_addr_t)pfn << PAGE_SHIFT) |
	    ((uintptr_t)addr & (PAGE_SIZE - 1));
}

/*
 * struct dma_pool — illumos implementation.
 *
 * Encodes the allocation size and alignment.  The alignment is critical:
 * vmwgfx writes the DMA address of command buffer headers to
 * SVGA_REG_COMMAND_LOW with the low 6 bits used for the CB context
 * (SVGA_CB_CONTEXT_MASK = 0x3f).  The address must therefore be
 * 64-byte aligned, matching the pool's align parameter.
 *
 * We use a stored-original-pointer trick to support aligned allocation
 * without a separate free-list: store the raw kmalloc result just before
 * the aligned payload, then recover it in dma_pool_free.
 */
struct dma_pool {
	size_t size;
	size_t align;
};

static inline struct dma_pool *
dma_pool_create(const char *name, struct device *dev,
    size_t size, size_t align, size_t boundary)
{
	struct dma_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);
	if (pool) {
		pool->size  = size;
		pool->align = (align < sizeof(void *)) ? sizeof(void *) : align;
	}
	return pool;
}

static inline void
dma_pool_destroy(struct dma_pool *pool)
{
	kfree(pool);
}

/*
 * __dma_pool_alloc_aligned — allocate pool memory with correct alignment.
 *
 * Layout of the raw allocation:
 *   [raw kmalloc base]
 *   ...padding to align boundary, leaving room for one void* before aligned ptr...
 *   [void *orig] ← stored here (sizeof(void*) bytes before aligned ptr)
 *   [aligned payload] ← returned to caller; also used as DMA address source
 *
 * On free, we read back orig from (ptr - sizeof(void*)) and kfree it.
 */
static inline void *
__dma_pool_alloc_aligned(struct dma_pool *pool, gfp_t flags, dma_addr_t *handle)
{
	size_t align = pool->align;
	size_t hdr   = sizeof(void *);   /* space to store original pointer */
	/* Worst-case padding: (align - 1) bytes; plus header before aligned start */
	size_t total = pool->size + align + hdr;
	char  *raw   = kmalloc(total, flags);
	char  *aligned;
	void **slot;

	if (!raw)
		return NULL;

	/*
	 * Find first align-boundary at or after (raw + hdr).
	 * This guarantees there are at least hdr bytes before aligned for slot.
	 */
	aligned = (char *)(((uintptr_t)(raw + hdr) + align - 1) & ~(align - 1));
	slot    = (void **)(aligned - hdr);
	*slot   = raw;

	*handle = illumos_va_to_pa(aligned);
	return aligned;
}

static inline void *
dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle)
{
	return __dma_pool_alloc_aligned(pool, mem_flags, handle);
}

static inline void
dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t addr)
{
	/* Recover the original kmalloc pointer stored before the aligned area */
	void *raw = *((void **)((char *)vaddr - sizeof(void *)));
	kfree(raw);
}

static inline void *
dma_pool_zalloc(struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle)
{
	void *addr = __dma_pool_alloc_aligned(pool, mem_flags | __GFP_ZERO, handle);
	return addr;
}

#endif /* _LINUX_DMAPOOL_H_ */
