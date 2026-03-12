/* Public domain. */

#ifndef _LINUX_FONT_H
#define _LINUX_FONT_H

#include <sys/types.h>
#include <sys/param.h>
#include <linux/types.h>

/*
 * Stub for Linux kernel font support (used by drm_panic.c).
 * On illumos, console font is managed separately; provide stub types.
 */

struct font_desc {
	int		idx;
	const char	*name;
	int		width, height, count;
	int		pref;
	const void	*data;
	unsigned int	charcount;
};

/* Returns NULL on illumos (no kernel font support in Phase 1) */
static inline const struct font_desc *
get_default_font(int width, int height, u32 *glyphs_asked, u32 *glyphs_available)
{
	(void)width; (void)height;
	(void)glyphs_asked; (void)glyphs_available;
	return NULL;
}

#endif
