#
# Copyright (c) 2024, illumos contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
#
###############################################################################
#
#  DRM_OBJS — all object files for misc/drm
#
###############################################################################

#
# Top-level DRM objects
#
DRM_CORE_OBJS = \
	dma-resv.o \
	drm_agpsupport.o \
	drm_aperture.o \
	drm_atomic.o \
	drm_atomic_helper.o \
	drm_atomic_state_helper.o \
	drm_atomic_uapi.o \
	drm_auth.o \
	drm_blend.o \
	drm_bridge.o \
	drm_buddy.o \
	drm_cache.o \
	drm_client.o \
	drm_client_modeset.o \
	drm_client_setup.o \
	drm_color_mgmt.o \
	drm_connector.o \
	drm_crtc.o \
	drm_crtc_helper.o \
	drm_damage_helper.o \
	drm_debugfs.o \
	drm_debugfs_crc.o \
	drm_displayid.o \
	drm_draw.o \
	drm_drv.o \
	drm_dumb_buffers.o \
	drm_edid.o \
	drm_eld.o \
	drm_encoder.o \
	drm_encoder_slave.o \
	drm_exec.o \
	drm_fbdev_client.o \
	drm_fbdev_dma.o \
	drm_fbdev_shmem.o \
	drm_fbdev_ttm.o \
	drm_fb_dma_helper.o \
	drm_fb_helper.o \
	drm_file.o \
	drm_flip_work.o \
	drm_format_helper.o \
	drm_fourcc.o \
	drm_framebuffer.o \
	drm_gem.o \
	drm_gem_atomic_helper.o \
	drm_gem_dma_helper.o \
	drm_gem_framebuffer_helper.o \
	drm_gem_ttm_helper.o \
	drm_gpuvm.o \
	drm_ioc32.o \
	drm_ioctl.o \
	drm_kms_helper_common.o \
	drm_linux.o \
	drm_illumos_mod.o \
	drm_illumos_file.o \
	drm_illumos_irq.o \
	drm_illumos_pci.o \
	drm_managed.o \
	drm_mipi_dsi.o \
	drm_mm.o \
	drm_mode_config.o \
	drm_mode_object.o \
	drm_modes.o \
	drm_modeset_helper.o \
	drm_modeset_lock.o \
	drm_mtrr.o \
	drm_panel.o \
	drm_panel_orientation_quirks.o \
	drm_panic.o \
	drm_pci.o \
	drm_plane.o \
	drm_plane_helper.o \
	drm_prime.o \
	drm_print.o \
	drm_privacy_screen.o \
	drm_privacy_screen_x86.o \
	drm_probe_helper.o \
	drm_property.o \
	drm_rect.o \
	drm_self_refresh_helper.o \
	drm_suballoc.o \
	drm_syncobj.o \
	drm_sysfs.o \
	drm_trace_points.o \
	drm_vblank.o \
	drm_vblank_work.o \
	drm_vma_manager.o \
	hdmi.o \
	linux_list_sort.o \
	linux_radix.o \
	linux_sort.o \
	linux_sort_r.o

#
# display/ subdirectory objects (prefixed with display_)
#
DRM_DISPLAY_OBJS = \
	display_drm_display_helper_mod.o \
	display_drm_dp_aux_dev.o \
	display_drm_dp_dual_mode_helper.o \
	display_drm_dp_helper.o \
	display_drm_dp_mst_topology.o \
	display_drm_dp_tunnel.o \
	display_drm_dsc_helper.o \
	display_drm_hdcp_helper.o \
	display_drm_hdmi_helper.o \
	display_drm_hdmi_state_helper.o \
	display_drm_scdc_helper.o

#
# ttm/ subdirectory objects (prefixed with ttm_)
#
DRM_TTM_OBJS = \
	ttm_ttm_agp_backend.o \
	ttm_ttm_bo.o \
	ttm_ttm_bo_util.o \
	ttm_ttm_bo_vm.o \
	ttm_ttm_device.o \
	ttm_ttm_execbuf_util.o \
	ttm_ttm_module.o \
	ttm_ttm_pool.o \
	ttm_ttm_range_manager.o \
	ttm_ttm_resource.o \
	ttm_ttm_sys_manager.o \
	ttm_ttm_tt.o

#
# scheduler/ subdirectory objects (prefixed with sched_)
#
DRM_SCHED_OBJS = \
	sched_sched_entity.o \
	sched_sched_fence.o \
	sched_sched_main.o

DRM_OBJS = \
	$(DRM_CORE_OBJS) \
	$(DRM_DISPLAY_OBJS) \
	$(DRM_TTM_OBJS) \
	$(DRM_SCHED_OBJS)
