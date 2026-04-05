# GEMINI.md - gfx-drm-refactor

This project is a refactor and maintenance gate for the Graphics Direct Rendering Manager (DRM) on illumos/OpenIndiana. It provides kernel-level DRM infrastructure and user-level libraries (`libdrm`) to support hardware-accelerated graphics (Intel i915, VMware vmwgfx, etc.).

## Project Overview

- **Hybrid Architecture:** Combines OS-specific kernel modules with external user-level libraries.
- **Linux KPI Emulation:** Relies on a compatibility layer to run Linux-derived DRM drivers on the illumos kernel.
- **Upstream Sync:** Actively tracks upstream Linux DRM and `libdrm` sources, applying local patches where necessary.

## Key Technologies

- **Languages:** C (Kernel & Userland), ksh93 (Tools/Build scripts).
- **Build System:** illumos-style Makefiles (based on `on-skel`).
- **Platform:** illumos / OpenIndiana (x86/amd64).
- **Upstream Sources:** Linux Kernel DRM, `dri.freedesktop.org/libdrm`.

## Directory Structure

- `usr/src/uts/common/io/drm/`: DRM core kernel code (mostly imported from Linux).
- `usr/src/uts/intel/io/`: Platform drivers like `i915`, `vmwgfx`, and `agpgart`.
- `usr/src/lib/libdrm*`: User-level DRM libraries.
- `usr/src/common/libdrm/`: Infrastructure for downloading and patching external `libdrm` sources.
- `usr/src/cmd/drm-tests/`: DRM test suite and utilities.
- `usr/src/pkg/`: IPS packaging manifests.
- `tools/`: Build tools including `bldenv` for environment setup.

## Building and Running

Refer to `BUILD_WORKFLOW.md` for building instructions

## Refereneces

Under references there are three directories for reference:
- `linux-drm`: Authentic linux DRM source code
- `linux-drm-header`: Authentic linux DRM headers
- `openbsd-drm`: The OpenBSD port of linux DRM (targeting linux-6.12.74); use for reference on how to implement compatibility layers

## Development Conventions

- **Coding Style:** Adhere to **linux kernel style**.
- **Patch Management:**
    - Use `usr/src/common/libdrm/Check-patches` to verify local edits to imported libraries.
    - Follow `linux_sources.md` and `compatibility_layers.md` to distinguish between imported Linux code and local glue.
- **Commit Style:** Use short, imperative subjects with area prefixes (e.g., `drm: ...`, `vmwgfx: ...`, `docs: ...`).

## Testing

- Test binaries are installed to `/opt/drm-test/*`.
- Use `usr/src/cmd/drm-tests/Run_all.sh` to execute the full test suite.
- For remote VM testing (vmwgfx), refer to `BUILD_WORKFLOW.md`.

## Important Documentation
- `README`: General project introduction and layout.
- `BUILD_WORKFLOW.md`: Specific instructions for remote VM builds and validation.
- `AGENTS.md`: Detailed repository guidelines for AI agents and developers.
- `linux_sources.md` & `compatibility_layers.md`: Inventory of imported vs. local code.
- `file_change_notes.md`: Decisions on how to handle edited Linux files.

## illumos Headers
On the Linux development machine, system headers are NOT illumos headers. To inspect illumos headers, look in:
- `../core/usr/src/head`
- `../core/usr/src/uts/common`
