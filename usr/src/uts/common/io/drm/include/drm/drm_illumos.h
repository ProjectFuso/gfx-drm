#ifndef _DRM_ILLUMOS_H_
#define _DRM_ILLUMOS_H_

#include <sys/types.h>
#include <sys/poll.h>
#include <sys/sunddi.h>

#include <linux/fs.h>
#include <linux/interrupt.h>

struct drm_device;
struct drm_file;
struct drm_minor;
struct pollhead;

#define DRM_ILUMOS_MAX_OPENS 64

struct drm_illumos_open {
	struct file filp;
	struct drm_minor *minor;
	bool in_use;
};

struct drm_illumos_file_state {
	struct drm_illumos_open opens[DRM_ILUMOS_MAX_OPENS];
};

struct drm_illumos_irq_state {
	ddi_intr_handle_t intr_hdl;
	irq_handler_t handler;
	irq_handler_t thread_fn;
	void *dev_id;
	taskq_t *tq;
	bool registered;
};

int drm_illumos_open(struct drm_illumos_file_state *state,
    struct drm_device *drm, dev_t *devp);
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
void drm_illumos_irq_uninstall(struct drm_illumos_irq_state *irq);

#endif
