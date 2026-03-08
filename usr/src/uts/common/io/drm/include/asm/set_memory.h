/*	$OpenBSD: set_memory.h,v 1.5 2023/01/01 01:34:58 jsg Exp $	*/
/*
 * Copyright (c) 2013, 2014, 2015 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ASM_SET_MEMORY_H
#define _ASM_SET_MEMORY_H

#include <sys/types.h>
#include <sys/param.h>		/* for PAGE_SIZE */

/* illumos: uvm/uvm_extern.h and machine/pmap.h removed */
/* struct page is a forward decl from linux/gfp.h */
#include <linux/gfp.h>

#if defined(__amd64__) || defined(__i386__)

/*
 * illumos Phase 1: WB/WC cache-type operations are no-op stubs.
 * Full implementation requires HAT-level page attribute management.
 */
static inline int
set_pages_array_wb(struct page **pages, int addrinarray)
{
	return 0;
}

static inline int
set_pages_array_wc(struct page **pages, int addrinarray)
{
	return 0;
}

static inline int
set_pages_array_uc(struct page **pages, int addrinarray)
{
	return 0;
}

static inline int
set_pages_wb(struct page *page, int numpages)
{
	return 0;
}

static inline int
set_pages_uc(struct page *page, int numpages)
{
	return 0;
}

#endif /* defined(__amd64__) || defined(__i386__) */

#endif
