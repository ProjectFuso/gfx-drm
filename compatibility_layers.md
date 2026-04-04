# Compatibility Layer Inventory

This file documents the parts of `usr/src/uts/common/io/drm` that are acting as portability glue rather than Linux source imports.

## Decision rule

A file is treated as compatibility code if at least one of these is true:

- there is no same-path file in `linux-drm/`;
- its primary purpose is to emulate Linux kernel APIs on illumos;
- it is build glue, generated glue, or an illumos-only replacement source.

## illumos/OpenBSD glue sources

These are compatibility/build files, not Linux source imports:

```text
LICENSE_DRM
LICENSE_DRM.descrip
Makefile.mod
dma-resv.c
drm_agpsupport.c
drm_dp_helper.c
drm_illumos_mod.c
drm_linux.c
drm_mtrr.c
hdmi.c
linux_list_sort.c
linux_radix.c
linux_sort.c
linux_sort_r.c
vmwgfx/Makefile.mod
vmwgfx/vmwgfx_illumos.c
```

Notes:

- `drm_linux.c` is the main Linux KPI emulation runtime.
- `drm_illumos_mod.c` is the illumos module lifecycle wrapper.
- `drm_dp_helper.c` in the root is not the modern Linux DP helper source. The actual Linux import is `display/drm_dp_helper.c`.
- `vmwgfx/vmwgfx_illumos.c` is the VMware-specific illumos backend.

## Compatibility header space

These directories are primarily portability scaffolding:

- `include/linux/**` (273 files): Linux KPI shims and wrappers for memory management, devices, DMA, files, locks, wait queues, timers, workqueues, debugfs, etc.
- `include/asm/**` (21 files): Linux arch header façade used by DRM and vmwgfx.
- `include/sys/**` (9 files), `include/machine/**` (2 files), `include/uvm/**` (2 files), `include/dev/**` (3 files): imported or shimmed BSD/illumos-side compatibility headers needed by the port.
- `include/generated/**` (5 files): generated/autoconf-style glue used to satisfy Linux build assumptions.
- `include/trace/**` (2 files): trace plumbing needed by imported code.

## Linux-derived API headers that are not compatibility implementations

These live in the local include tree, but should not be treated as compatibility glue just because they are under `include/`:

- `include/drm/**` (123 files)
- `include/uapi/**` (13 files)
- `include/video/**` (7 files)
- `include/media/**` (1 file)
- `include/acpi/**` (4 files)
- `include/sound/**` (1 file)
- `include/xen/**` (1 file)

Those are better treated as Linux-side API/header imports, even when locally edited.

## Practical split to use during future sync work

- Sync target first: direct-match files listed in `linux_sources.md`.
- Sync target second: Linux-derived public headers under `include/drm/**` and `include/uapi/**`.
- Porting layer: everything in the glue-source list above plus the compatibility header directories.

That split keeps the future diff review focused on true upstream sync points rather than on illumos KPI emulation.
