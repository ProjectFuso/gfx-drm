/* Public domain. */

#ifndef _LINUX_MM_H
#define _LINUX_MM_H

#include <sys/types.h>
#include <sys/param.h>

/* illumos: removed OpenBSD UVM and libkern includes:
 *   sys/malloc.h, sys/stdint.h, sys/atomic.h, machine/cpu.h,
 *   uvm/uvm_extern.h, uvm/uvm_glue.h, lib/libkern/libkern.h
 */

#include <linux/slab.h>		/* kvmalloc / kvfree / kzalloc etc. */
#include <linux/shrinker.h>
#include <linux/overflow.h>
#include <linux/pgtable.h>

#define PageHighMem(x)	0

/*
 * illumos Phase 1: page address macros are stubs.
 * Full implementation requires tying into the illumos HAT/page layer.
 */
#define page_to_phys(page)	((uint64_t)0)
#define page_to_pfn(pp)		((uint64_t)0)
#define pfn_to_page(pfn)	((struct page *)NULL)
#define nth_page(page, n)	(&(page)[(n)])
#define offset_in_page(off)	((uintptr_t)(off) & PAGE_MASK)
#define set_page_dirty(page)	do { (void)(page); } while (0)

#define PAGE_ALIGN(addr)	(((addr) + PAGE_MASK) & ~PAGE_MASK)

#define PFN_UP(x)		(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)		((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)		((x) << PAGE_SHIFT)

bool is_vmalloc_addr(const void *);

/* kvmalloc/kvfree/kvcalloc/kvzalloc are provided by linux/slab.h */

/*
 * illumos Phase 1: vmalloc_to_page / virt_to_page return NULL.
 * Full implementation requires HAT KPM mapping.
 */
static inline struct page *
vmalloc_to_page(const void *va)
{
	return NULL;
}

static inline struct page *
virt_to_page(const void *va)
{
	return NULL;
}

static inline long
si_mem_available(void)
{
	/* Phase 1 stub */
	return 0;
}

/*
 * get_order(size) — smallest n such that 2^n * PAGE_SIZE >= size.
 * Uses GCC __builtin_clzl to avoid dependency on flsl().
 */
static inline unsigned int
get_order(size_t size)
{
	unsigned long n;

	if (size <= PAGE_SIZE)
		return 0;
	n = (size - 1) >> PAGE_SHIFT;
	return (unsigned int)(sizeof(unsigned long) * 8 - __builtin_clzl(n));
}

static inline long
totalram_pages(void)
{
	/* Phase 1 stub */
	return 0;
}

static inline bool
want_init_on_free(void)
{
	return false;
}

#endif
