/* Public domain. */

#ifndef _DRM_GEM_TTM_HELPER_H_
#define _DRM_GEM_TTM_HELPER_H_

#include <drm/drm_gem.h>
#include <drm/ttm/ttm_bo.h>
#include <linux/mm_types.h>

struct iosys_map;
struct drm_device;
struct drm_printer;

static inline struct ttm_buffer_object *
drm_gem_ttm_of_gem(struct drm_gem_object *gem_obj)
{
	return container_of(gem_obj, struct ttm_buffer_object, base);
}

int drm_gem_ttm_mmap(struct drm_gem_object *gem_obj,
		     struct vm_area_struct *vma);
int drm_gem_ttm_vmap(struct drm_gem_object *, struct iosys_map *);
void drm_gem_ttm_vunmap(struct drm_gem_object *, struct iosys_map *);
int drm_gem_ttm_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
    uint32_t handle, uint64_t *offset);

static inline void
drm_gem_ttm_print_info(struct drm_printer *p, unsigned int indent,
		       const struct drm_gem_object *gem_obj)
{
	/* Phase 1 stub */
}

#endif
