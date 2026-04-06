# Goal C — Minimal Wayland with AMDGPU (kernel side)

Builds on Goal A (PRIME/dmabuf, render node) and Goal B (amdgpu kernel
module reaching basic modesetting). This document lists only the
*additional* kernel-side gaps needed for a minimal Wayland compositor
(e.g. Weston with drm-backend, or a fresh `wlroots`-based compositor)
running on an AMDGPU device.

## What "minimal Wayland" needs from the kernel

A Wayland compositor using `drm-backend` requires:
1. A DRM primary fd (master) to do modeset and page flip.
2. A DRM render fd (render node) per client for GBM / EGL / GL.
3. dma-buf import on the compositor side to consume client buffers.
4. Atomic modesetting with proper page-flip completion events.
5. Vblank events delivered through `poll()` on the DRM fd.
6. `DRM_CAP_PRIME`, `DRM_CAP_CRTC_IN_VBLANK_EVENT`, `DRM_CAP_TIMESTAMP_MONOTONIC`.
7. sync primitives — either implicit fences on dma-buf (baseline) or
   explicit sync via `drm_syncobj` + sync_file (modern).

## Gaps beyond Goal A + B

### C1. dma-buf import (cross-buffer, possibly cross-device)

Goal A's dma-buf export covers single-device use.  For Wayland clients
to hand a buffer to the compositor that then imports it into amdgpu
(or into a scanout plane) you need **import** as well as export, and
the import path must work for buffers originating from the same
amdgpu device (the common case).

Path:
- `drm_gem_prime_fd_to_handle` → `drm_gem_prime_import` →
  `drm->driver->gem_prime_import` → `amdgpu_gem_prime_import`.
- Requires `drm_gem_prime_import_dev` and
  `dma_buf_dynamic_attach` / `dma_buf_map_attachment`.

**Fix:** extend the illumos dma-buf implementation (A3) to handle
`dma_buf_attach`, `dma_buf_map_attachment`, and `dma_buf_ops->pin/unpin`
(non-dynamic is OK).  Add a GEM prime import helper that wraps an
already-resident BO.

### C2. `drm_event` delivery via poll()

`drm_illumos_chpoll` (`drm_illumos_file.c:135-163`) already reports
POLLIN/POLLRDNORM based on `file_priv->event_list`.  Confirm the atomic
commit path actually queues a `drm_pending_vblank_event` / page-flip
event onto that list on completion.  This requires
`drm_send_event_locked` to function, which it should (it's Linux-source
unchanged), but the `struct drm_pending_event` lifecycle should be
tested end-to-end once AMDGPU is up.

Also: `drm_read` (the file `read` fop that userland calls after `poll`)
needs an illumos route.  `drm_illumos_file.c` does not currently expose
`cb_read`.  A Wayland compositor expects `read(drm_fd, &event_hdr,
sizeof(event_hdr))` to return the vblank/page-flip event.

**Fix:** add `cb_read` in the per-device driver (vmwgfx + amdgpu) that
dispatches to `drm_read(file_priv, buf, count, ppos)`.  Translate
illumos `uio_t` to a plain kernel copy-to-user via `uiomove`.

### C3. `drm_syncobj` and `sync_file` for explicit sync

`drm_syncobj.c` is compiled; ioctls should work if the dma_fence layer
works (it does, per drm_linux.c:1435-1959).  **But** `sync_file` is
stubbed (`drm_linux.c:2678-2692`).  Modern wayland + EGL (esp. with the
`EGL_ANDROID_native_fence_sync` / `EGL_KHR_wait_sync` path used by
Mesa for explicit sync) expects `sync_file` to produce a real fd.

**Fix:** implement `sync_file_create` / `sync_file_get_fence` over the
same dmabuf-style fd infrastructure (see A3).  A sync_file fd wraps a
`dma_fence *`; closing the fd drops the refcount.

### C4. Atomic page-flip path — end-to-end wiring check

AMDGPU's atomic path uses the helper flow
(`drm_atomic_helper_commit`/`_commit_tail`).  This flow calls
`drm_atomic_helper_wait_for_fences`, which waits on `dma_fence`s, and
`drm_atomic_helper_fake_vblank`, which relies on HRT timers.  The
`hrtimer` compat (if any) should be audited; today, `callout_t`-backed
timers are used for `timer.h` but not necessarily `hrtimer.h`.

**Fix:** either map `hrtimer_*` onto `callout_t` nsec resolution
(illumos has nanosecond callouts via `cyclic` already), or add a thin
hrtimer shim.

### C5. EDID over DC I2C / AUX

Goal B provides I2C via the DDC bit-bang helper. DisplayPort AUX
transactions go through `drm_dp_helper.c` (already compiled) but DC
overrides the AUX transfer function with its own HW-accelerated
implementation.  Confirm DC's AUX path works via DDI register
reads/writes — it should, no compat layer involvement.

### C6. Vblank accuracy

`drm_vblank.c` is in the "move to compat/header" bucket (has `__sun`
edits currently).  Check that `drm_crtc_vblank_count_and_time` returns
monotonically increasing timestamps and that
`CLOCK_MONOTONIC`-equivalent time source is used (`gethrtime()`).
Wayland compositors reject vblank events with non-monotonic timestamps.

### C7. Implicit sync on dma-buf

If explicit sync (C3) is not available, wayland fallbacks expect
implicit sync: importing a dma-buf with an attached
`dma_resv`-implicit-write-fence blocks GPU reads until the fence
signals.  This requires `dma_resv` to be functional across the dma-buf
boundary — both attach + usage tracking.  `dma-resv.h` header is
upstream; runtime (`drm_linux.c`) exposes `dma_fence` but needs to
verify `dma_resv_add_fence` / `dma_resv_wait_timeout` work under import.

## Summary of ordered kernel work for Goal C

1. A3 + C1 — full PRIME export/import via dma-buf fd.
2. C2 — `cb_read` wiring + DRM event delivery via poll+read.
3. C3 — sync_file implementation.
4. C4 — hrtimer compat.
5. C6 — vblank timestamp audit.
6. C7 — dma-resv fence-tracking audit.

No new large kernel subsystems beyond these — most of the heavy lifting
for Goal C is in Goal B. Once amdgpu is up and PRIME works, a minimal
wlroots/Weston should come up because AMDGPU's upstream atomic/DC code
already satisfies every other compositor expectation.

## OpenBSD reference findings

(See `plans/openbsd_drm_analysis.md` for full details.)

- **C1 (dma-buf import):** OpenBSD stubs `dma_buf_attach()` to return
  NULL and `dma_buf_detach()` to panic. Cross-device import is **not
  implemented** on OpenBSD either. This confirms that single-device
  self-import (same amdgpu exporting and importing) is sufficient for
  a first Wayland compositor. The compositor imports buffers from
  clients on the same GPU — the common case.

- **C2 (drm_event read):** OpenBSD implements `drmread()` directly in
  `drm_drv.c:1851` using `msleep_nsec()` + event dequeue + `uiomove()`.
  Clean, ~80 lines. Our `cb_read` approach should follow the same
  pattern: wait on `file_priv->event_wait`, dequeue from `event_list`,
  `uiomove()` to the illumos `uio_t`.

- **C3 (sync_file):** OpenBSD implements `sync_file_create()` and
  `sync_file_get_fence()` in `drm_linux.c:3251-3268` using the same
  fd-table pattern as dma-buf (~40 lines). `fnew(p)`, set
  `f_type = DTYPE_SYNC`, wrap a `dma_fence *`. All other fops
  (read, write, ioctl) return error stubs. **This is sufficient for
  Mesa/Vulkan explicit sync.** Our illumos equivalent uses
  `falloc()`/`setf()` + vnode wrapping the fence.

- **C4 (hrtimer):** Not visible in OpenBSD's compat layer. They may
  rely on `callout_t` / `timeout` which has adequate resolution.
  Needs investigation on illumos — cyclic or high-res callout may
  be needed.

- **C7 (implicit sync / dma_resv):** OpenBSD's `dma_resv` code
  compiles unchanged from Linux. Their `dma_fence` implementation
  is functional. This suggests our dma_fence layer (already mostly
  working) should carry dma_resv through without major issues.
