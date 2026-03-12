/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __DRM_GEM_DMA_HELPER_H__
#define __DRM_GEM_DMA_HELPER_H__

#include <drm/drm_gem.h>

struct drm_mode_create_dumb;

/**
 * struct drm_gem_dma_object - GEM object backed by DMA memory allocations
 * @base: base GEM object
 * @dma_addr: DMA address of the backing memory
 * @sgt: scatter/gather table for imported PRIME buffers
 * @vaddr: kernel virtual address of the backing memory
 * @map_noncoherent: if true, the GEM object is backed by non-coherent memory
 */
struct drm_gem_dma_object {
	struct drm_gem_object base;
	dma_addr_t dma_addr;
	struct sg_table *sgt;
	void *vaddr;
	bool map_noncoherent;
};

#define to_drm_gem_dma_obj(gem_obj) \
	container_of(gem_obj, struct drm_gem_dma_object, base)

struct drm_gem_dma_object *drm_gem_dma_create(struct drm_device *drm,
					      size_t size);
void drm_gem_dma_free(struct drm_gem_dma_object *dma_obj);
void drm_gem_dma_free_object(struct drm_gem_object *obj);
struct sg_table *drm_gem_dma_get_sg_table(struct drm_gem_dma_object *dma_obj);
int drm_gem_dma_vmap(struct drm_gem_dma_object *dma_obj,
		     struct iosys_map *map);

int drm_gem_dma_dumb_create(struct drm_file *file_priv,
			    struct drm_device *drm,
			    struct drm_mode_create_dumb *args);
int drm_gem_dma_dumb_create_internal(struct drm_file *file_priv,
				     struct drm_device *drm,
				     struct drm_mode_create_dumb *args);
int drm_gem_dma_dumb_map_offset(struct drm_file *file_priv,
				struct drm_device *drm,
				uint32_t handle, uint64_t *offset);

#ifdef __linux__
int drm_gem_dma_mmap(struct drm_gem_dma_object *dma_obj,
		     struct vm_area_struct *vma);
extern const struct vm_operations_struct drm_gem_dma_vm_ops;
#endif

#define DEFINE_DRM_GEM_DMA_FOPS(name) struct file_operations name = {};

#define DRM_GEM_DMA_DRIVER_OPS_WITH_DUMB_CREATE(x)	\
	.dumb_create = (x)

#endif /* __DRM_GEM_DMA_HELPER_H__ */
