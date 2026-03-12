/* Public domain. */

#ifndef _ASM_PAGE_H
#define _ASM_PAGE_H

/*
 * illumos: On x86-64, page size is always 4096. Use compile-time constants
 * so that array sizes depending on PAGE_SIZE/PAGE_SHIFT are valid.
 */
#ifndef PAGE_SIZE
#define PAGE_SIZE	4096UL
#endif
#ifndef PAGE_SHIFT
#define PAGE_SHIFT	12
#endif
#ifndef PAGE_MASK
#define PAGE_MASK	(~(PAGE_SIZE - 1UL))
#endif

/* PFN_UP/PFN_DOWN/PFN_PHYS are defined in linux/mm.h — do not redefine here */

#endif /* _ASM_PAGE_H */
