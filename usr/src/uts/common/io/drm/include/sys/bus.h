/*
 * illumos DRM compat: OpenBSD bus_space / bus_dma type stubs.
 *
 * OpenBSD DRM uses bus_space_tag_t (MMIO mapping) and bus_dma_tag_t (DMA).
 * On illumos, the equivalents are ddi_acc_handle_t and DDI DMA handles.
 * For Phase 1, define these as opaque pointer types so the copied OpenBSD
 * DRM source files compile; proper DDI wiring is done in the driver shim.
 *
 * Public domain.
 */

#ifndef _SYS_BUS_COMPAT_H
#define _SYS_BUS_COMPAT_H

#include <sys/types.h>

/*
 * bus_space_tag_t / bus_space_handle_t:
 * OpenBSD MMIO mapping handles.  On illumos we use ddi_acc_handle_t.
 * Stub as void * so the TTM / DRM sources that carry OpenBSD signatures
 * compile without modification.
 */
typedef void *bus_space_tag_t;
typedef uintptr_t bus_space_handle_t;
typedef size_t bus_size_t;

/*
 * bus_dma_tag_t:
 * OpenBSD DMA tag.  On illumos the equivalent is ddi_dma_handle_t, but
 * Phase 1 only uses kmem_alloc-backed coherent memory; stub as void *.
 */
typedef void *bus_dma_tag_t;

#endif /* _SYS_BUS_COMPAT_H */
