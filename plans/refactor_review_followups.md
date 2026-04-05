# Refactor Review — Follow-up Items

Review findings on the `vmwgfx-refactor` branch (7 commits on top of
`vmwgfx-port`). The refactor extracted shared illumos bridges from
`vmwgfx_illumos.c` into three new drm-core units:

- `usr/src/uts/common/io/drm/drm_illumos_pci.c`
- `usr/src/uts/common/io/drm/drm_illumos_irq.c`
- `usr/src/uts/common/io/drm/drm_illumos_file.c`
- `usr/src/uts/common/io/drm/include/drm/drm_illumos.h`

Overall the split is clean and the pattern is good. The items below are
fixes / hardening tasks for an agent to work on. They are independent of
each other and can be picked up in any order.

---

## R1. `drm_illumos_file.c`: open/close slot allocation has no locking

**File:** `usr/src/uts/common/io/drm/drm_illumos_file.c:79-117`

The slot-scan loop in `drm_illumos_open` and the `in_use = false` clear
in `drm_illumos_close` run without synchronisation against each other
or against concurrent opens.

Concurrent scenarios that can go wrong:
- Two threads opening at the same time: both see the same slot as free,
  both bump `drm_dev_get` / `open_count`, one wins, the other silently
  corrupts state.
- A close racing an open on the *same* slot (after the minor was reused
  by the kernel) can clear `in_use` while the new opener is still
  populating `op->filp`.

**Fix:** add a `kmutex_t` to `struct drm_illumos_file_state`, initialise
it in a new `drm_illumos_file_init` helper, and hold it across:
- slot scan + `in_use = true` in `drm_illumos_open`;
- the `lookup_open` + `in_use = false` clear in `drm_illumos_close`.

`drm_illumos_ioctl` and `drm_illumos_chpoll` only read `in_use` and
deref `op->filp`, but once `drm_open_helper` has returned the filp is
stable for the lifetime of the open, so those only need a brief lock
for the lookup itself. Consider using a `krwlock_t` if contention
becomes a concern.

Also update `drm_illumos.h` to document the init/destroy contract.

---

## R2. `drm_illumos_file.c`: minor-slot encoding overlaps device instance

**File:** `usr/src/uts/common/io/drm/drm_illumos_file.c:47-49, 102`

```c
static int drm_illumos_minor_slot(dev_t dev) {
    return (int)(getminor(dev) & 0x3f);
}
...
*devp = makedevice(getmajor(*devp), (minor_t)slot);
```

`makedevice` replaces the original minor with just the slot number
(0..63), losing the driver instance bits entirely. Today this is masked
by `vmwgfx_global_state` being a single pointer (at most one vmwgfx
device per system), so every open resolves to the same state. When a
second DRM driver is ported (or a host has two vmwgfx devices somehow),
opens will misroute.

**Fix:** define a layout in `drm_illumos.h`:

- low 6 bits = open slot (current `DRM_ILUMOS_MAX_OPENS = 64`);
- mid bits = DRM node kind (primary/render/control);
- high bits = driver instance.

Provide encode/decode helpers and use them in every bridge caller.
Refactor `vmwgfx_cb_ioctl`'s own `VMWGFX_MINOR_SLOT` macro
(`vmwgfx_illumos.c:58`) to use the shared helper instead.

This also unblocks R5 (render-node support, see goal_a_vmwgfx_3d.md
item A2) by giving render-node opens a distinct minor encoding.

---

## R3. Return-sign convention is inconsistent across the bridge

**Files:**
- `drm_illumos_irq.c` — returns negative errno (`-ENODEV`, `-ENOMEM`)
- `drm_illumos_file.c` — returns positive errno (`ENXIO`, `EBUSY`,
  `EBADF`, `EINVAL`)

The vmwgfx caller `illumos_vmw_irq_install`
(`vmwgfx_illumos.c:121-124`) propagates the return value unchanged,
then the Linux-side caller in `vmwgfx_irq.c` compares it to zero for
success — which works, but is only right by coincidence.

**Fix:** pick one convention and document it at the top of
`drm_illumos.h`. Recommended: **positive errno** for everything that is
called from illumos `cb_ops` (they expect positive errno anyway), and
use Linux-style negative errno only inside functions called from Linux-
source code. Audit each bridge function, annotate its return
convention in the header, and fix mismatched callers.

---

## R4. `drm_illumos_irq.c` is hard-wired to DDI_INTR_TYPE_FIXED

**File:** `usr/src/uts/common/io/drm/drm_illumos_irq.c:71, 82`

`ddi_intr_get_nintrs(..., DDI_INTR_TYPE_FIXED, ...)` and
`ddi_intr_alloc(..., DDI_INTR_TYPE_FIXED, 0, 1, ...)` only allow
single-vector legacy INTx. vmwgfx works with INTx, but:

- Modern SVGA3 hardware and AMDGPU require MSI or MSI-X for correct
  operation (multi-vector IH ring, HPD, SDMA, etc.).
- Using INTx on shared bus segments causes spurious IRQ storms.

**Fix:** extend `drm_illumos_irq_install` to negotiate the interrupt
type in order MSI-X → MSI → FIXED, and to install `nvec` handlers
driven by a caller-supplied vector-dispatch table. Suggested shape:

```c
struct drm_illumos_irq_vector {
    irq_handler_t  handler;
    irq_handler_t  thread_fn;
    void          *dev_id;
    const char    *name;
};

int drm_illumos_irq_install_msix(dev_info_t *dip,
    struct drm_illumos_irq_state *irq,
    const char *taskq_name,
    uint_t nvec_requested,
    uint_t *nvec_actual,
    struct drm_illumos_irq_vector *vectors);
```

The existing single-vector API can be kept as a thin wrapper for
backwards compatibility with the current vmwgfx caller.

This is a prerequisite for Goal B (amdgpu) / goal doc item B5.

---

## R5. `drm_illumos_gem_ttm_devmap` populates the entire BO up front

**File:** `usr/src/uts/common/io/drm/drm_illumos_file.c:165-256`

`drm_illumos_gem_ttm_devmap` calls `ttm_tt_create` + `ttm_tt_populate`
for the whole BO before returning a devmap. For multi-MB vmwgfx BOs
this is fine; for multi-GB amdgpu BOs (VRAM or large GTT mappings) it
will:
- hold kernel memory for the full BO up front even if the client
  mmaps only a window,
- block the opening thread for potentially seconds.

**Fix (defer until Goal B is underway):** implement a devmap `access`
callback on the returned mapping that demand-pages by calling
`ttm_tt_populate` on individual page ranges. See goal_a_vmwgfx_3d.md
item A6 for context.

Not blocking for vmwgfx today. Noted so it is visible.

---

## R6. `drm_illumos_pci.c`: I/O BAR size mask truncated to 16 bits

**File:** `usr/src/uts/common/io/drm/drm_illumos_pci.c:59`

```c
size = (resource_size_t)((~(mask & ~0x3U) + 1U) & 0xFFFFU);
```

The `& 0xFFFFU` mask truncates I/O BAR sizes at 64 KB. Legacy I/O BARs
are indeed capped at 256 bytes typically, but PCIe devices can expose
larger I/O windows. Truncating the computed size silently narrows the
reported `pdev->resource[i].end`.

**Fix:** remove the `& 0xFFFFU` mask, or widen it to 32 bits for
consistency with the memory-BAR path a few lines below. Also handle
the case where `bar_lo == 0xFFFFFFFF` (unimplemented BAR) explicitly —
the current early-continue catches it but the comment should make it
obvious.

---

## R7. `drm_illumos_pci_init_device` does not enable BAR decoding

**File:** `usr/src/uts/common/io/drm/drm_illumos_pci.c:124-145`

After populating `pdev->resource[]`, the function does not set
`PCI_COMM_MAE` (memory access enable) or `PCI_COMM_ME` (bus master
enable) in the command register. vmwgfx is getting away with this
because the bootloader/firmware left the card enabled, which is fragile.

**Fix:** near the end of `drm_illumos_pci_init_device`, read
`PCI_CONF_COMM`, OR in `PCI_COMM_MAE | PCI_COMM_ME` (bus master
required for MSI/MSI-X and DMA), and write it back. Skip `PCI_COMM_IO`
unless the device actually exposes I/O BARs. Consider adding a
companion `drm_illumos_pci_disable_device` for the detach path.

---

## R8. `struct pci_bus` is barely populated

**File:** `usr/src/uts/common/io/drm/drm_illumos_pci.c:138-139`

```c
pdev->bus = &pdev->_bus;
pdev->bus->config_handle = cfg_handle;
```

Only `config_handle` is set on the embedded `_bus`. Any Linux DRM code
that walks `pdev->bus` (e.g., `pci_upstream_bridge`, `pci_bus->number`,
sibling iteration) will read zero / misbehave. vmwgfx does not walk the
bus tree, but amdgpu and its PSP/DC subsystems do.

**Fix:** decide whether to back `struct pci_bus` with real PCI tree
data from DDI (`ddi_get_parent`, `ddi_prop_lookup_int_array` for
`reg`/`bus-range`/`ranges`), or to explicitly stub the fields that
callers read (e.g., `bus->number`, `bus->parent = NULL`) with a
comment. At minimum add a comment in the header recording the
limitation so the next driver port doesn't fall into it.

---

## R9. No `drm_illumos_file_init` / `_destroy` helpers

**File:** `usr/src/uts/common/io/drm/drm_illumos_file.c`

`struct drm_illumos_file_state` is initialised by the caller using
`kmem_zalloc`, which works today but prevents adding non-zero-init
fields (e.g. the lock from R1, or a slot-allocation bitmap). The lack
of a paired destroy helper also means the close-on-detach loop is
implicit (today vmwgfx doesn't close outstanding opens in detach).

**Fix:** add

```c
void drm_illumos_file_state_init(struct drm_illumos_file_state *state);
void drm_illumos_file_state_destroy(struct drm_illumos_file_state *state);
```

Init takes the mutex from R1. Destroy walks all `in_use` slots and
releases them (calls `drm_release` on each). Call from vmwgfx attach
and detach respectively.

---

## R10. `drm_illumos.h` is not `extern "C"`-wrapped

**File:** `usr/src/uts/common/io/drm/include/drm/drm_illumos.h`

The header has no `#ifdef __cplusplus` / `extern "C"` block. Not a
correctness issue today (everything is C), but the illumos kernel has
a convention of wrapping public headers for C++ consumers. Minor
hygiene fix.

---

## Summary

| # | File | Severity | Blocks goal |
|---|---|---|---|
| R1 | drm_illumos_file.c | **High** (correctness race) | A, B |
| R2 | drm_illumos_file.c | Medium (latent) | A (render node), B |
| R3 | drm_illumos_irq.c / _file.c | Medium | — |
| R4 | drm_illumos_irq.c | **High** | B |
| R5 | drm_illumos_file.c | Low (defer) | B (eventually) |
| R6 | drm_illumos_pci.c | Low | — |
| R7 | drm_illumos_pci.c | Medium (robustness) | B |
| R8 | drm_illumos_pci.c | Medium | B |
| R9 | drm_illumos_file.c | Low (code quality) | — |
| R10 | drm_illumos.h | Trivial | — |

Recommended order: R1, R3, R4, R2, R7, R8, R9, R10, R6, R5.
