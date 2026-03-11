/* Public domain. */

#ifndef _ASM_BYTEORDER_H
#define _ASM_BYTEORDER_H

#include <sys/byteorder.h>	/* BSWAP_16/32/64 */
#include <linux/types.h>

/*
 * illumos x86 is always little-endian.
 * le*_to_cpu / cpu_to_le* are identity casts.
 * be*_to_cpu / cpu_to_be* use GCC built-in byte-swap.
 *
 * OpenBSD equivalents that were here previously:
 *   letoh16/32/64 → identity (le*_to_cpu)
 *   betoh16/32/64 → bswap   (be*_to_cpu)
 *   htole16/32/64 → identity (cpu_to_le*)
 *   htobe16/32/64 → bswap   (cpu_to_be*)
 *   lemtoh16/32/64  → unaligned LE load
 *   bemtoh16/32/64  → unaligned BE load
 *   htolem16/32/64  → unaligned LE store
 */

#define le16_to_cpu(x)		((uint16_t)(x))
#define le32_to_cpu(x)		((uint32_t)(x))
#define le64_to_cpu(x)		((uint64_t)(x))
#define be16_to_cpu(x)		((uint16_t)__builtin_bswap16(x))
#define be32_to_cpu(x)		((uint32_t)__builtin_bswap32(x))
#define be64_to_cpu(x)		((uint64_t)__builtin_bswap64(x))

#define cpu_to_le16(x)		((uint16_t)(x))
#define cpu_to_le32(x)		((uint32_t)(x))
#define cpu_to_le64(x)		((uint64_t)(x))
#define cpu_to_be16(x)		((uint16_t)__builtin_bswap16(x))
#define cpu_to_be32(x)		((uint32_t)__builtin_bswap32(x))
#define cpu_to_be64(x)		((uint64_t)__builtin_bswap64(x))

/* Unaligned memory loads — x86 handles unaligned natively */
#define le16_to_cpup(p)		le16_to_cpu(*(const uint16_t *)(p))
#define le32_to_cpup(p)		le32_to_cpu(*(const uint32_t *)(p))
#define le64_to_cpup(p)		le64_to_cpu(*(const uint64_t *)(p))
#define be16_to_cpup(p)		be16_to_cpu(*(const uint16_t *)(p))
#define be32_to_cpup(p)		be32_to_cpu(*(const uint32_t *)(p))
#define be64_to_cpup(p)		be64_to_cpu(*(const uint64_t *)(p))

#define get_unaligned_le16(p)	le16_to_cpup(p)
#define get_unaligned_le32(p)	le32_to_cpup(p)
#define get_unaligned_le64(p)	le64_to_cpup(p)
#define get_unaligned_be16(p)	be16_to_cpup(p)
#define get_unaligned_be32(p)	be32_to_cpup(p)
#define get_unaligned_be64(p)	be64_to_cpup(p)

/* Unaligned memory stores */
#define put_unaligned_le16(v, p)  (*(uint16_t *)(p) = cpu_to_le16(v))
#define put_unaligned_le32(v, p)  (*(uint32_t *)(p) = cpu_to_le32(v))
#define put_unaligned_le64(v, p)  (*(uint64_t *)(p) = cpu_to_le64(v))
#define put_unaligned_be16(v, p)  (*(uint16_t *)(p) = cpu_to_be16(v))
#define put_unaligned_be32(v, p)  (*(uint32_t *)(p) = cpu_to_be32(v))
#define put_unaligned_be64(v, p)  (*(uint64_t *)(p) = cpu_to_be64(v))

/* Unconditional byte swaps */
#define swab16(x)	((uint16_t)__builtin_bswap16(x))
#define swab32(x)	((uint32_t)__builtin_bswap32(x))
#define swab64(x)	((uint64_t)__builtin_bswap64(x))

static inline void
le16_add_cpu(uint16_t *p, uint16_t n)
{
	*p = cpu_to_le16(le16_to_cpu(*p) + n);
}

static inline void
le32_add_cpu(uint32_t *p, uint32_t n)
{
	*p = cpu_to_le32(le32_to_cpu(*p) + n);
}

#endif /* _ASM_BYTEORDER_H */
