/* Public domain. */

#ifndef _LINUX_GFP_H
#define _LINUX_GFP_H

#include <sys/kmem.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/mmzone.h>

#define __GFP_ZERO		0x0001u	/* handled via kmem_zalloc */
#define __GFP_DMA32		0x0002u
#define __GFP_NOWARN		0
#define __GFP_NORETRY		0
#define __GFP_RETRY_MAYFAIL	0
#define __GFP_MOVABLE		0
#define __GFP_COMP		0
#define __GFP_KSWAPD_RECLAIM	0
#define __GFP_HIGHMEM		0
#define __GFP_RECLAIMABLE	0
#define __GFP_NOMEMALLOC	0

#define GFP_ATOMIC		KM_NOSLEEP
#define GFP_NOWAIT		KM_NOSLEEP
#define GFP_KERNEL		KM_SLEEP
#define GFP_USER		KM_SLEEP
#define GFP_HIGHUSER		KM_SLEEP
#define GFP_DMA32		KM_SLEEP
#define GFP_TRANSHUGE_LIGHT	KM_SLEEP

static inline bool
gfpflags_allow_blocking(const unsigned int flags)
{
	return (flags & KM_NOSLEEP) == 0;
}

/* page allocation stubs - full impl deferred to Phase 2 */
struct page;
struct page *alloc_pages(unsigned int gfp_mask, unsigned int order);
void __free_pages(struct page *page, unsigned int order);

static inline struct page *
alloc_page(unsigned int gfp_mask)
{
	return alloc_pages(gfp_mask, 0);
}

static inline void
__free_page(struct page *page)
{
	__free_pages(page, 0);
}

/* __get_free_page / free_page - Phase 2 */
static inline unsigned long
__get_free_page(unsigned int gfp_mask)
{
	return 0;
}

static inline void
free_page(unsigned long addr)
{
}

#endif
