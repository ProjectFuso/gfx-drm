/* Public domain. */

#ifndef _ASM_PGTABLE_H
#define _ASM_PGTABLE_H

#include <linux/types.h>

/*
 * illumos: page protection type.
 * On illumos x86, memory type attributes go through the HAT layer.
 * We define pgprot_t as a plain ulong carrying x86 PTE flag bits.
 * The write-combine and no-cache helpers set standard x86 PTE bits;
 * actual caching policy is enforced when the mapping is created via
 * hat_devload() or ddi_dma_mem_alloc() with the right access attributes.
 */
typedef unsigned long pgprot_t;

#define pgprot_val(p)		(p)
#define pgprot_decrypted(p)	(p)
#define PAGE_KERNEL		0UL
#define PAGE_KERNEL_IO		0UL

/*
 * x86 PTE flag bits (identical on i386 and amd64).
 * Defined here to avoid pulling in OpenBSD machine/pte.h.
 */
#if defined(__i386__) || defined(__amd64__)
#ifndef PG_V
#define PG_V	0x001UL		/* valid / present */
#define PG_RW	0x002UL		/* read/write */
#define PG_WT	0x008UL		/* write-through */
#define PG_N	0x010UL		/* no-cache (PCD) */
#define PG_PAT	0x080UL		/* PAT bit (4K pages) */
#endif

/* Linux aliases */
#define _PAGE_PRESENT	PG_V
#define _PAGE_RW	PG_RW
#define _PAGE_PAT	PG_PAT
#define _PAGE_PWT	PG_WT
#define _PAGE_PCD	PG_N
#endif /* __i386__ || __amd64__ */

/*
 * Write-combine: PWT=1, PCD=0 selects PAT entry for WC when PAT is
 * configured (which the illumos boot path arranges on x86).
 * No-cache: PWT=0, PCD=1.
 */
static inline pgprot_t
pgprot_writecombine(pgprot_t prot)
{
#if defined(__i386__) || defined(__amd64__)
	/* Clear PCD, set PWT — PAT entry 1 = WC */
	return (prot & ~PG_N) | PG_WT;
#else
	return prot;
#endif
}

static inline pgprot_t
pgprot_noncached(pgprot_t prot)
{
#if defined(__i386__) || defined(__amd64__)
	/* Set PCD — uncacheable */
	return prot | PG_N;
#else
	return prot;
#endif
}

#endif /* _ASM_PGTABLE_H */
