/* Public domain. */

#ifndef _LINUX_FIRMWARE_H
#define _LINUX_FIRMWARE_H

#include <sys/types.h>
#include <sys/sunddi.h>
#include <linux/types.h>
#include <linux/gfp.h>

#ifndef __DECONST
#define __DECONST(type, var)	((type)(__uintptr_t)(const void *)(var))
#endif

struct firmware {
	size_t size;
	const u8 *data;
};

/*
 * illumos Phase 1: firmware loading is stubbed — no loadfirmware equivalent.
 * Drivers must provide their own firmware loading mechanism.
 */
static inline int
request_firmware(const struct firmware **fw, const char *name,
    struct device *device)
{
	*fw = NULL;
	return -ENOENT;
}

static inline int
firmware_request_nowarn(const struct firmware **fw, const char *name,
    struct device *device)
{
	*fw = NULL;
	return -ENOENT;
}

static inline int
request_firmware_direct(const struct firmware **fw, const char *name,
    struct device *device)
{
	*fw = NULL;
	return -ENOENT;
}

#define request_firmware_nowait(a, b, c, d, e, f, g) -EINVAL

static inline void
release_firmware(const struct firmware *fw)
{
	if (fw == NULL)
		return;
	if (fw->data)
		kmem_free(__DECONST(void *, fw->data), fw->size);
	kmem_free(__DECONST(void *, fw), sizeof(struct firmware));
}

#endif
