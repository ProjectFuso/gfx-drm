# File Change Notes

This file records what to do with the 83 edited Linux-path files from `linux_sources.md`.

## Decision buckets

- `keep direct`: keep the local source edit for now.
- `move to compat/header`: the result should be achievable by improving the compatibility layer or portability macros instead of patching the imported Linux file.
- `move to illumos replacement unit`: the Linux file is currently stubbed or heavily rewritten; better to stop patching the import and build an illumos-only source/header instead.

## Keep direct

These are the edits that currently look worth keeping as direct source deltas.

| File | Why keep it direct for now |
| --- | --- |
| `drm_auth.c` | illumos has no `logind`-style master handoff. The forced drop-and-retake path is an OS policy choice in the DRM master path, not just a missing helper. |
| `drm_drv.c` | non-Linux device bring-up is centralized here: minimal `drm_device` init, anon-inode stub setup, and reduced minor registration/sysfs handling. |
| `vmwgfx/vmwgfx_drv.c` | two active runtime workarounds live here: forcing coherent DMA mode on illumos and pushing an initial SVGA modeset because there is no fbdev bring-up path yet. |
| `vmwgfx/vmwgfx_execbuf.c` | the `PTR_ERR(vmw_bo)` return fix is a real correctness fix, not a portability shim. |
| `vmwgfx/vmwgfx_ttm_buffer.c` | the `mem->bus.addr` VRAM KVA population is a targeted runtime workaround for illumos VRAM access until the TTM I/O mapping path is taught to do the right thing generically. |

## Move to illumos replacement unit

These files are no longer good sync points in their current form. They should become illumos-only replacements selected by the build, leaving the Linux import unmodified.

```text
display/drm_dp_aux_dev.c
display/drm_dp_helper_internal.h
display/drm_dp_mst_topology_internal.h
display/drm_dp_tunnel.c
display/drm_hdcp_helper.c
drm_fb_dma_helper.c
drm_file.c
drm_gem.c
drm_gem_atomic_helper.c
drm_gem_dma_helper.c
drm_gem_framebuffer_helper.c
drm_gem_ttm_helper.c
drm_ioc32.c
drm_prime.c
drm_privacy_screen.c
drm_privacy_screen_x86.c
drm_sysfs.c
drm_trace.h
scheduler/gpu_scheduler_trace.h
ttm/ttm_bo_vm.c
ttm/ttm_module.h
ttm/ttm_pool.c
```

Reasoning:

- Most of these are either full stubs, mostly-`#ifdef __sun` replacements, or Linux facilities that illumos does not have at all yet: sysfs, debug/trace plumbing, Linux VFS/file ops, PRIME mmap/import, GEM DMA helpers, HDCP helper, and VM fault plumbing.
- Carrying those changes inside the imported Linux files will make future syncs noisy for little value.
- A build-time split such as `*_illumos.c` / `*_illumos.h` is the cleaner end state.

## Move to compat/header

Everything below looks better handled by better KPI wrappers, helper functions, or minor portability macros instead of by patching the imported Linux file.

```text
display/drm_dp_dual_mode_helper.c
display/drm_dp_helper.c
display/drm_dp_mst_topology.c
drm_aperture.c
drm_atomic.c
drm_atomic_helper.c
drm_atomic_uapi.c
drm_bridge.c
drm_buddy.c
drm_cache.c
drm_client_modeset.c
drm_connector.c
drm_debugfs.c
drm_debugfs_crc.c
drm_draw.c
drm_edid.c
drm_encoder_slave.c
drm_exec.c
drm_fb_helper.c
drm_fbdev_client.c
drm_fbdev_dma.c
drm_fbdev_shmem.c
drm_fbdev_ttm.c
drm_format_helper.c
drm_framebuffer.c
drm_gpuvm.c
drm_internal.h
drm_ioctl.c
drm_mipi_dsi.c
drm_mm.c
drm_mode_config.c
drm_mode_object.c
drm_modes.c
drm_modeset_lock.c
drm_panel.c
drm_panel_orientation_quirks.c
drm_panic.c
drm_pci.c
drm_print.c
drm_probe_helper.c
drm_property.c
drm_rect.c
drm_syncobj.c
drm_vblank.c
drm_vblank_work.c
scheduler/sched_fence.c
scheduler/sched_main.c
ttm/ttm_agp_backend.c
ttm/ttm_bo_util.c
ttm/ttm_device.c
ttm/ttm_module.c
ttm/ttm_resource.c
ttm/ttm_tt.c
vmwgfx/vmwgfx_drv.h
vmwgfx/vmwgfx_irq.c
vmwgfx/vmwgfx_page_dirty.c
```

Typical examples from this bucket:

- timer, waitqueue, tasklet, poll, and `seq_file` differences should be absorbed by compat helpers instead of open-coding `__sun` branches in many core files;
- `GFP_KERNEL_ACCOUNT`, `LIST_HEAD`, `msleep`, cache management, and simple `kref` callback mismatches belong in headers/macros;
- scheduler and trace changes are mostly “Linux feature absent here” plumbing and should be isolated from the imported source;
- vmwgfx IRQ differences already point at `vmwgfx_illumos.c`; the remaining source edits should shrink to thin wrappers once the compat side is a bit richer.

## Net result

- `keep direct`: 5 files
- `move to illumos replacement unit`: 22 files
- `move to compat/header`: 56 files

That accounts for all 83 edited Linux-path files.
