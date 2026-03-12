/* Public domain. */
/*
 * illumos page compat — block vm/page.h and forward-declare page_t.
 *
 * Include this BEFORE sys/sunddi.h in any Linux-compat header that
 * includes sys/sunddi.h. This prevents illumos's vm/page.h from
 * defining "struct page" (page_t), which conflicts with our Linux-compat
 * struct page in linux/gfp.h.
 *
 * We forward-declare "struct page" and "page_t" so that sys/ddidevmap.h
 * (pulled in by sys/sunddi.h) can compile its "page_t **pparray" declaration.
 */
#ifndef _LINUX_ILLUMOS_PAGE_COMPAT_H
#define _LINUX_ILLUMOS_PAGE_COMPAT_H

/*
 * Block vm/page.h before sys/sunddi.h can pull it in.
 * Forward-declare struct page + page_t for system headers that need it.
 */
#ifndef _VM_PAGE_H
#define _VM_PAGE_H
struct page;
typedef struct page page_t;
#endif

#endif /* _LINUX_ILLUMOS_PAGE_COMPAT_H */
