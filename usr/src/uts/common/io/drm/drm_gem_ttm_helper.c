/* Public domain. */

#include <linux/kernel.h>
#include <linux/iosys-map.h>
#include <linux/mm_types.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_ttm_helper.h>
#include <drm/ttm/ttm_bo.h>
#include <drm/drm_file.h>

int
drm_gem_ttm_mmap(struct drm_gem_object *gem_obj, struct vm_area_struct *vma)
{
	struct ttm_buffer_object *bo = drm_gem_ttm_of_gem(gem_obj);
	int ret;

	ret = ttm_bo_mmap_obj(vma, bo);
	if (ret != 0) {
		drm_gem_object_put(gem_obj);
		return ret;
	}

	return 0;
}

int
drm_gem_ttm_vmap(struct drm_gem_object *obj, struct iosys_map *ism)
{
	struct ttm_buffer_object *tbo =
	    container_of(obj, struct ttm_buffer_object, base);

	return ttm_bo_vmap(tbo, ism);
}

void
drm_gem_ttm_vunmap(struct drm_gem_object *obj, struct iosys_map *ism)
{
	struct ttm_buffer_object *tbo =
	    container_of(obj, struct ttm_buffer_object, base);

	ttm_bo_vunmap(tbo, ism);
}

/*
 * drm_gem_ttm_dumb_map_offset - mmap offset for dumb buffers
 *
 * Registers the GEM object in the VMA offset manager and returns the
 * offset to pass to mmap(). Delegates to drm_gem_dumb_map_offset which
 * calls drm_gem_create_mmap_offset internally.
 */
int
drm_gem_ttm_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
    uint32_t handle, uint64_t *offset)
{
	return drm_gem_dumb_map_offset(file, dev, handle, offset);
}
