# OpenBSD DRM Analysis — Reference for illumos Port

Analysis of `references/openbsd-drm/` (targeting Linux ~6.12) conducted
2026-04-06. OpenBSD ships four working drivers: amdgpu, i915, radeon,
Apple.

## Overall Architecture

OpenBSD's porting strategy: **bulk-import Linux source, patch minimally
with `#ifdef __OpenBSD__` / `#ifdef __linux__`, provide a single
`drm_linux.c` (3,982 lines) as the Linux KPI emulation runtime, plus
~273 shim headers under `include/linux/`.**

This is structurally identical to our approach (our `drm_linux.c` is
3,303 lines). The differences are in how the two OSes provide kernel
primitives.

## Key Implementation Details

### 1. `struct page` = real VM page (not a shim)

OpenBSD's `struct page` IS `struct vm_page` (UVM). This gives them
real, zero-cost `page_to_pfn` / `pfn_to_page`:

```c
/* include/linux/mm.h */
#define page_to_pfn(pp)    (VM_PAGE_TO_PHYS(pp) / PAGE_SIZE)
#define pfn_to_page(pfn)   (PHYS_TO_VM_PAGE(ptoa(pfn)))
```

Also: `dma_map_page()` → `VM_PAGE_TO_PHYS(page)` (one macro).

**Why illumos can't use the same approach (native page_t as struct page):**

Unlike OpenBSD's `struct vm_page`, illumos `page_t` is `typedef struct page`
— the same tag name as Linux. This creates an unavoidable include-file
conflict (you can't have both definitions in the same TU). Beyond the
name collision:

- **Field mismatch:** Linux DRM accesses `page->lru` (list_head),
  `page->kaddr`, `page->_refcount`. illumos page_t has none of these;
  its list linkage (`p_next/p_prev`) is owned by the VM subsystem.
- **Ownership model:** illumos page_t lives in the global page hash,
  is subject to the page scanner, and requires `page_lock/page_unlock`.
  Linux DRM pages are simple "I own this memory" descriptors.
- **Allocation model:** our code uses `kmem_alloc`/`ddi_umem_alloc`.
  Using native page_t would require `page_create_va()` with
  vnode+offset — a complete rework.

**Chosen approach for illumos:** keep the `struct page` shim, add a
`_pfn` field, store `hat_getpfnum()` result at allocation time, and
maintain a global PFN→page hash table for `pfn_to_page()` reverse
lookups. See Goal A / A10 for implementation details.

### 2. dma-buf — fd-backed, ~80 lines

`drm_linux.c:2578-2668`. Uses OpenBSD's file descriptor table directly:

- `dma_buf_export()` → `fnew(p)` (allocate kernel file), set
  `f_type = DTYPE_DMABUF`, custom `dmabufops` fileops, store dmabuf in
  `f_data`.
- `dma_buf_get(fd)` → `fd_getfile(fdp, fd)` + type-check
  `DTYPE_DMABUF`, return `fp->f_data`.
- `dma_buf_fd()` → `fdalloc()` + `fdinsert()` to install in process
  fd table.
- `dma_buf_put()` → `FRELE(dmabuf->file, curproc)`.
- `get_dma_buf()` → `FREF(dmabuf->file)`.

**NOT implemented:** `dma_buf_attach()` returns NULL,
`dma_buf_detach()` calls `panic()`. Cross-device import is not
supported. `dma_buf_is_dynamic()` returns false.

**illumos equivalent:** our `falloc()`/`setf()`/`getf()` + vnode
approach is the right analogue. OpenBSD's `DTYPE_DMABUF` maps to our
vnode-backed file with custom vnodeops.

### 3. sync_file — same fd pattern, ~40 lines

`drm_linux.c:3167-3268`. Identical pattern to dma-buf:

- `sync_file_create(fence)` → `fnew(p)`, `f_type = DTYPE_SYNC`,
  `syncfileops`, wraps a single `dma_fence *`.
- `sync_file_get_fence(fd)` → `fd_getfile()` + type-check
  `DTYPE_SYNC`, return `dma_fence_get(sf->fence)`.
- Close → `dma_fence_put(sf->fence)`.

All other syncfile fops (read, write, ioctl, kqfilter) return error
stubs. **Minimal but sufficient for Mesa/Vulkan explicit sync.**

### 4. Firmware loading — trivial wrapper

`include/linux/firmware.h` (~40 lines):

```c
request_firmware() → loadfirmware(name, &data, &size)
release_firmware() → free(data) + free(fw)
request_firmware_nowait() → -EINVAL  (async not supported)
```

illumos equivalent: `vn_open()` + `vn_rdwr()` from
`/usr/lib/firmware/` or `/kernel/firmware/`. Same complexity.

### 5. I2C — full bit-bang bridge, ~120 lines

`drm_linux.c:1182-1295`:

- `i2c_master_xfer()` translates Linux `i2c_msg` (addr, flags, buf,
  len) to OpenBSD's `iic_exec(tag, op, addr, cmd, cmdlen, buf, len)`.
- `i2c_bb_master_xfer()` wraps bit-bang adapters — takes
  `i2c_algo_bit_data`, creates a temporary adapter pointing to the
  bit-bang controller, delegates to `i2c_master_xfer`.
- `i2c_bit_algo` struct wires `.master_xfer = i2c_bb_master_xfer`.
- `i2c_bit_add_bus()` sets `adap->algo = &i2c_bit_algo`.

illumos has `i2c_transfer()` in `sys/i2c/i2c.h` — similar translation
target.

### 6. File open/close — SPLAY tree on minor number

`drm_drv.c:1739-1848`. Uses a SPLAY tree (balanced BST) keyed on
device minor, not a fixed slot array:

- `drmopen()`: `minor(kdev)` decoded as 0-63 → PRIMARY, 128-191 →
  RENDER. Calls `drm_file_alloc(dm)`, `SPLAY_INSERT()` into
  `dev->files`. Master negotiation on first primary open.
- `drmclose()`: `drm_find_file_by_minor()` + `SPLAY_REMOVE()` +
  `drm_file_free()`.
- `drmread()`: `msleep_nsec()` on `file_priv->event_wait`, dequeue
  events, `uiomove()` to userland.
- `drmkqfilter()`: kqueue filter for POLLIN on event list.

Locking: `filelist_mutex` held across insert/remove. No races.

**Render nodes are trivial:** just a minor-number range check. No
separate device node registration needed beyond the char dev.

### 7. Fault handling — UVM pmap, not devmap

`ttm/ttm_bo_vm.c:352-623`. Complete `#else /* !__linux__ */` block:

- `ttm_bo_vm_fault_reserved()` receives `struct uvm_faultinfo *ufi`,
  walks TTM BO pages, calls `pmap_enter()` directly for each page.
- I/O memory (VRAM): `bus_space_mmap()` for device-mapped addresses.
- Return value conversion: `VM_FAULT_NOPAGE` → 0,
  `VM_FAULT_RETRY` → `ERESTART`.
- Locking: `uvmfault_unlockall()` instead of `mmap_read_unlock()`.

This is demand-paging: pages are populated on fault, not up-front.
Our devmap approach (`devmap_umem_setup` + `devmap_devmem_setup`)
pre-populates, which works for vmwgfx but won't scale for amdgpu.
Consider adding a `devmap_access()` callback for demand-paging later.

### 8. IRQ — stubbed generically, wired per-driver

`include/linux/interrupt.h`:
```c
#define request_irq(irq, hdlr, flags, name, dev)  (0)
```

Each driver wires IRQs through OpenBSD's PCI interrupt framework
directly in the driver's attach function, bypassing the Linux `request_irq`
API entirely. **There is no shared IRQ bridge like our
`drm_illumos_irq.c`.**

Tasklets are mapped to OpenBSD's `struct task` + `taskq` system.

### 9. Workqueue / Threading

- `struct work_struct` wraps OpenBSD `struct task`.
- `queue_work()` → `task_add(taskq, &work->task)`.
- `struct delayed_work` adds `struct timeout` for delayed execution.
- `alloc_workqueue()` → `taskq_create()`.
- kthreads via OpenBSD `kthread_create()`.

### 10. Memory allocation

`drm_linux.c:565-721`:
- `alloc_pages()` → `uvm_pglistalloc()` with DMA constraints.
- `kmap()` → `pmap_map_direct()` or `km_alloc()` + `pmap_kenter_pa()`.
- `vmap()` → `km_alloc()` + `pmap_enter()` per page.

### 11. PCI

`include/linux/pci.h` maps Linux `struct pci_dev` to OpenBSD
`pci_chipset_tag_t` + `pcitag_t`. Config space access via native PCI
ops. Bus master / memory decode enable handled by the driver attach
code, not a shared helper.

## amdgpu-Specific Findings

### Scale

- 2,452 files total under `amd/`
- `amd/amdgpu/`: 569 files (core driver)
- `amd/display/`: 1,047 files (DC — complete, all DCE/DCN generations)
- `amd/amdkfd/`: 64 files (compute — fully included, not stubbed)
- `amd/pm/`: 259 files (power management)
- `amd/include/`: 515 files (register headers)

### Modification sites

Only **96 conditional compilation sites** across the entire amd/ tree:
- 91 `#ifdef __linux__` (Linux-specific code excluded)
- 5 `#ifdef __OpenBSD__` (OpenBSD-specific additions)

Affected files (~30 in amdgpu/):
- Core: `amdgpu_device.c`, `amdgpu_drv.c`, `amdgpu_acpi.c`
- Memory: `amdgpu_vm.c`, `amdgpu_gart.c`, `amdgpu_object.c`,
  `amdgpu_ttm.c`
- IRQ: `amdgpu_irq.c`, `amdgpu_ih.c`, `amdgpu_fence.c`
- PSP: `amdgpu_psp.c`
- Display: `amdgpu_dm.c`

### What OpenBSD patches in amdgpu

- `amdgpu_drv.c`: `.mmap = drm_gem_mmap` under `#ifdef __OpenBSD__`,
  Linux `.fops` excluded
- `amdgpu_device.c`: chip name reporting adjustments, memory
  reporting via `ptoa(physmem)` instead of Linux `si_meminfo`
- `amdgpu_acpi.c`: skip S3 reset for VEGA10
- `amdgpu_psp.c`: PSP firmware handling differences
- `amdgpu_dm.c`: disable PSR on specific ThinkPad models
- `display/dc/basics/amdgpu_vector.c`: `krealloc` replacement

### What OpenBSD does NOT implement for amdgpu

- `mmu_interval_notifier` / userptr — gated with `#ifdef __linux__`
- Runtime PM — stubs
- `request_firmware_nowait` — returns -EINVAL
- `eventfd` — not implemented
- Cross-device dma-buf import — attach returns NULL

## Summary: What OpenBSD Teaches Us

| Lesson | Detail |
|--------|--------|
| page_to_pfn is foundational | Must be real, not stubbed. Solve early. |
| dma-buf / sync_file are small | ~80 + ~40 lines of fd-table ops each |
| Firmware loading is trivial | ~40 line wrapper around OS file read |
| I2C is ~120 lines | Translate Linux i2c_msg to OS i2c ops |
| IRQ per-driver, not shared bridge | Avoids complex generic abstraction |
| amdgpu is a bulk import + ~30 file patches | Not a rewrite |
| Render nodes = minor number range | No separate device registration needed |
| SPLAY tree > fixed slot array | For file tracking |
| Demand-paging via fault handler | Not pre-populate; needed for amdgpu |
