#ifndef _DRM_ILLUMOS_H_
#define _DRM_ILLUMOS_H_

#include <linux/illumos_page_compat.h>

#include <sys/types.h>
#include <sys/poll.h>
#include <sys/mutex.h>
#include <sys/sunddi.h>

#include <linux/fs.h>
#include <linux/interrupt.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Return convention for drm_illumos_* bridge functions:
 *
 * Functions return a positive errno value (e.g. ENOMEM, EINVAL) on failure,
 * and 0 on success. This matches the illumos kernel driver convention.
 *
 * When called from Linux-derived code (which expects negative errno),
 * the caller is responsible for negating the return value.
 */

struct drm_device;
struct drm_file;
struct drm_minor;
struct pci_dev;
struct pollhead;

#define DRM_ILUMOS_MAX_OPENS 64

/*
 * Minor encoding:
 * bits 0-5:   Open slot (0-63)
 * bits 6-7:   DRM node kind (0=primary, 1=control, 2=render)
 * bits 8-31:  Driver instance
 */
#define DRM_ILLUMOS_SLOT_MASK		0x3F
#define DRM_ILLUMOS_KIND_MASK		0x3
#define DRM_ILLUMOS_KIND_SHIFT		6
#define DRM_ILLUMOS_INST_SHIFT		8

#define DRM_ILLUMOS_KIND_PRIMARY	0
#define DRM_ILLUMOS_KIND_CONTROL	1
#define DRM_ILLUMOS_KIND_RENDER		2

static inline minor_t
drm_illumos_encode_minor(int instance, int kind, int slot)
{
	return (minor_t)((instance << DRM_ILLUMOS_INST_SHIFT) |
	    ((kind & DRM_ILLUMOS_KIND_MASK) << DRM_ILLUMOS_KIND_SHIFT) |
	    (slot & DRM_ILLUMOS_SLOT_MASK));
}

static inline int
drm_illumos_decode_inst(minor_t minor)
{
	return (int)(minor >> DRM_ILLUMOS_INST_SHIFT);
}

static inline int
drm_illumos_decode_kind(minor_t minor)
{
	return (int)((minor >> DRM_ILLUMOS_KIND_SHIFT) & DRM_ILLUMOS_KIND_MASK);
}

static inline int
drm_illumos_decode_slot(minor_t minor)
{
	return (int)(minor & DRM_ILLUMOS_SLOT_MASK);
}

struct drm_illumos_open {
	struct file filp;
	struct drm_minor *minor;
	bool in_use;
};

struct drm_illumos_file_state {
	kmutex_t mutex;
	struct drm_illumos_open opens[DRM_ILUMOS_MAX_OPENS];
};

struct drm_illumos_irq_vector {
	irq_handler_t	handler;
	irq_handler_t	thread_fn;
	void		*dev_id;
	const char	*name;
};

struct drm_illumos_irq_state {
	ddi_intr_handle_t *intr_hdls;
	int		intr_type;
	int		nvec;
	struct drm_illumos_irq_vector *vectors;
	taskq_t		*tq;
	bool		registered;
};

void drm_illumos_file_state_init(struct drm_illumos_file_state *state);
void drm_illumos_file_state_destroy(struct drm_illumos_file_state *state);

int drm_illumos_open(struct drm_illumos_file_state *state,
    struct drm_device *drm, int instance, int kind, dev_t *devp);
int drm_illumos_close(struct drm_illumos_file_state *state, dev_t dev);
int drm_illumos_ioctl(struct drm_illumos_file_state *state, dev_t dev,
    int cmd, intptr_t arg);
int drm_illumos_chpoll(struct drm_illumos_file_state *state, dev_t dev,
    short events, int anyyet, short *reventsp, struct pollhead **phpp);
int drm_illumos_gem_ttm_devmap(struct drm_device *drm, devmap_cookie_t dhp,
    offset_t off, size_t len, size_t *maplen);
int drm_illumos_irq_install(dev_info_t *dip, struct drm_illumos_irq_state *irq,
    const char *taskq_name, irq_handler_t handler, irq_handler_t thread_fn,
    void *dev_id);
int drm_illumos_irq_install_multivector(dev_info_t *dip,
    struct drm_illumos_irq_state *irq, const char *taskq_name,
    uint_t nvec_requested, uint_t *nvec_actual,
    const struct drm_illumos_irq_vector *vectors);
void drm_illumos_irq_uninstall(struct drm_illumos_irq_state *irq);
void drm_illumos_pci_init_device(struct pci_dev *pdev, dev_info_t *dip,
    ddi_acc_handle_t cfg_handle);

#ifdef __cplusplus
}
#endif

#endif
