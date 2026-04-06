# Goal A — Full vmwgfx with 3D acceleration (kernel side)

## Current state (summary)

Desktop already lights up via direct SVGA register writes in
`vmwgfx_drv.c:1695-1713` (illumos branch of fbdev bring-up) plus the VIS
framebuffer console in `vmwgfx_illumos.c`. The DRM char node `/dev/dri/card0`
is created in `vmwgfx_attach`, and user ioctls route through
`drm_illumos_open` → `drm_ioctl`. All vmwgfx 3D ioctls
(`DRM_VMW_EXECBUF`, `DRM_VMW_CREATE_CONTEXT`, `DRM_VMW_CREATE_SURFACE`,
`DRM_VMW_GB_SURFACE_CREATE{,_EXT}`, `DRM_VMW_CREATE_SHADER`,
`DRM_VMW_GET_3D_CAP`, …) are wired into the ioctl table unchanged
(`vmwgfx_drv.c:170-259`). KMS is atomic-capable (`DRIVER_ATOMIC` in
`driver_features`, `vmwgfx_drv.c:1625`). IRQ, fences, MOBs, OTables,
command-buffer manager, STDU/Screen-Object/LDU display units are all
compiled unchanged and gated only by runtime device caps.

The port therefore has a high chance of running SVGA3D immediately once a
compatible userland (Mesa `vmwgfx`/`svga` gallium driver) is present. The
kernel-side gaps below are what still blocks a working 3D pipeline.

## Gaps blocking 3D (ordered by severity)

### A1. `vmwgfx_supported()` disables the backdoor on illumos

`vmwgfx_drv.c:1341-1355` only has `CONFIG_X86` / `CONFIG_ARM64` branches.
On illumos neither is set, so the `#else` path runs:
`drm_warn_once("vmwgfx is running on an unknown architecture.")` and
returns `false`. That forces `vmw_disable_backdoor()` at
`vmwgfx_drv.c:911`, which disables the SVGA backdoor channel
(`vmwgfx_msg.c`). `DRM_VMW_MSG` then fails, VMware-Tools-style userland
breaks, and Mesa's host-log / GL renderer-string negotiation may misbehave.

**Fix:** add an illumos branch (x86 only) that runs the same `cpuid`
hypervisor-vendor probe as Linux and returns `true` for `VMware` /
`KVM`/`QEMU` as appropriate. Either add a compile macro (`-D__x86` style)
or detect via `#if defined(__sun) && (defined(__amd64) || defined(__i386))`.

### A2. Render node is not exposed

`vmwgfx_illumos.c:508` creates only the primary minor (`card0`).  DRI3
clients and Mesa open `/dev/dri/renderD128` to get an unprivileged,
`DRM_RENDER_ALLOW` fd for EXECBUF.  Without it, every 3D client must be
DRM master, which breaks multi-client desktops.

**Fix:** register a second minor (`ddi_display:drm`, instance 128) and
teach `drm_illumos_open` to select `drm->render` when the caller opens a
render minor. `drm_minor_register` already creates `drm->render` in
`drm_drv.c` — just wire a second `ddi_create_minor_node` and select the
right `drm_minor *` from the opened slot.

### A3. PRIME / dma-buf is fully stubbed

Required for DRI3 (current X server default) and Wayland under vmwgfx.
Current state:
- `drm_linux.c:2354-2381`: `dma_buf_export/get/fd` return `-ENOTSUP`.
- `include/linux/dma-buf.h:80-82`: `dma_buf_attach` returns NULL;
  `dma_buf_vmap`/`dma_buf_mmap` return `-ENOSYS`.
- `drm_prime.c` has large `#ifdef __linux__` blocks; internal handle/fd
  translation returns `-ENOSYS` at lines 1073, 1103;
  `drm_gem_prime_mmap` returns `-ENOSYS` at 806-809.

**Fix:** implement an illumos file-descriptor-backed dma-buf.  Minimal
path for vmwgfx-self-share (DRI3 across a single device):
1. Add a `drm_illumos_dmabuf` struct holding a GEM handle + inode + refcnt.
2. Allocate an illumos `file_t` via `falloc()` / custom fops, use
   `setf(fd, fp)` to install it.
3. `drm_gem_prime_handle_to_fd` wraps the GEM BO in the dmabuf file;
   `drm_gem_prime_fd_to_handle` unwraps via `getf(fd)`.
4. `mmap` on the dmabuf fd routes through the same `devmap` path already
   used by `/dev/dri/card0`.

Cross-device import (from e.g. amdgpu later) can be deferred.

**OpenBSD reference:** `drm_linux.c:2578-2668` implements this in ~80
lines. `dma_buf_export()` allocates a kernel file via `fnew(p)`, sets
`f_type = DTYPE_DMABUF` with custom fileops, stores dmabuf in `f_data`.
`dma_buf_get(fd)` does `fd_getfile()` + type-check. `dma_buf_fd()`
does `fdalloc()` + `fdinsert()`. OpenBSD also stubs `dma_buf_attach`
(returns NULL) — cross-device is deferred there too. Our approach is
validated by theirs.

### A4. `vmwgfx_page_dirty.c` dirty tracking uses Linux MM primitives

Uses `clean_record_shared_mapping_range`, `wp_shared_mapping_range`,
`unmap_shared_mapping_range`, `vm_fault_t`, `struct vm_fault`.  The compat
layer does not provide these.  Without them `vmw_bo_dirty_scan_pagetable`
and `vmw_bo_vm_mkwrite`/`vmw_bo_vm_fault` cannot detect writes to
user-mapped MOB-backed surfaces, so host-side surface contents can stay
stale.  Impact: CPU-writable 3D textures / dynamic vertex buffers may
render incorrectly; most GL draws still work because most buffers are
GPU-written.

**Fix options:**
- Wire dirty tracking through the illumos devmap access-mode bits. The
  `devmap_access()` callback can intercept write faults and set a bit in
  per-BO state; `vmw_bo_dirty_scan_pagetable` then reads that bit.
- Alternative: implement `VMW_BO_DIRTY_MKWRITE` over illumos `segmap`
  page-protection change + explicit `hat_unload` + `devmap_do_fault`.
- If neither is worth building yet: gate page-dirty tracking at build
  time and force `VMW_BO_DIRTY_NONE`, accepting that user-writable
  texture coherency requires an explicit `SYNCCPU` call from userland
  (Mesa already issues `DRM_VMW_SYNCCPU` around CPU reads/writes, so in
  practice this is OK).

### A5. `vmw_generic_ioctl` wrapper is Linux-only

`vmwgfx_drv.c:1262-1319` enforces `DRM_MASTER|CAP_SYS_ADMIN` on
`DRM_VMW_UPDATE_LAYOUT` and opens an EXECBUF fast path. Under illumos the
call goes straight through `drm_ioctl`; the fast path is lost (minor
perf) and the permission check is absent (security issue for multi-user
hosts).

**Fix:** replicate the wrapper inside `drm_illumos_ioctl` — before the
generic dispatch, special-case these two ioctl numbers.

### A6. TTM BO mmap fault path is `VM_FAULT_SIGBUS`

`ttm/ttm_bo_vm.c:352` onwards (illumos `#else`) returns
`VM_FAULT_SIGBUS`. Does not matter for vmwgfx today because
`drm_illumos_gem_ttm_devmap` populates pages via `ttm_tt_populate` and
hands them to `devmap_umem_setup` up-front, so no faults occur at access
time. **But** any userland that relies on demand-paging (lazy mmap after
BO create, growing BOs) will fail.

**Fix (when needed):** implement a proper devmap `access` callback that
calls `ttm_tt_populate`/`ttm_tt_swapin` on demand and loads pages into
the mapping.

### A7. `schedule_timeout` does not wake early

`drm_linux.c:180-188` uses `delay()` unconditionally and cannot be cut
short by `wake_up_process`.  GPU scheduler TDR (`drm_sched`) and some
fence-wait fallback paths use this.  Under heavy GPU load the watchdog
may fire later than Linux would, but execution remains correct.

**Fix:** convert `schedule_timeout` to a per-task `cv_timedwait_sig`
against `current`'s TSD, matched to `wake_up_process`.

### A8. TTM swap is `-ENOSYS`

`ttm/ttm_tt.c:254` / `:318` — `ttm_tt_swapin`/`ttm_tt_swapout` return
`-ENOSYS` on illumos.  Under memory pressure BOs cannot be paged to
swap storage, which may OOM-kill the vmwgfx module.  Low priority —
vmwgfx in a VM usually has bounded VRAM and system memory.

**Fix (nice-to-have):** back TTM swap onto illumos `vmem`/`vnode` or
simply `vm_object`-style shadow buffers in kmem.

### A9. `vmw_irq_install` uses only DDI_INTR_TYPE_FIXED

`drm_illumos_irq.c:71` hard-codes fixed interrupts. vmwgfx works fine
with INTx but MSI/MSI-X is available on modern SVGA3 devices. Not
blocking for correctness.

**Fix:** try `DDI_INTR_TYPE_MSIX`, then `DDI_INTR_TYPE_MSI`, then
`DDI_INTR_TYPE_FIXED`. Pass through the vector count via `num_irq_vectors`.

### A10. `page_to_pfn` / `pfn_to_page` are stubs

`include/linux/mm.h:41` returns 0. Various callers reach this through
GEM/TTM helpers.  TTM's own pool supplies real PFNs via `hat_getpfnum`,
so vmwgfx's MOB paths are fine in practice — but any caller that starts
from a `struct page` obtained elsewhere and converts to a PFN is broken.

**OpenBSD reference:** their `struct page` IS `struct vm_page` (UVM),
so `page_to_pfn(pp)` is just `VM_PAGE_TO_PHYS(pp) / PAGE_SIZE` and
`pfn_to_page(pfn)` is `PHYS_TO_VM_PAGE(ptoa(pfn))` — zero-cost macros.

**Why we can't use native illumos `page_t` directly:**

Using illumos `page_t` as Linux `struct page` (the OpenBSD approach)
is not feasible on illumos for several reasons:

1. **Name collision:** both illumos and Linux define `struct page`.
   illumos `page_t` is `typedef struct page { ... }`. Including
   `<vm/page.h>` in any translation unit that also sees the Linux
   compat `struct page` is a hard conflict. (OpenBSD avoids this
   because UVM uses `struct vm_page` — a different tag name.)

2. **Field incompatibility:** Linux DRM accesses `page->lru`
   (list_head for chaining, used by vmwgfx_validation.c), `page->kaddr`
   (kernel VA, used by page_address()), `page->_refcount`. illumos
   `page_t` has none of these fields. Its list linkage (`p_next`,
   `p_prev`, `p_vpnext`, `p_vpprev`) is owned by the VM subsystem.

3. **Ownership model mismatch:** illumos `page_t` instances are
   managed by the VM system — they live in the global page hash,
   are subject to page scanning and pageout, and require
   `page_lock`/`page_unlock` protocols. Linux DRM's `struct page`
   from `alloc_pages` represents "I own this physical memory" with
   simple refcounting. On OpenBSD/UVM, `uvm_pglistalloc()` gives
   detached pages with a compatible ownership model. On illumos,
   getting free physical pages means `page_create_va()` which ties
   pages to a vnode+offset pair.

4. **Allocation model:** our `alloc_pages()` uses `kmem_alloc`, our
   TTM pool uses `ddi_umem_alloc`. Switching to page-level VM
   allocation would require choosing vnodes+offsets for DRM memory,
   managing `p_selock`, and dealing with the page scanner — a
   complete rework of the memory model.

**Fix — shim with real PFN tracking:**

Add a `_pfn` field to the existing `struct page` shim and maintain
a global PFN→page hash table for the reverse lookup:

```c
struct page {
    struct list_head lru;
    void            *kaddr;
    unsigned long    index;
    unsigned int     _refcount;
    pfn_t           _pfn;        /* physical frame number */
};
```

Implementation:

1. **Forward direction (`page_to_pfn`):** `alloc_pages()` calls
   `hat_getpfnum(kas.a_hat, kaddr)` and stores it in `page->_pfn`.
   `ttm_pool_alloc()` already calls `hat_getpfnum` — just also store
   the result in the page shim.
   ```c
   #define page_to_pfn(pp)   ((pp)->_pfn)
   #define page_to_phys(pp)  ((uint64_t)(pp)->_pfn << PAGE_SHIFT)
   ```

2. **Reverse direction (`pfn_to_page`):** maintain a global hash
   table (`mod_hash_t` or a simple power-of-2 chained hash) mapping
   `pfn_t → struct page *`. Insert at alloc time, remove at free
   time.
   ```c
   #define pfn_to_page(pfn)  drm_illumos_pfn_to_page(pfn)
   ```
   Use a hash table — get it working before optimizing.

3. **Update `dma_map_page`:** currently stubbed or using
   `hat_getpfnum` ad-hoc. With `page->_pfn` available, it becomes
   `(dma_addr_t)page->_pfn << PAGE_SHIFT` — one macro.

4. **Update `ttm_pool_alloc`:** the `hat_getpfnum` call at
   `ttm_pool.c:110` should also store into `tt->pages[i]->_pfn`
   and insert into the hash.

This is foundational — it unblocks B3, D6, and `dma_map_page`
correctness across all drivers.

## Suggested kernel work order for Goal A

(Updated based on OpenBSD DRM analysis — see `plans/openbsd_drm_analysis.md`)

1. A10 — `page_to_pfn` / `pfn_to_page` (**moved up — foundational**).
   OpenBSD gets this for free from UVM (`VM_PAGE_TO_PHYS`). We must
   store PFN at `alloc_pages` time and build a reverse lookup. This
   unblocks amdgpu (B3/D6) too, so solving it now pays off across
   all goals.
2. A1 — enable backdoor (trivial, 1 commit).
3. A2 — render node (small; unblocks DRI3 clients). OpenBSD does this
   as a minor-number range check (0-63 = primary, 128-191 = render)
   in their `drmopen()` — no separate device node needed. See R2 for
   the minor encoding fix that enables this.
4. A5 — ioctl permission wrapper (small).
5. A3 — PRIME/dma-buf for single-device DRI3 (medium, blocks real
   wayland). OpenBSD validates this approach: ~80 lines of fd-table
   ops using `fnew()` + `DTYPE_DMABUF`. Our illumos equivalent uses
   `falloc()`/`setf()`/`getf()` + vnode backing. Note: OpenBSD also
   does NOT implement `dma_buf_attach` (returns NULL) — cross-device
   is deferred there too.
6. A4 — dirty tracking (medium; can initially be gated off with SYNCCPU
   mandated).
7. A9 — MSI-X (small, quality-of-life). Consider per-driver IRQ wiring
   instead of a shared bridge — OpenBSD stubs `request_irq` to no-op
   and wires IRQs per-driver in attach. See R4 notes.
8. A7 — `schedule_timeout` wake path (medium; only matters under load).
9. A6 — demand-paging TTM fault (defer until userland needs it).
   OpenBSD implements this via UVM fault handler with `pmap_enter()`;
   our equivalent would be a `devmap_access()` callback.
10. A8 — TTM swap (defer).

## What is NOT a blocker for 3D

- `vmwgfx_ttm_buffer.c` VRAM `bus.addr` shim (already working).
- KMS atomic path (already active).
- IRQ wiring (already working).
- Command buffer manager / MOB / OTables (already working).
- SG DMA mode (intentionally forced to coherent; fine for SVGA3).
- fbdev emulation (replaced by direct SVGA modeset + VIS console).
- sysfs/debugfs (no-ops are acceptable).
