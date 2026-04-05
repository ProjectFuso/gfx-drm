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

**Fix:** make `alloc_pages` store the PFN at allocation time and have
`page_to_pfn` read it from the `struct page` shim.

## Suggested kernel work order for Goal A

1. A1 — enable backdoor (trivial, 1 commit).
2. A2 — render node (small; unblocks DRI3 clients).
3. A5 — ioctl permission wrapper (small).
4. A9 — MSI-X (small, quality-of-life).
5. A3 — PRIME/dma-buf for single-device DRI3 (medium, blocks real wayland).
6. A4 — dirty tracking (medium; can initially be gated off with SYNCCPU
   mandated).
7. A7 — `schedule_timeout` wake path (medium; only matters under load).
8. A6 — demand-paging TTM fault (defer until userland needs it).
9. A8 — TTM swap (defer).
10. A10 — `page_to_pfn` (defer until something trips it).

## What is NOT a blocker for 3D

- `vmwgfx_ttm_buffer.c` VRAM `bus.addr` shim (already working).
- KMS atomic path (already active).
- IRQ wiring (already working).
- Command buffer manager / MOB / OTables (already working).
- SG DMA mode (intentionally forced to coherent; fine for SVGA3).
- fbdev emulation (replaced by direct SVGA modeset + VIS console).
- sysfs/debugfs (no-ops are acceptable).
