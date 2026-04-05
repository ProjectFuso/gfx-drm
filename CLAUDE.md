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

## Build commands

All builds run inside a `bldenv` shell that sets up the ON build environment. The top-level `Makefile` is a convenience wrapper:

```sh
# Full build (debug + non-debug, then package)
make install
make package

# Debug-only build
make debug

# Clean
make clean
```

For incremental module builds (the typical inner loop), enter `bldenv` and build the specific module:

```sh
# Interactive bldenv shell for incremental work
ksh93 tools/bldenv -d myenv.sh

# One-shot module builds (from repo root)
ksh93 tools/bldenv -d myenv.sh "cd usr/src/uts/intel/drm && make install"
ksh93 tools/bldenv -d myenv.sh "cd usr/src/uts/intel/vmwgfx && make install"

# Build both modules in one command
ksh93 tools/bldenv -d myenv.sh "cd usr/src/uts/intel/drm && make install && cd ../vmwgfx && make install"
```

Build output lands in `proto/root_i386/kernel/drv/amd64/{drm,vmwgfx}`.

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

### Kernel module dependency graph

```
vmwgfx  →  drm  →  agpmaster, gfx_private
```

### Linux source reference tree

`linux-drm/` holds the upstream Linux DRM source used as a sync reference. It is not compiled — it exists only for diffing and future upstream syncs.

### Refactoring guidance (`file_change_notes.md`)

For the 83 edited Linux-path files, there are three intended outcomes:
- **Keep direct** (5 files): illumos-specific policy or runtime workarounds that belong in the source itself (e.g., `vmwgfx/vmwgfx_drv.c`, `drm_drv.c`).
- **Move to illumos replacement unit** (22 files): heavily stubbed files; should become `*_illumos.c` replacements selected at build time.
- **Move to compat/header** (56 files): edits that should become improved KPI wrappers in `include/linux/`, not patches to imported files.

### Userland libraries

Userland libraries are from the old gfx-drm repository and haven't been updated yet. It is not the priority right now.
