/* Public domain. */

#ifndef _LINUX_FB_H
#define _LINUX_FB_H

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/visual_io.h>
#include <linux/bug.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <linux/backlight.h>
#include <linux/kgdb.h>
#include <linux/fs.h>
#include <linux/i2c.h> /* via uapi/linux/fb.h */

struct fb_cmap;
struct fb_fillrect;
struct fb_copyarea;
struct fb_image;
struct fb_info;
struct module;
struct vm_area_struct;

struct apertures_struct;

struct fb_bitfield {
	uint32_t offset;
	uint32_t length;
	uint32_t msb_right;
};

struct fb_var_screeninfo {
	int pixclock;
	uint32_t xres;
	uint32_t yres;
	uint32_t xres_virtual;
	uint32_t yres_virtual;
	uint32_t xoffset;
	uint32_t yoffset;
	uint32_t width;
	uint32_t height;
	uint32_t bits_per_pixel;
	uint32_t grayscale;
	uint32_t nonstd;
	uint32_t activate;
	uint32_t accel_flags;
	uint32_t left_margin;
	uint32_t right_margin;
	uint32_t upper_margin;
	uint32_t lower_margin;
	uint32_t hsync_len;
	uint32_t vsync_len;
	uint32_t sync;
	uint32_t vmode;
	uint32_t rotate;
	uint32_t colorspace;
	uint32_t reserved[4];
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;
};

struct fb_cmap {
	uint32_t start;
	uint32_t len;
	uint16_t *red;
	uint16_t *green;
	uint16_t *blue;
	uint16_t *transp;
};

struct fb_deferred_io_pageref {
	struct list_head list;
	unsigned long offset;
};

struct fb_deferred_io {
	unsigned long delay;
	struct page *(*get_page)(struct fb_info *info, unsigned long offset);
	void (*deferred_io)(struct fb_info *info, struct list_head *pagereflist);
};

struct fb_ops {
	struct module *owner;
	int (*fb_open)(struct fb_info *, int);
	int (*fb_release)(struct fb_info *, int);
	int (*fb_set_par)(struct fb_info *);
	int (*fb_check_var)(struct fb_var_screeninfo *, struct fb_info *);
	int (*fb_setcmap)(struct fb_cmap *, struct fb_info *);
	int (*fb_blank)(int, struct fb_info *);
	int (*fb_pan_display)(struct fb_var_screeninfo *, struct fb_info *);
	int (*fb_debug_enter)(struct fb_info *);
	int (*fb_debug_leave)(struct fb_info *);
	int (*fb_ioctl)(struct fb_info *, unsigned int, unsigned long);
	ssize_t (*fb_read)(struct fb_info *, char __user *, size_t, loff_t *);
	ssize_t (*fb_write)(struct fb_info *, const char __user *, size_t, loff_t *);
	void (*fb_fillrect)(struct fb_info *, const struct fb_fillrect *);
	void (*fb_copyarea)(struct fb_info *, const struct fb_copyarea *);
	void (*fb_imageblit)(struct fb_info *, const struct fb_image *);
	int (*fb_mmap)(struct fb_info *, struct vm_area_struct *);
	void (*fb_destroy)(struct fb_info *);
};

struct fb_fix_screeninfo {
	char id[16];
	uint64_t smem_start;
	uint64_t smem_len;
	uint32_t type;
	uint32_t visual;
	uint32_t mmio_start;
	uint32_t mmio_len;
	uint32_t type_aux;
	uint32_t xpanstep;
	uint32_t ypanstep;
	uint32_t ywrapstep;
	uint32_t accel;
	uint32_t line_length;
};

struct fb_info {
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	const struct fb_ops *fbops;
	char *screen_buffer;
	char *screen_base;
	size_t screen_size;
	void *par;
	void *pseudo_palette;
	struct fb_cmap cmap;
	struct fb_deferred_io *fbdefio;
	int fbcon_rotate_hint;
	int state;
	int node;
	bool skip_vt_switch;
	bool skip_panic;
	int flags;
};

#define KHZ2PICOS(a)	(1000000000UL/(a))

#define FB_BLANK_UNBLANK	0
#define FB_BLANK_NORMAL		1
#define FB_BLANK_HSYNC_SUSPEND	2
#define FB_BLANK_VSYNC_SUSPEND	3
#define FB_BLANK_POWERDOWN	4

#define FB_ACTIVATE_NOW		0
#define FB_ACTIVATE_KD_TEXT	0x200

#define FBINFO_STATE_RUNNING	0
#define FBINFO_STATE_SUSPENDED	1

#define FBINFO_VIRTFB		0x0001
#define FBINFO_READS_FAST	0x0002
#define FBINFO_HIDE_SMEM_START	0x0004

#define FB_TYPE_PACKED_PIXELS	0

#define FB_VISUAL_PSEUDOCOLOR	0
#define FB_VISUAL_TRUECOLOR	2

#define FB_ACCEL_NONE		0

#define FB_ROTATE_UR		0
#define FB_ROTATE_CW		1
#define FB_ROTATE_UD		2
#define FB_ROTATE_CCW		3

#define FB_DEFAULT_DEFERRED_OPS(a) \
	.fb_read = NULL, \
	.fb_write = NULL, \
	.fb_fillrect = NULL, \
	.fb_copyarea = NULL, \
	.fb_imageblit = NULL
#define __FB_DEFAULT_DEFERRED_OPS_RDWR(a) \
	.fb_read = NULL, \
	.fb_write = NULL
#define __FB_DEFAULT_DEFERRED_OPS_DRAW(a) \
	.fb_fillrect = NULL, \
	.fb_copyarea = NULL, \
	.fb_imageblit = NULL
#define FB_GEN_DEFAULT_DEFERRED_IOMEM_OPS(a, b, c)
#define FB_GEN_DEFAULT_DEFERRED_DMAMEM_OPS(a, b, c)
#define FB_GEN_DEFAULT_DEFERRED_SYSMEM_OPS(a, b, c)
#define fb_WARN_ON_ONCE(info, condition) WARN_ON_ONCE(condition)

static inline struct fb_info *
framebuffer_alloc(size_t size, void *dev)
{
	struct fb_info *fbi = kzalloc(sizeof(struct fb_info) + size, GFP_KERNEL);

	if (fbi != NULL)
		fbi->state = FBINFO_STATE_RUNNING;

	return fbi;
}

static inline void
fb_set_suspend(struct fb_info *fbi, int s)
{
	if (fbi != NULL)
		fbi->state = s ? FBINFO_STATE_SUSPENDED : FBINFO_STATE_RUNNING;
}

static inline void
framebuffer_release(struct fb_info *fbi)
{
	kfree(fbi);
}

static inline int
fb_get_options(const char *name, char **opt)
{
	return 0;
}

static inline int
register_framebuffer(struct fb_info *fbi)
{
	if (fbi == NULL)
		return -EINVAL;
	if (fbi->fbops && fbi->fbops->fb_set_par)
		fbi->fbops->fb_set_par(fbi);
	return 0;
}

static inline void
unregister_framebuffer(struct fb_info *fbi)
{
}

static inline void
fb_deferred_io_cleanup(struct fb_info *fbi)
{
}

static inline int
fb_deferred_io_init(struct fb_info *fbi)
{
	return 0;
}

static inline int
fb_deferred_io_mmap(struct fb_info *fbi, struct vm_area_struct *vma)
{
	return 0;
}

static inline int
fb_alloc_cmap(struct fb_cmap *cmap, int len, int transp)
{
	size_t alloc;

	if (cmap == NULL || len < 0)
		return -EINVAL;

	alloc = sizeof(uint16_t) * (size_t)len;
	cmap->red = kzalloc(alloc, GFP_KERNEL);
	cmap->green = kzalloc(alloc, GFP_KERNEL);
	cmap->blue = kzalloc(alloc, GFP_KERNEL);
	cmap->transp = transp ? kzalloc(alloc, GFP_KERNEL) : NULL;
	if (cmap->red == NULL || cmap->green == NULL || cmap->blue == NULL ||
	    (transp && cmap->transp == NULL)) {
		kfree(cmap->red);
		kfree(cmap->green);
		kfree(cmap->blue);
		kfree(cmap->transp);
		bzero(cmap, sizeof (*cmap));
		return -ENOMEM;
	}

	cmap->len = (uint32_t)len;
	return 0;
}

static inline void
fb_dealloc_cmap(struct fb_cmap *cmap)
{
	if (cmap == NULL)
		return;

	kfree(cmap->red);
	kfree(cmap->green);
	kfree(cmap->blue);
	kfree(cmap->transp);
	bzero(cmap, sizeof (*cmap));
}

static inline void
get_page(struct page *page)
{
	if (page != NULL)
		page->_refcount++;
}

#endif
