/* Public domain. */
/*
 * illumos: OpenBSD UVM (unified virtual memory) compat stubs.
 *
 * DRM source files include <uvm/uvm_extern.h> for OpenBSD page management.
 * On illumos, physical page management uses the HAT and page_t infrastructure.
 * For Phase 1, provide stub types that allow compilation.
 */

#ifndef _UVM_UVM_EXTERN_H
#define _UVM_UVM_EXTERN_H

#include <sys/types.h>
#include <sys/rwlock.h>		/* for krwlock_t */
#include <linux/rwlock.h>	/* struct rwlock { krwlock_t rw; } */

/*
 * vaddr_t: virtual address (same width as a pointer)
 * voff_t: virtual offset (same as off_t)
 * vm_prot_t: VM protection flags
 * vm_fault_t: VM fault type
 */
typedef uintptr_t	vaddr_t;
typedef off_t		voff_t;
typedef int		vm_prot_t;
typedef int		vm_fault_t;

/* VM prot bits */
#define VM_PROT_NONE	0x00
#define VM_PROT_READ	0x01
#define VM_PROT_WRITE	0x02
#define VM_PROT_EXECUTE	0x04

/* VM fault types */
#define VM_FAULT_INVALID	0
#define VM_FAULT_PROTECT	1
#define VM_FAULT_WIRE		2

/*
 * vm_page_t: OpenBSD physical page handle.
 * On illumos, pages are managed via page_t. For Phase 1, use void * stub.
 */
typedef void *	vm_page_t;

/*
 * struct uvm_object: OpenBSD UVM object (analogous to Linux struct address_space).
 * DRM embeds this in gem objects for mmap/fault support.
 * On illumos, mmap uses the segmap/hat framework.
 */
struct uvm_object {
	struct rwlock		vmobjlock;	/* lock protecting this obj */
	const struct uvm_pagerops *pgops;	/* pager operations */
	int			uo_npages;	/* number of resident pages */
	int			uo_refs;	/* reference count */
};

/*
 * struct uvm_pagerops: pager operations table.
 * DRM registers pgo_reference, pgo_detach, pgo_fault, pgo_flush.
 */
struct uvm_faultinfo;	/* forward */

struct uvm_pagerops {
	void		(*pgo_reference)(struct uvm_object *);
	void		(*pgo_detach)(struct uvm_object *);
	int		(*pgo_fault)(struct uvm_faultinfo *, vaddr_t,
			    vm_page_t *, int, int, vm_fault_t, vm_prot_t, int);
	boolean_t	(*pgo_flush)(struct uvm_object *, voff_t, voff_t, int);
};

/*
 * struct uvm_faultinfo: page fault context.
 * Used as an argument to pgo_fault. Mostly opaque for Phase 1.
 */
struct vm_amap;
struct vm_map_entry {
	struct {
		struct uvm_object	*uvm_obj;	/* backing object */
	} object;
	struct {
		struct vm_amap		*ar_amap;	/* anonymous map */
	} aref;
	vm_prot_t		 protection;		/* current protection */
};

struct uvm_faultinfo {
	struct vm_map_entry	*entry;		/* the faulted entry */
	struct vm_map		*orig_map;	/* original map (for pmap) */
	int			 fault_type;
};

/*
 * uvm_obj_init: initialize a UVM object.
 * On illumos, just zeroes the reference count and stores the pager ops.
 */
static inline void
uvm_obj_init(struct uvm_object *uobj,
    const struct uvm_pagerops *pgops, int refs)
{
	uobj->pgops = pgops;
	uobj->uo_npages = 0;
	uobj->uo_refs = refs;
}

/*
 * uvm_obj_destroy: tear down a UVM object.
 * On illumos, the backing is managed differently; this is a stub.
 */
static inline void
uvm_obj_destroy(struct uvm_object *uobj)
{
	(void)uobj;
}

/* uvm_wait: sleep waiting for VM memory (stub) */
static inline void
uvm_wait(const char *wmesg)
{
	(void)wmesg;
	/* Phase 1: stub; real implementation would use cv_wait */
}

/* uvmfault_unlockall: release fault locks (stub) */
static inline void
uvmfault_unlockall(struct uvm_faultinfo *ufi,
    struct vm_amap *amap, struct uvm_object *uobj)
{
	(void)ufi; (void)amap; (void)uobj;
}

/*
 * vsize_t: virtual memory size type (same as size_t on most platforms)
 */
typedef size_t	vsize_t;

/*
 * OpenBSD cdevsw compat for drm_gem.c:udv_attach_drm().
 *
 * drm_gem.c checks cdevsw[major(device)].d_mmap to verify the device
 * is a DRM device before returning a UVM object.  On illumos this check
 * is replaced by a Phase 1 stub that always returns NULL (no mmap).
 *
 * drmmmap is declared as an opaque function pointer type so the comparison
 * compiles; udv_attach_drm() will always return NULL in Phase 1 anyway.
 */
/*
 * paddr_t: physical address type.
 * sys/bus.h may define it first; guard against redefinition.
 */
#ifndef _UVM_UVM_EXTERN_H_PADDR
#define _UVM_UVM_EXTERN_H_PADDR
typedef uintptr_t	paddr_t;
#endif

/*
 * UVM pager flags (used as bitmask arguments to pgo_fault).
 */
#define PGO_ALLPAGES	0x01	/* process all pages in range */
#define PGO_DONTCARE	((vm_page_t)(-1UL))	/* caller doesn't care */

/*
 * pmap: OpenBSD physical map (page table management).
 * Stubbed as opaque pointer; real wiring deferred to Phase 2.
 */
struct pmap;

struct vm_map {
	struct pmap	*pmap;
};

/*
 * Extend struct uvm_faultinfo with orig_map field.
 * (We re-declare by adding orig_map to the base struct.)
 */
/* orig_map is already in the struct definition above */

/*
 * Extend vm_map_entry with protection field.
 */

/*
 * pmap operations — stubs, do nothing.
 */
static inline int
pmap_enter(struct pmap *pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	(void)pmap; (void)va; (void)pa; (void)prot; (void)flags;
	return 0;
}

static inline void
pmap_update(struct pmap *pmap)
{
	(void)pmap;
}

#define PMAP_CANFAIL	0x0001	/* fail gracefully if no memory */

/*
 * round_page: round a value up to the next page boundary.
 * PAGE_SIZE comes from <sys/param.h> which is included by most callers.
 */
#ifndef round_page
#define round_page(x)  (((uintptr_t)(x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#endif
#ifndef trunc_page
#define trunc_page(x)  ((uintptr_t)(x) & ~(PAGE_SIZE - 1))
#endif

/*
 * struct pglist: OpenBSD physical page list (used in TTM).
 * On OpenBSD it's a TAILQ of vm_page.  Phase 1: stub as a simple struct
 * that satisfies TAILQ_INIT() — callers only use it as an opaque container.
 */
#include <sys/queue.h>
struct _vm_page_stub { TAILQ_ENTRY(_vm_page_stub) pageq; };
struct pglist { struct _vm_page_stub *tqh_first; struct _vm_page_stub **tqh_last; };

/*
 * OpenBSD VM kernel memory allocation (km_alloc/km_free).
 * These use "virtual type" and "physical type" descriptors.
 * On illumos, we simply use kmem_alloc/kmem_free.
 */
struct kmem_va_mode  { int _dummy; };
struct kmem_pa_mode  { int _dummy; };
struct kmem_dyn_mode { int _dummy; };

/* Global allocation modes — just markers, not used by illumos backend */
extern const struct kmem_va_mode  kv_any;
extern const struct kmem_pa_mode  kp_zero;
extern const struct kmem_dyn_mode kd_waitok;
extern const struct kmem_dyn_mode kd_nowait;

static inline void *
km_alloc(size_t size, const struct kmem_va_mode *kv,
    const struct kmem_pa_mode *kp, const struct kmem_dyn_mode *kd)
{
	(void)kv; (void)kp; (void)kd;
	/* Phase 1: map to kmem_zalloc (kp_zero is always used in DRM code) */
	return kmem_zalloc(size, KM_SLEEP);
}

static inline void
km_free(void *ptr, size_t size, const struct kmem_va_mode *kv,
    const struct kmem_pa_mode *kp)
{
	(void)kv; (void)kp;
	if (ptr)
		kmem_free(ptr, size);
}

/*
 * UVM anonymous object (swap storage) operations.
 * On illumos, swap-backed objects are handled differently.
 * Phase 1: stub as no-ops.
 */
struct uvm_object;

static inline void
uao_detach(struct uvm_object *uobj)
{
	(void)uobj;
}

static inline int
uvm_obj_wire(struct uvm_object *uobj, voff_t start, voff_t stop,
    struct pglist *plist)
{
	(void)uobj; (void)start; (void)stop; (void)plist;
	return ENOMEM;
}

static inline void
uvm_obj_unwire(struct uvm_object *uobj, voff_t start, voff_t stop)
{
	(void)uobj; (void)start; (void)stop;
}

static inline void
uvm_pagecopy(vm_page_t src, vm_page_t dst)
{
	(void)src; (void)dst;
}

/*
 * PHYS_TO_VM_PAGE / VM_PAGE_TO_PHYS: OpenBSD physical page macros.
 * On illumos, physical page management is via page_t / HAT.
 * Phase 1 stub: return NULL / 0.
 */
#ifndef PHYS_TO_VM_PAGE
#define PHYS_TO_VM_PAGE(pa)	((vm_page_t)NULL)
#endif
#ifndef VM_PAGE_TO_PHYS
#define VM_PAGE_TO_PHYS(pg)	((paddr_t)0)
#endif

/*
 * bus types: include sys/bus.h compat for bus_dma_tag_t, bus_dma_segment_t,
 * bus_dmamem_mmap, BUS_DMA_NOCACHE.
 */
#include <sys/bus.h>

/* BUS_DMA_64BIT: allow 64-bit DMA addresses (OpenBSD extension) */
#ifndef BUS_DMA_64BIT
#define BUS_DMA_64BIT	0x2000
#endif

/*
 * cdevsw: OpenBSD character device switch table.
 * Only the d_mmap function pointer is used by DRM.
 * Defined in drm_linux.c as a stub array with NULL entries.
 */
struct _cdevsw_stub {
	void *d_mmap;
};

extern struct _cdevsw_stub cdevsw[];	/* stub defined in drm_linux.c */
extern void *drmmmap;			/* stub defined in drm_linux.c */

#endif /* _UVM_UVM_EXTERN_H */
