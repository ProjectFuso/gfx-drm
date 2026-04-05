# GEMINI.md - gfx-drm-refactor

This project is a refactor and maintenance gate for the Graphics Direct Rendering Manager (DRM) on illumos/OpenIndiana. It provides kernel-level DRM infrastructure and user-level libraries (`libdrm`) to support hardware-accelerated graphics (Intel i915, VMware vmwgfx, etc.).

## What this repo is

A port of the Linux DRM (Direct Rendering Manager) kernel subsystem and related userland libraries to illumos. The kernel code is built using the illumos-gate ON skeleton build system; the userland libraries are downloaded from freedesktop.org at build time and patched.

Currently active driver: **vmwgfx** (VMware virtual GPU). The i915 driver is a remain from old gfx-drm stack and not currently being worked on.

## Goals

1. Port DRM stack from Linux to illumos (currently targeting linux 6.12.74)
2. Keep the changes to the original linux source to absolutely minimal; only modify source codes when necessary. This is critical for easier syncing in the future.

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
- **Development Practice** Commit frequently in small increments; Don't wait until next test build before commiting. Frequent commit in small increments make it easier to work with others, and make it easier to bisect during debugging.

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
