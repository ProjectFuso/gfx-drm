# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

A port of the Linux DRM (Direct Rendering Manager) kernel subsystem and related userland libraries to illumos. The kernel code is built using the illumos-gate ON skeleton build system; the userland libraries are downloaded from freedesktop.org at build time and patched.

Currently active driver: **vmwgfx** (VMware virtual GPU). The i915 driver is a remain from old gfx-drm stack and not currently being worked on.

## Goals

1. Port DRM stack from Linux to illumos (currently targeting linux 6.12.74)
2. Keep the changes to the original linux source to absolutely minimal; only modify source codes when necessary. This is critical for easier syncing in the future.

## Remote VM build workflow

Compilation is done on an illumos VM (the local host is a Linux development machine). See `BUILD_WORKFLOW.md` for the full SSH-based workflow. The VM is the authoritative compiler for illumos integration issues.

## Code architecture

### Source categories

The DRM source in `usr/src/uts/common/io/drm/` falls into three categories tracked by the docs in the repo root:

1. **Linux source imports** (`linux_sources.md`): 182 files with the same relative path in `linux-drm/`. These should stay close to upstream; local edits to them are tracked in `file_change_notes.md`.

2. **Compatibility/portability glue** (`compatibility_layers.md`): Files whose primary purpose is emulating Linux kernel APIs on illumos. Key ones:
   - `drm_linux.c` — Linux KPI emulation runtime
   - `drm_illumos_mod.c` — illumos module lifecycle wrapper
   - `include/linux/` (273 headers) — Linux KPI shims (memory, DMA, locks, workqueues, timers, etc.)
   - `include/asm/` (21 headers) — arch header façade
   - `include/generated/` — autoconf-style glue

3. **illumos replacement units**: Files that stub or fully replace Linux facilities that don't exist on illumos (sysfs, debugfs, PRIME, VFS/file ops). These are marked "move to illumos replacement unit" in `file_change_notes.md` and are candidates for splitting into `*_illumos.c` files.

### Linux source reference tree

`linux-drm/` holds the upstream Linux DRM source used as a sync reference. It is not compiled — it exists only for diffing and future upstream syncs.

### Userland libraries

Userland libraries are from the old gfx-drm repository and haven't been updated yet. It is not the priority right now.

## Refereneces

Under references there are three directories for reference:
- `linux-drm`: Authentic linux DRM source code
- `linux-drm-header`: Authentic linux DRM headers
- `openbsd-drm`: The OpenBSD port of linux DRM (targeting linux-6.12.74); use for reference on how to implement compatibility layers

# illumos Headers
On the Linux development machine, system headers are NOT illumos headers. To inspect illumos headers, look in:
- `../core/usr/src/head`
- `../core/usr/src/uts/common`

## Developing Instructions

- Adhere to **linux kernel style**.
- Commit frequently in small increments; Don't wait until next test build before commiting. Frequent commit in small increments make it easier to work with others, and make it easier to bisect during debugging.
