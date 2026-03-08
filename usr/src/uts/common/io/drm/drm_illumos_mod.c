/*
 * Copyright (c) 2024, illumos contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * illumos DRM module lifecycle: _init / _fini / _info
 * This replaces the old drm_sunmod.c.
 */

#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>

extern void drm_linux_init(void);
extern void drm_linux_exit(void);

static struct modlmisc drm_modlmisc = {
	&mod_miscops,
	"DRM kernel compat"
};

static struct modlinkage drm_modlinkage = {
	MODREV_1,
	{ &drm_modlmisc, NULL }
};

int
_init(void)
{
	drm_linux_init();
	return (mod_install(&drm_modlinkage));
}

int
_fini(void)
{
	int ret;

	ret = mod_remove(&drm_modlinkage);
	if (ret == 0)
		drm_linux_exit();
	return (ret);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&drm_modlinkage, modinfop));
}
