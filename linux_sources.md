# Linux Source Inventory

This file documents which parts of `usr/src/uts/common/io/drm` are still best treated as Linux-source imports.

## Method

- Mechanical match: a file is treated as a Linux source import if the same relative path exists under `linux-drm/`.
- Linux-derived but not mechanically matchable here: some headers are still Linux-origin, but `linux-drm/` does not carry the full kernel include tree, so they cannot be matched by simple path comparison.
- Anything else is treated as compatibility/build glue and is listed in `compatibility_layers.md`.

## Mechanically matched against `linux-drm`

There are 182 direct path matches.

### Top-level DRM core files

These 85 files exist both in `usr/src/uts/common/io/drm` and `linux-drm/`:

```text
drm_aperture.c
drm_atomic.c
drm_atomic_helper.c
drm_atomic_state_helper.c
drm_atomic_uapi.c
drm_auth.c
drm_blend.c
drm_bridge.c
drm_buddy.c
drm_cache.c
drm_client.c
drm_client_modeset.c
drm_client_setup.c
drm_color_mgmt.c
drm_connector.c
drm_crtc.c
drm_crtc_helper.c
drm_crtc_helper_internal.h
drm_crtc_internal.h
drm_damage_helper.c
drm_debugfs.c
drm_debugfs_crc.c
drm_displayid.c
drm_displayid_internal.h
drm_draw.c
drm_draw_internal.h
drm_drv.c
drm_dumb_buffers.c
drm_edid.c
drm_eld.c
drm_encoder.c
drm_encoder_slave.c
drm_exec.c
drm_fb_dma_helper.c
drm_fb_helper.c
drm_fbdev_client.c
drm_fbdev_dma.c
drm_fbdev_shmem.c
drm_fbdev_ttm.c
drm_file.c
drm_flip_work.c
drm_format_helper.c
drm_format_internal.h
drm_fourcc.c
drm_framebuffer.c
drm_gem.c
drm_gem_atomic_helper.c
drm_gem_dma_helper.c
drm_gem_framebuffer_helper.c
drm_gem_ttm_helper.c
drm_gpuvm.c
drm_internal.h
drm_ioc32.c
drm_ioctl.c
drm_kms_helper_common.c
drm_managed.c
drm_mipi_dsi.c
drm_mm.c
drm_mode_config.c
drm_mode_object.c
drm_modes.c
drm_modeset_helper.c
drm_modeset_lock.c
drm_panel.c
drm_panel_orientation_quirks.c
drm_panic.c
drm_pci.c
drm_plane.c
drm_plane_helper.c
drm_prime.c
drm_print.c
drm_privacy_screen.c
drm_privacy_screen_x86.c
drm_probe_helper.c
drm_property.c
drm_rect.c
drm_self_refresh_helper.c
drm_suballoc.c
drm_syncobj.c
drm_sysfs.c
drm_trace.h
drm_trace_points.c
drm_vblank.c
drm_vblank_work.c
drm_vma_manager.c
```

### `display/`

All 13 files are direct Linux path matches:

```text
display/drm_display_helper_mod.c
display/drm_dp_aux_dev.c
display/drm_dp_dual_mode_helper.c
display/drm_dp_helper.c
display/drm_dp_helper_internal.h
display/drm_dp_mst_topology.c
display/drm_dp_mst_topology_internal.h
display/drm_dp_tunnel.c
display/drm_dsc_helper.c
display/drm_hdcp_helper.c
display/drm_hdmi_helper.c
display/drm_hdmi_state_helper.c
display/drm_scdc_helper.c
```

### `scheduler/`

All 4 files are direct Linux path matches:

```text
scheduler/gpu_scheduler_trace.h
scheduler/sched_entity.c
scheduler/sched_fence.c
scheduler/sched_main.c
```

### `ttm/`

All 13 files are direct Linux path matches:

```text
ttm/ttm_agp_backend.c
ttm/ttm_bo.c
ttm/ttm_bo_util.c
ttm/ttm_bo_vm.c
ttm/ttm_device.c
ttm/ttm_execbuf_util.c
ttm/ttm_module.c
ttm/ttm_module.h
ttm/ttm_pool.c
ttm/ttm_range_manager.c
ttm/ttm_resource.c
ttm/ttm_sys_manager.c
ttm/ttm_tt.c
```

### `vmwgfx/`

Treat the whole imported driver subtree as Linux source except the local illumos add-ons:

- Linux-imported: all files under `vmwgfx/` that also exist in `linux-drm/vmwgfx/`, including `device_include/*`, `Kconfig`, `Makefile`, `ttm_object.*`, and all `vmwgfx_*` sources and headers.
- Local additions, not Linux imports: `vmwgfx/vmwgfx_illumos.c` and `vmwgfx/Makefile.mod`.

## Linux-derived headers not mechanically matchable in this repo layout

These are still Linux-origin interfaces, but `linux-drm/` does not include the full kernel header tree needed for a direct path match:

- `include/drm/**` (123 files): DRM/TTM/public internal headers, mostly from the kernel `include/drm/` tree.
- `include/uapi/**` (13 files): DRM and Linux UAPI headers.
- `include/video/**` (7 files), `include/media/**` (1 file), `include/acpi/**` (4 files), `include/sound/**` (1 file), `include/xen/**` (1 file): Linux/shared subsystem headers consumed by DRM.
- `vmwgfx/device_include/**` is already counted in the mechanical match set above because those headers exist under `linux-drm/vmwgfx/device_include/`.

These should still be treated as Linux-side API surface, not illumos compatibility code, unless a specific file is called out otherwise in `file_change_notes.md`.

## Sync status against `linux-drm`

- Direct matches with identical contents: 99
- Direct matches with local edits: 83

The edited Linux-path files are:

```text
display/drm_dp_aux_dev.c
display/drm_dp_dual_mode_helper.c
display/drm_dp_helper.c
display/drm_dp_helper_internal.h
display/drm_dp_mst_topology.c
display/drm_dp_mst_topology_internal.h
display/drm_dp_tunnel.c
display/drm_hdcp_helper.c
drm_aperture.c
drm_atomic.c
drm_atomic_helper.c
drm_atomic_uapi.c
drm_auth.c
drm_bridge.c
drm_buddy.c
drm_cache.c
drm_client_modeset.c
drm_connector.c
drm_debugfs.c
drm_debugfs_crc.c
drm_draw.c
drm_drv.c
drm_edid.c
drm_encoder_slave.c
drm_exec.c
drm_fb_dma_helper.c
drm_fb_helper.c
drm_fbdev_client.c
drm_fbdev_dma.c
drm_fbdev_shmem.c
drm_fbdev_ttm.c
drm_file.c
drm_format_helper.c
drm_framebuffer.c
drm_gem.c
drm_gem_atomic_helper.c
drm_gem_dma_helper.c
drm_gem_framebuffer_helper.c
drm_gem_ttm_helper.c
drm_gpuvm.c
drm_internal.h
drm_ioc32.c
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
drm_prime.c
drm_print.c
drm_privacy_screen.c
drm_privacy_screen_x86.c
drm_probe_helper.c
drm_property.c
drm_rect.c
drm_syncobj.c
drm_sysfs.c
drm_trace.h
drm_vblank.c
drm_vblank_work.c
scheduler/gpu_scheduler_trace.h
scheduler/sched_fence.c
scheduler/sched_main.c
ttm/ttm_agp_backend.c
ttm/ttm_bo_util.c
ttm/ttm_bo_vm.c
ttm/ttm_device.c
ttm/ttm_module.c
ttm/ttm_module.h
ttm/ttm_pool.c
ttm/ttm_resource.c
ttm/ttm_tt.c
vmwgfx/vmwgfx_drv.c
vmwgfx/vmwgfx_drv.h
vmwgfx/vmwgfx_execbuf.c
vmwgfx/vmwgfx_irq.c
vmwgfx/vmwgfx_page_dirty.c
vmwgfx/vmwgfx_ttm_buffer.c
```

Use `file_change_notes.md` for the keep-vs-move decision on each class of edit.
