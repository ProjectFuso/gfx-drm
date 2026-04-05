# Goal B — Basic modesetting for AMDGPU (kernel side)

## Current state

**Zero** amdgpu kernel driver code is present in `usr/src/uts/`.

What exists already:
- `linux-drm/amd/` — full upstream reference tree (~2525 files):
  amdgpu/ (571), amdkfd/ (66), display/dc/ (973 files, 243 subdirs), pm/
  (269), include/ (515), amdxcp/, acp/.
- ABI headers: `include/uapi/drm/amdgpu_drm.h`, `include/drm/amd_asic_type.h`,
  `include/linux/acpi_amd_wbrf.h` (stub).
- GPU scheduler (`scheduler/sched_main.c`, `sched_entity.c`,
  `sched_fence.c`) — operational with one minor `__sun` limitation.
- TTM (13 .c + 10 headers) — operational with stubbed swap and fault.
- Unrelated: legacy DRM1 `radeon` driver at `usr/src/uts/intel/io/radeon/`
  (pre-KMS, not usable for modern AMD GPUs).

"Basic modesetting" target: bring up a display, read EDID, set a mode,
show a framebuffer. No 3D, no compute, no video decode.

## Scope and scale

Even a stripped-down amdgpu kernel module is large. AMDGPU is monolithic:
you cannot meaningfully skip GFX engine bring-up because IP discovery,
SMU, GMC and the display subsystem all depend on common bring-up paths.
A realistic minimum port is:

- `amdgpu/*.c` core bring-up (~100-150 files of 571) — device init, IP
  discovery, GMC, ATOM BIOS, PSP firmware loading, SMU/power, IH (IRQ
  handler), SDMA, GFX (minimal), display manager glue (`amdgpu_dm.c` +
  helpers).
- `amd/display/dc/` — this is the display core. Cannot be skipped for
  Navi/RDNA/DCN GPUs. This subtree alone is 973 files across 243
  subdirectories. It is the single biggest porting burden after the
  amdgpu core itself.
- `amd/pm/` SMU drivers for the target ASIC(s).
- `amd/include/` headers (515 files) — mostly register definitions, flat
  copy should work.

For a *very* narrow first target, pick a single ASIC family (e.g. a
single DCN generation such as DCN1/Raven or DCN3/Navi21) and only port
the IP blocks that exact GPU uses. AMD's `amdgpu_discovery.c` reads the
IP table out of VRAM at bring-up and picks functions at runtime, so
unused IP blocks simply don't get called — but they still must compile.

## Kernel compatibility-layer gaps that block AMDGPU

These are in addition to the vmwgfx gaps (many are shared; fix those
first). AMDGPU exercises far more of the Linux kernel API surface than
vmwgfx.

### B1. Firmware loading (`request_firmware`)

Not implemented. AMDGPU needs dozens of signed firmware blobs loaded at
init (PSP, SMU, GFX MEC/PFP/ME, SDMA, DMCUB, VCN, RLC, …). Without them
bring-up stops at PSP load.

**Fix:** implement `request_firmware` / `release_firmware` on top of an
illumos kernel file read (`vn_open`, `vn_rdwr`) from e.g.
`/usr/lib/firmware/amdgpu/` or `/kernel/firmware/amdgpu/`. Respect
`dev->driver->name` prefix.

### B2. IOMMU bridge

Currently a no-op (`drm_linux.c:2916-2953`). AMDGPU GPUVM uses IOMMU
mapping when present (`amdgpu_iommu.c`, dma_map_page). If IOMMU can be
stubbed to "not present" the driver falls back to direct DMA — acceptable
for a first port.

**Fix:** keep stubs for now, route `dma_map_page` through DDI DMA.
Revisit when compute / SR-IOV comes online.

### B3. `page_to_pfn` / `pfn_to_page` must return real values

AMDGPU GPUVM writes GPU page tables directly from CPU `struct page`
pointers. The current stub will silently fault the GPU.

**Fix:** store PFN at allocation time in `struct page` shim; back-map
`pfn_to_page` via a PFN → struct-page lookup table. See Goal A / A10.

### B4. I2C bus needs a real backend

`drm_linux.c:1189-1216` delegates `i2c_transfer` to `algo->master_xfer`
if set, but the DDC bit-banging driver on the card uses the native
i2c-algo-bit framework calling back into `algo->getsda` / `setsda` /
`getscl` / `setscl`. Those callbacks must be wired. Without I2C, EDID
read returns zero and connector probe fails.

**Fix:** implement a minimal `i2c-algo-bit` that drives GPIO through
the amdgpu DC I2C engine callbacks (which talk to hardware registers
directly — no host I2C controller needed; it's the GPU's own I2C).

### B5. Multi-vector MSI-X IRQ

AMDGPU uses MSI/MSI-X with an interrupt handler chip abstraction (IH
ring). `drm_illumos_irq.c` is single-vector fixed only.

**Fix:** extend to `DDI_INTR_TYPE_MSIX` with N vectors, let AMDGPU's
`amdgpu_irq.c` dispatch by vector ID. See Goal A / A9.

### B6. Large-BAR resource mapping

AMDGPU often has a 16GB+ framebuffer BAR. `pci.h` and
`drm_illumos_pci.c` need to handle 64-bit BARs (they already do;
verify). TTM IO region mapping for large BARs needs
`ddi_regs_map_setup` with no-cache attributes and may need
`ddi_mem_alloc`-style partial mapping for systems that cannot map 16GB
into KVA. Check limits.

### B7. Dynamic Power Management call-ins

AMDGPU's SMU/PP layer calls `pm_runtime_get_sync`,
`pm_runtime_put_autosuspend`, `dev_pm_*` — currently stubs
(`drm_linux.c:1232`). Stubs that always return "device is on" are fine
for a first port (no runtime PM). Later add real D3cold if/when illumos
ACPI allows it.

### B8. ACPI for display detection

AMDGPU/DC reads ACPI tables (`_DSM`, `_DDC`) for connector topology on
laptops and some desktops. `drm_linux.c:1236-1315` stubs ACPI.

**Fix (optional for server cards):** either leave stubs and rely on
ATOM BIOS parsing of connector table (works on discrete desktop cards),
or add a minimal ACPI bridge using illumos `acpica` for `_DSM` evaluation.

### B9. DMI / quirks

`dmi_match` returns false (`drm_linux.c:519-578`). AMDGPU uses DMI for
panel orientation quirks and a few ASIC-specific workarounds. Acceptable
to stub for desktop cards.

### B10. Compound pages / `folio` APIs

Not used by vmwgfx, used heavily by amdgpu_ttm.c (`hmm_range_fault`,
`mmu_interval_notifier`, `folio_*`). For basic modesetting (no user-
pointer BO, no userptr range fault), these paths can be `#ifdef
CONFIG_HMM_MIRROR`-gated off. Userptr is a 3D/compute feature.

### B11. DRM fb_helper / generic fbdev

Currently disabled (`drm_linux.c:3229` stub). For *basic modesetting*
AMDGPU expects `drm_client_register` → generic fb client to push an
initial modeset. Needs either:
- enable the fbdev client path (noisy, touches many files), or
- follow the vmwgfx model: directly call `drm_client_modeset_commit`
  after `amdgpu_device_init` to push an initial modeset.

### B12. Build infrastructure for a separate amdgpu module

No Makefile.mod for amdgpu yet. Needs a new
`usr/src/uts/intel/amdgpu/Makefile` (see `usr/src/uts/intel/vmwgfx/`)
plus an object list. Consider whether amdgpu should be one giant module
or split (e.g., `amdgpu`, `amdgpu-dc`). Upstream keeps it as one.

## Required new illumos bridge code

- `amdgpu_illumos.c` — DDI attach/detach, VIS console glue (optional,
  may defer to a generic "drm_illumos_fb_console" helper), PCI wrapper.
- `drm_illumos_firmware.c` — `request_firmware` implementation.
- `drm_illumos_i2c.c` — i2c-algo-bit bridge.
- Extend `drm_illumos_irq.c` for MSI-X.
- Extend `drm_illumos_pci.c` for PCIe capability walks (PCIe link speed,
  ASPM, FLR) if not already complete.

## Suggested phased approach

### Phase 0 — prerequisites
1. Render node support (shared with Goal A).
2. MSI-X IRQ support (shared with Goal A).
3. Real `page_to_pfn` / `pfn_to_page` (shared with Goal A).
4. Firmware loading from disk.

### Phase 1 — amdgpu compile
5. Copy `linux-drm/amd/` into `usr/src/uts/common/io/drm/amdgpu/` as a
   bulk import.
6. Add `usr/src/uts/intel/amdgpu/` module skeleton.
7. Get it to *compile* (expect a large volume of missing Linux KPIs;
   add stubs as they come up). Target: single ASIC family
   (pick Raven/DCN1 or Navi10/DCN2 — one has a wide HW testbase).

### Phase 2 — bring-up to IP init
8. Implement `amdgpu_illumos.c` attach path, PCI probe, IRQ.
9. Wire firmware loading.
10. Get `amdgpu_device_init` through IP discovery → GMC init.
11. Expect and debug errors in PSP / SMU firmware load.

### Phase 3 — display
12. `amdgpu_dm_init` — the bulk of DC compiles; expect many missing
    helpers in ttm/, drm_atomic_helper, drm_dp_mst.
13. Get connector probe + EDID read working (needs B4).
14. First `drm_mode_setcrtc` from userland modetest — this is "basic
    modesetting done".

## Realistic scope note

AMDGPU is an order of magnitude more complex than vmwgfx. A reasonable
estimate for reaching "basic modesetting lights up a screen on one
specific desktop GPU" is many months of focused work. The `amd/display/dc/`
subtree alone is larger than the entire current illumos DRM port. Before
committing to this goal, consider: is a narrower target GPU (e.g., a
specific Radeon RX card that uses DCN2.1) acceptable? That lets you
ignore large swaths of DC code for other DCN generations.
