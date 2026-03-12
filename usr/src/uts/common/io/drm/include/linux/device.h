/* Public domain. */

#ifndef _LINUX_DEVICE_H
#define _LINUX_DEVICE_H

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <linux/illumos_page_compat.h>
#include <sys/sunddi.h>
#include <sys/varargs.h>
#include <sys/bus.h>		/* bus_space_tag_t / bus_dma_tag_t illumos stubs */
#include <linux/ioport.h>
#include <linux/lockdep.h>
#include <linux/pm.h>
#include <linux/kobject.h>
#include <linux/ratelimit.h> /* dev_printk.h -> ratelimit.h */
#include <linux/module.h> /* via device/driver.h */
#include <linux/device/bus.h>
#include <linux/numa.h>		/* dev_to_node() */

struct device_node;

/*
 * illumos struct device: minimal Linux-compat device wrapper.
 * The Linux DRM code uses this as the generic device handle.
 * On illumos, real DDI operations use dev->dip directly.
 */
struct device {
	dev_info_t		*dip;		/* illumos DDI device info node */
	struct device_node	*of_node;	/* device tree node (NULL on x86) */
	const char		*init_name;	/* device name string */
	struct pci_dev		*pdev;		/* back-pointer to PCI device */
};

struct device_driver {
	const char		*name;
	struct device		*dev;
	const struct dev_pm_ops	*pm;
};

struct device_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device *, struct device_attribute *, char *);
};

#define DEVICE_ATTR(_name, _mode, _show, _store) \
	struct device_attribute dev_attr_##_name
#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name

#define device_create_file(a, b)	0
#define device_remove_file(a, b)

void	*dev_get_drvdata(struct device *);
void	dev_set_drvdata(struct device *, void *);

#define dev_pm_set_driver_flags(x, y)

#define devm_kzalloc(x, y, z)	kzalloc(y, z)
#define devm_kfree(x, y)	kfree(y)

static inline int
devm_device_add_group(struct device *dev, const struct attribute_group *g)
{
	return 0;
}

/* dev_* print macros — use cmn_err for illumos kernel context */
#define dev_warn(dev, fmt, ...)	\
	cmn_err(CE_WARN, "!drm: " fmt, ##__VA_ARGS__)
#define dev_WARN(dev, fmt, ...) \
	WARN(1, "drm: " fmt, ##__VA_ARGS__)
#define dev_notice(dev, fmt, ...) \
	cmn_err(CE_NOTE, "!drm: " fmt, ##__VA_ARGS__)
#define dev_crit(dev, fmt, ...) \
	cmn_err(CE_WARN, "!drm: " fmt, ##__VA_ARGS__)
#define dev_err(dev, fmt, ...) \
	cmn_err(CE_WARN, "!drm: " fmt, ##__VA_ARGS__)
#define dev_emerg(dev, fmt, ...) \
	cmn_err(CE_WARN, "!drm: " fmt, ##__VA_ARGS__)
#define dev_printk(level, dev, fmt, ...) \
	cmn_err(CE_CONT, "!" fmt, ##__VA_ARGS__)

#define dev_warn_ratelimited	dev_warn
#define dev_notice_ratelimited	dev_notice
#define dev_err_ratelimited	dev_err
#define dev_warn_once		dev_warn
#define dev_WARN_ONCE(dev, cond, fmt, ...) \
	WARN_ONCE(cond, "drm: " fmt, ##__VA_ARGS__)
#define dev_err_once		dev_err
#define dev_err_probe(dev, err, fmt, ...) \
	({ cmn_err(CE_WARN, "!drm: " fmt, ##__VA_ARGS__); (err); })

#ifdef DRMDEBUG

static inline void
dev_info(struct device *dev, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vcmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

static inline void
dev_info_once(struct device *dev, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vcmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

static inline void
dev_dbg(struct device *dev, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vcmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

static inline void
dev_dbg_once(struct device *dev, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vcmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

static inline void
dev_dbg_ratelimited(struct device *dev, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vcmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

#else

static inline void dev_info(struct device *dev, const char *fmt, ...) {}
static inline void dev_info_once(struct device *dev, const char *fmt, ...) {}
static inline void dev_dbg(struct device *dev, const char *fmt, ...) {}
static inline void dev_dbg_once(struct device *dev, const char *fmt, ...) {}
static inline void dev_dbg_ratelimited(struct device *dev, const char *fmt, ...) {}

#endif

static inline const char *
dev_driver_string(struct device *dev)
{
	if (dev != NULL && dev->dip != NULL)
		return ddi_driver_name(dev->dip);
	return "drm";
}

/* XXX return true for thunderbolt/USB4 */
#define dev_is_removable(x)	false

/* should be bus id as string, ie 0000:00:02.0 */
#define dev_name(dev)		""

static inline void
device_set_wakeup_path(struct device *dev)
{
}

#endif
