# Goal D — Full Wayland with AMDGPU (kernel side)

Builds on Goals A, B, C. This document covers only the additional
kernel-side gaps for a *production-quality* Wayland experience: 3D
acceleration, multi-display, HDR, VRR, suspend/resume, video decode,
and general robustness.

## What "full" adds on top of Goal C

- GL/Vulkan 3D acceleration via Mesa/RADV/radeonsi.
- Video decode via VCN (VA-API / VDPAU backends in userland).
- Multi-monitor, hotplug, DP-MST topology.
- HDCP, HDR metadata, YCbCr pipelines.
- Variable refresh rate (FreeSync / VRR).
- Suspend / resume (S3 and D3cold).
- Hang recovery / GPU reset.
- Power management with DPM table switching.

## Additional kernel gaps

### D1. Full 3D command submission path

AMDGPU GFX ring + compute rings + SDMA rings + scheduler TDR must all
work. This includes:

- `amdgpu_cs_ioctl` (GL/Vulkan command submission).
- `amdgpu_gem_userptr_ioctl` (userptr BOs for shader scratch, optional
  but used by RADV).
- `drm_sched_main` kthread per ring — relies on `kthread_*` (already
  functional) and `dma_fence` (already functional).
- `amdgpu_job` / timeout detect response (TDR) — requires working
  `schedule_timeout` wake-up semantics (see Goal A / A7). TDR firing
  late is OK; firing at a *wrong* monotonic time or never firing is
  not OK.

**Fix:** resolve A7 (schedule_timeout wake-early) before 3D becomes
load-bearing.

### D2. GPU reset path

`amdgpu_device_gpu_recover` needs BACO/mode1/mode2 reset sequences.
Requires SMU / PSP firmware commands to reinit. Relies on:
- Stopping all rings & draining fences (scheduler already supports).
- IRQ disable / re-enable (extend `drm_illumos_irq_install` to support
  enable/disable without free).
- PCI config state save/restore (`pci_save_state`, `pci_restore_state`)
  — currently stubbed in `pci.h`; implement via DDI config space save.

### D3. Userptr BOs / MMU notifier

`amdgpu_mn.c` / `amdgpu_hmm.c` use `mmu_interval_notifier` to invalidate
GPU page tables when userspace unmaps memory. illumos does not have
per-process MMU notifiers in the Linux sense. Options:
- Stub out userptr support entirely (Mesa/RADV tolerates this at a
  perf cost; some Vulkan extensions become unavailable).
- Build a notifier over illumos `as_unmap_wait` / a devmap `unmap`
  callback registered per-process.

Userptr is not required for basic GL/Vulkan; RADV can avoid it. Defer.

### D4. HMM / shared virtual memory

Required for KFD (compute). Not required for graphics. Skip.

### D5. Video decode (VCN)

VCN uses a dedicated ring that amdgpu manages like any other ring —
once D1 works, VCN rings are mostly additional firmware loads and
`amdgpu_uvd/vce/vcn.c` files.  Requires:
- Larger GTT allocations (D6).
- Per-decode-session DPB (decoded picture buffer) BOs.

No new kernel infrastructure beyond D1.

### D6. GTT / System memory apertures

AMDGPU GMC splits VRAM and GTT. GTT is GPU-pagetable-mapped system
memory. TTM backs this with scatterlists over real PFNs — requires
`page_to_pfn` (A10), `scatterlist` iterators (already functional).
For large GTT (several GB), the TTM pool allocator must scale. illumos
`ddi_umem_alloc` used by `ttm_pool.c` works for contiguous allocations
but for fragmented GTT, many smaller allocations are needed. Check
allocation throughput.

### D7. DP-MST (daisy-chained / hub monitors)

`drm_dp_mst_topology.c` is compiled with one `__sun` guard (skipped aux
sysfs registration). MST hotplug is driven by HPD IRQs from the DP
sink; amdgpu DC dispatches these. Should work; validate after D1.

### D8. HPD (Hot-Plug Detect)

AMDGPU uses a GPIO-chip abstraction for HPD pins. Currently GPIO is
not provided in the compat layer. DC has its own `dc_link_detect` that
reads HPD directly from GPU registers — check whether it bypasses the
Linux GPIO framework entirely (it should).

### D9. Suspend / Resume (S3 / S0ix)

Requires:
- DDI `devo_power` + `detach(DDI_SUSPEND)` / `attach(DDI_RESUME)` hooks
  wired into `amdgpu_device_suspend` / `amdgpu_device_resume`.
- `pm_runtime_*` becoming real instead of stubs.
- PCI config space save/restore (D2).
- Firmware reload after resume (some ASICs).

Defer until basic operation is stable.

### D10. HDCP

Currently in the "move to illumos replacement unit" bucket
(`display/drm_hdcp_helper.c`). Requires PSP-backed HDCP key negotiation
commands. Not blocking for a functional desktop — only needed for
protected content playback (Netflix DRM etc).

### D11. HDR / Color management

`drm_color_mgmt.c`, CRTC gamma/degamma LUTs, CTM, etc. — upstream
compiles clean. DC's HDR metadata path needs per-plane property
setup. Should work through the atomic helpers once they are
fully wired. Additional kernel support required: none beyond core
atomic modesetting.

### D12. VRR / FreeSync

`drm_connector_attach_vrr_capable_property`, VRR window on atomic
commits. Upstream-native. Needs accurate vblank timestamps (C6) and
correct page-flip completion events (C2). No new kernel work beyond
what Goal C demands.

### D13. Explicit sync

`drm_syncobj_eventfd_ioctl`, `DRM_IOCTL_SYNCOBJ_TRANSFER`,
timeline semaphores (Vulkan). Timeline syncobjs use `dma_fence_chain`
which is implemented (`drm_linux.c:2143-2351`). Eventfd is not
implemented (illumos has `port_create`/`port_alert` but no eventfd).

**Fix:** either stub eventfd (Vulkan drivers can still function, at
a perf cost) or implement eventfd on top of illumos `port` events.

### D14. TTM swap (revisit)

For memory pressure from running many Wayland clients, large textures,
and video decode DPBs, TTM swap becomes relevant. See Goal A / A8.

### D15. IOMMU (revisit)

With IOMMU stubbed, amdgpu DMA goes direct-physical. For SR-IOV or
secure boot scenarios, IOMMU support is needed. Not relevant to a
single-user desktop.

### D16. Firmware versioning & upgrades

`request_firmware` (Goal B / B1) must respect multiple firmware version
candidates (e.g. `sdma_ucode.bin` vs `sdma_ucode_XX_YY.bin`). AMDGPU
tries several names — the implementation must be able to return
-ENOENT from one and find the next.

### D17. `drm_client_setup_with_fourcc_format` / initial framebuffer

For a seamless transition from bootloader/firmware framebuffer
("seamless boot") to AMDGPU with the same content, DC supports a
"takeover" path. Requires reading the current scanout config from
AMDGPU hardware at attach. Quality-of-life only.

## Summary

Goal D is not one more lump of work — it is ~15 independent items, most
of them deferrable until driven by a concrete user complaint. The
critical ones for a **functional** full-wayland amdgpu desktop are:

- D1 — 3D command submission (blocks Mesa/Vulkan acceleration).
- D13 — sync_file + explicit sync for modern Mesa.
- D2 — GPU reset (any 3D workload will eventually hit a hang).
- D9 — suspend/resume (desktop use cases demand it).

The rest are incremental quality / coverage / robustness items.

## Cross-cutting: compat-layer hardening

By the time Goal D is being addressed, the illumos compat layer will
have ~15-20 new subsystems that were added ad-hoc. Before declaring
"done" it is worth:
- Auditing `drm_linux.c` for correctness under concurrent load (the
  file has grown to 3303 lines of state-heavy code).
- Adding CTF types for every exposed structure (for DTrace).
- Running the drm-tests suite at `/opt/drm-test/` (already installed)
  and tracking regressions.
- Considering whether some of the `drm_linux.c` subsystems should move
  into dedicated source files (idr/xa/dma_fence are the largest).

## OpenBSD reference findings

(See `plans/openbsd_drm_analysis.md` for full details.)

- **D3 (userptr / MMU notifier):** OpenBSD gates `mmu_interval_notifier`
  with `#ifdef __linux__`. amdkfd compute is fully included but
  userptr paths are excluded. Confirms: defer userptr, stub it out.

- **D13 (explicit sync / eventfd):** OpenBSD does NOT implement
  `eventfd`. Their sync_file is implemented (see C3) but eventfd-based
  signaling is absent. Confirms: stub eventfd initially; sync_file
  alone is sufficient for basic Vulkan.

- **D2 (GPU reset):** OpenBSD has the full amdgpu reset path compiled.
  PCI config save/restore would use their native PCI ops. Confirms
  we need real `pci_save_state`/`pci_restore_state` via DDI config
  space access, not stubs.

- **D9 (suspend/resume):** OpenBSD has ACPI integration for
  suspend/resume. Their `amdgpu_acpi.c` has one targeted workaround.
  illumos suspend is less mature; defer as planned.

- **Compat layer size:** OpenBSD's `drm_linux.c` is 3,982 lines
  (vs our 3,303). By Goal D our file will be similar or larger.
  The split-into-subsystems cleanup is important — OpenBSD keeps
  everything in one file and it's maintainable but dense.
