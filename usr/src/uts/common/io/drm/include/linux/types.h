/* Public domain. */

#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

/*
 * Block illumos sys/file.h which defines "struct file" (file_t).
 * This conflicts with our Linux-compat struct file in linux/fs.h.
 * sys/file.h is pulled in transitively by many system headers
 * (e.g. sys/cpuvar.h, sys/proc.h → sys/user.h).
 */
#ifndef _SYS_FILE_H
#define _SYS_FILE_H
#endif

#include <sys/types.h>
#include <sys/int_types.h>
#include <stdbool.h>
#include <linux/compiler.h>

typedef int8_t   __s8;
typedef uint8_t  __u8;
typedef int16_t  __s16;
typedef uint16_t __u16;
typedef int32_t  __s32;
typedef uint32_t __u32;
typedef int64_t  __s64;
typedef uint64_t __u64;

typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;

typedef uint16_t __le16;
typedef uint16_t __be16;
typedef uint32_t __le32;
typedef uint32_t __be32;
typedef uint64_t __le64;
typedef uint64_t __be64;

typedef uint64_t dma_addr_t;
typedef uint64_t phys_addr_t;
typedef uint64_t resource_size_t;

typedef off_t loff_t;

typedef ptrdiff_t ptrdiff_t;

typedef unsigned int umode_t;
typedef unsigned int gfp_t;

typedef unsigned long pgoff_t;
#ifndef __pgprot_t_defined
#define __pgprot_t_defined
typedef unsigned long pgprot_t;
#endif

typedef int atomic_t;

struct list_head {
	struct list_head *next, *prev;
};

struct hlist_node {
	struct hlist_node *next, **prev;
};

struct hlist_head {
	struct hlist_node *first;
};

#define DECLARE_BITMAP(x, y)	unsigned long x[BITS_TO_LONGS(y)];

/* Linux page mask (complement of page-aligned bits) */
#define LINUX_PAGE_MASK		(PAGE_SIZE - 1)

#endif /* _LINUX_TYPES_H */
