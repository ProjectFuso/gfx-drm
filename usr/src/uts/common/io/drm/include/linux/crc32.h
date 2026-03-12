/* SPDX-License-Identifier: MIT */
/* illumos stub for linux/crc32.h */
#ifndef _LINUX_CRC32_H_
#define _LINUX_CRC32_H_

#include <linux/types.h>

/* Simple CRC32 table-based implementation */
static inline uint32_t
crc32_le(uint32_t crc, const uint8_t *p, size_t len)
{
	uint32_t i;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
	}
	return crc;
}

#define crc32(seed, data, length)  crc32_le(seed, (const uint8_t *)(data), length)

#endif /* _LINUX_CRC32_H_ */
