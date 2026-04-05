# Repository Guidelines

## Project Structure & Module Organization
Top-level builds are driven by [`Makefile`](/home/hokuto/workspace/project_fuso/gfx-drm-refactor/Makefile) and [`myenv.sh`](/home/hokuto/workspace/project_fuso/gfx-drm-refactor/myenv.sh). Core source lives under `usr/src/`: kernel DRM code in `usr/src/uts/common/io/drm/`, platform drivers in `usr/src/uts/intel/`, user libraries in `usr/src/lib/libdrm*` and `usr/src/lib/libkms`, packaging in `usr/src/pkg/`, and test programs in `usr/src/cmd/drm-tests/`. Workflow notes belong in repo docs such as `README` and `BUILD_WORKFLOW.md`.

## Build, Test, and Development Commands
Use an illumos/OpenIndiana build host with `/opt/onbld` available.

- `make install` builds the workspace, including debug `uts` first, then installs into `proto/`.
- `make debug` runs the debug-oriented install path only.
- `make package` assembles packages under `packages/${MACH}/nightly/`.
- `make clean` runs the workspace `clobber` target.
- `/usr/bin/ksh93 tools/bldenv -d myenv.sh` opens a shell for incremental driver work.
- `usr/src/common/libdrm/Check-patches` verifies local libdrm edits still match the patch set.

For VM validation, follow `BUILD_WORKFLOW.md`; the current documented path builds `drm` and `vmwgfx` remotely with `tools/bldenv`.

## Coding Style & Naming Conventions
Match existing illumos kernel style: tabs for indentation, K&R braces, aligned wrapped arguments, and descriptive `drm_*`, `vmwgfx_*`, or subsystem-prefixed names. Preserve existing license headers and avoid mixing unrelated refactors into driver changes. The tree exposes ON tooling via `usr/src/Makefile.master`, including `cstyle` and `hdrchk`; use them when touching headers or large C changes.

## Testing Guidelines
Run at least the narrowest relevant build before submitting. For kernel changes, build the affected subtree from a `bldenv` shell, for example `cd usr/src/uts/intel/drm && make install`. User test binaries install under `/opt/drm-test/*`; use `usr/src/cmd/drm-tests/Run_all.sh` or the relevant test family when hardware or VM coverage is available. Re-run `Check-patches` after updating imported libdrm sources.

## Commit & Pull Request Guidelines
Recent history uses short, imperative subjects with an area prefix, for example `drm: share illumos irq bridge` or `docs: record remote vm build workflow`. Keep commits scoped to one subsystem. Pull requests should state the affected module path, build/test commands run, target hardware or VM used, and any packaging impact. Include logs or screenshots only when they clarify driver behavior or test failures.
