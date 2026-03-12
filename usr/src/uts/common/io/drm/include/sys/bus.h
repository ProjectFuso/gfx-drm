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
 * bus_dma_tag_t / bus_dmamap_t / bus_dma_segment_t:
 * OpenBSD DMA types.  On illumos the equivalents are DDI DMA handles/cookies.
 * Phase 1 stubs: opaque pointer / struct so the DRM sources compile.
 */
typedef void *bus_dma_tag_t;
typedef void *bus_dmamap_t;

struct bus_dma_segment {
	uintptr_t	ds_addr;	/* DMA address */
	size_t		ds_len;		/* length of transfer */
};
typedef struct bus_dma_segment bus_dma_segment_t;

/*
 * bus_dma flags (OpenBSD): used in pool allocations / dmamem calls.
 * Stub as 0 since actual DMA wiring is done in driver shims.
 */
#define BUS_DMA_NOWAIT		0x0001
#define BUS_DMA_ALLOCNOW	0x0002
#define BUS_DMA_ZERO		0x0010
#define BUS_DMA_WAITOK		0x0000

#endif /* _SYS_BUS_COMPAT_H */
