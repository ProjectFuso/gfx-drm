/* Public domain. */
/*
 * illumos: stub for drm_gem_shmem_helper.h
 * The upstream Linux shmem GEM helper uses anonymous shared memory via
 * shmem_read_mapping_page(). On illumos this is not available; provide
 * a minimal struct so drm_fbdev_shmem.c compiles.
 */

#ifndef _DRM_GEM_SHMEM_HELPER_H
#define _DRM_GEM_SHMEM_HELPER_H

#include <linux/mutex.h>
#include <linux/types.h>
#include <drm/drm_gem.h>
#include <linux/iosys-map.h>

struct drm_gem_shmem_object {
	struct drm_gem_object	base;

	struct mutex		pages_lock;
	struct page		**pages;
	unsigned int		pages_use_count;
	int			pages_mark_dirty_on_put;
	int			pages_mark_accessed_on_put;
	struct sg_table		*sgt;

	bool			map_wc;

	struct iosys_map	vmap;
	u32			vmap_use_count;
};

#define to_drm_gem_shmem_obj(obj) \
	container_of(obj, struct drm_gem_shmem_object, base)

struct drm_gem_shmem_object *drm_gem_shmem_create(struct drm_device *dev, size_t size);
void drm_gem_shmem_free(struct drm_gem_shmem_object *shmem);

int drm_gem_shmem_vmap(struct drm_gem_object *obj, struct iosys_map *map);
void drm_gem_shmem_vunmap(struct drm_gem_object *obj, struct iosys_map *map);

int drm_gem_shmem_dumb_create(struct drm_file *file, struct drm_device *dev,
			      struct drm_mode_create_dumb *args);

#define DRM_GEM_SHMEM_DRIVER_OPS \
	.gem_prime_import_sg_table	= NULL,	\
	.gem_prime_mmap			= NULL

#endif /* _DRM_GEM_SHMEM_HELPER_H */
