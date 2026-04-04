/* Public domain. */

#ifndef _LINUX_IO_H
#define _LINUX_IO_H

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/bus.h>		/* bus_space_tag_t / bus_space_handle_t / bus_size_t */

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/vmalloc.h> /* via asm/io.h */

#define memcpy_toio(d, s, n)	memcpy(d, s, n)
#define memcpy_fromio(d, s, n)	memcpy(d, s, n)
#define memset_io(d, b, n)	memset(d, b, n)

#define iobarrier()		barrier()

/*
 * MMIO accessors — illumos x86 is little-endian, so MMIO reads are
 * direct volatile pointer dereferences (same as the non-byteswapping path).
 */

static inline u8
ioread8(const volatile void __iomem *addr)
{
	u8 val;
	iobarrier();
	val = *(volatile uint8_t *)addr;
	rmb();
	return val;
}

static inline void
iowrite8(u8 val, volatile void __iomem *addr)
{
	wmb();
	*(volatile uint8_t *)addr = val;
}

static inline u16
ioread16(const volatile void __iomem *addr)
{
	uint16_t val;
	iobarrier();
	val = *(volatile uint16_t *)addr;
	rmb();
	return val;
}

static inline u32
ioread32(const volatile void __iomem *addr)
{
	uint32_t val;
	iobarrier();
	val = *(volatile uint32_t *)addr;
	rmb();
	return val;
}

static inline u64
ioread64(const volatile void __iomem *addr)
{
	uint64_t val;
	iobarrier();
	val = *(volatile uint64_t *)addr;
	rmb();
	return val;
}

static inline void
iowrite16(u16 val, volatile void __iomem *addr)
{
	wmb();
	*(volatile uint16_t *)addr = val;
}

static inline void
iowrite32(u32 val, volatile void __iomem *addr)
{
	wmb();
	*(volatile uint32_t *)addr = val;
}

static inline void
iowrite64(u64 val, volatile void __iomem *addr)
{
	wmb();
	*(volatile uint64_t *)addr = val;
}

#define readb(p) ioread8(p)
#define writeb(v, p) iowrite8(v, p)
#define readw(p) ioread16(p)
#define writew(v, p) iowrite16(v, p)
#define readl(p) ioread32(p)
#define writel(v, p) iowrite32(v, p)
#define readq(p) ioread64(p)
#define writeq(v, p) iowrite64(v, p)

#define readl_relaxed(p) readl(p)
#define writel_relaxed(v, p) writel(v, p)

int	drm_mtrr_add(unsigned long, size_t, int);
int	drm_mtrr_del(int, unsigned long, size_t, int);

/* Write-combining MTRR type */
#define DRM_MTRR_WC	1

static inline void *
IOMEM_ERR_PTR(long error)
{
	return (void *) error;
}

#define MEMREMAP_WB	(1 << 0)

void	*memremap(phys_addr_t, size_t, int);
void	memunmap(void *);

/*
 * illumos exports outl(port, value) / inl(port) — opposite of Linux convention
 * outl(value, port) / inl(port).  Override with inline asm using Linux order.
 */
#ifdef outl
#undef outl
#endif
#ifdef outw
#undef outw
#endif
#ifdef outb
#undef outb
#endif
#ifdef inl
#undef inl
#endif
#ifdef inw
#undef inw
#endif
#ifdef inb
#undef inb
#endif

#define outl(val, port)  __asm__ volatile("outl %0,%w1" : : "a"((uint32_t)(val)), "Nd"((uint16_t)(port)))
#define outw(val, port)  __asm__ volatile("outw %0,%w1" : : "a"((uint16_t)(val)), "Nd"((uint16_t)(port)))
#define outb(val, port)  __asm__ volatile("outb %0,%w1" : : "a"((uint8_t)(val)),  "Nd"((uint16_t)(port)))
#define inl(port)  ({ uint32_t _v; __asm__ volatile("inl %w1,%0" : "=a"(_v) : "Nd"((uint16_t)(port))); _v; })
#define inw(port)  ({ uint16_t _v; __asm__ volatile("inw %w1,%0" : "=a"(_v) : "Nd"((uint16_t)(port))); _v; })
#define inb(port)  ({ uint8_t  _v; __asm__ volatile("inb %w1,%0" : "=a"(_v) : "Nd"((uint16_t)(port))); _v; })

#define DRM_MEM_CACHED 0
#define DRM_MEM_UNCACHED 1
#define DRM_MEM_WC 2

void *drm_sun_ioremap(uint64_t paddr, size_t size, uint32_t mode);
void drm_sun_iounmap(void *addr);

#define ioremap_cache(base, size) \
	drm_sun_ioremap((base), (size), DRM_MEM_CACHED)
#define ioremap_wc(base, size) drm_sun_ioremap((base), (size), DRM_MEM_WC)
#define ioremap(base, size) drm_sun_ioremap((base), (size), DRM_MEM_UNCACHED)
#define iounmap(addr) drm_sun_iounmap((addr))

#endif
