/* Public domain — illumos port of Linux arch/x86/include/asm/vmware.h */

#ifndef _ASM_VMWARE_H
#define _ASM_VMWARE_H

#include <linux/types.h>

/*
 * VMware backdoor hypercall port and magic.
 * These match the documented VMware backdoor interface.
 */
#define VMWARE_HYPERVISOR_MAGIC		0x564D5868U
#define VMWARE_HYPERVISOR_PORT		0x5658U
#define VMWARE_HYPERVISOR_PORT_HB	0x5659U

/*
 * vmware_hypercall1 — issue a backdoor call with one input, no output.
 * EAX = magic, EBX = in1, ECX = cmd, EDX = port.
 */
static __inline__ void
vmware_hypercall1(unsigned long cmd, unsigned long in1)
{
	__asm__ __volatile__(
		"inl (%%dx)"
		: /* no outputs */
		: "a" (VMWARE_HYPERVISOR_MAGIC),
		  "b" (in1),
		  "c" (cmd),
		  "d" (VMWARE_HYPERVISOR_PORT)
		: "memory"
	);
}

/*
 * vmware_hypercall5 — backdoor call: 4 inputs + cmd, 1 output.
 * Inputs:  EAX=magic, EBX=in1, ECX=cmd, EDX=in2, ESI=in3, EDI=in4
 * Output:  ECX → *out1
 */
static __inline__ void
vmware_hypercall5(unsigned long cmd, unsigned long in1, unsigned long in2,
    unsigned long in3, unsigned long in4, uint32_t *out1)
{
	uint32_t ecx;

	__asm__ __volatile__(
		"inl (%%dx)"
		: "=c" (ecx)
		: "a" (VMWARE_HYPERVISOR_MAGIC),
		  "b" (in1),
		  "c" (cmd),
		  "d" (in2),
		  "S" (in3),
		  "D" (in4)
		: "memory"
	);
	*out1 = ecx;
}

/*
 * vmware_hypercall6 — backdoor call: 2 inputs + cmd, 4 outputs.
 * Inputs:  EAX=magic, EBX=in1, ECX=cmd, EDX=in2
 * Outputs: ECX→*out1, EDX→*out2, ESI→*out3, EDI→*out4
 */
static __inline__ void
vmware_hypercall6(unsigned long cmd, unsigned long in1, unsigned long in2,
    uint32_t *out1, uint32_t *out2, uint32_t *out3, uint32_t *out4)
{
	uint32_t ecx, edx, esi, edi;

	__asm__ __volatile__(
		"inl (%%dx)"
		: "=c" (ecx), "=d" (edx), "=S" (esi), "=D" (edi)
		: "a" (VMWARE_HYPERVISOR_MAGIC),
		  "b" (in1),
		  "c" (cmd),
		  "d" (in2)
		: "memory"
	);
	*out1 = ecx;
	*out2 = edx;
	*out3 = esi;
	*out4 = edi;
}

/*
 * vmware_hypercall7 — backdoor call: 4 inputs + cmd, 3 outputs.
 * Inputs:  EAX=magic, EBX=in1, ECX=cmd, EDX=in2, ESI=in3, EDI=in4
 * Outputs: EBX→*out1, ECX→*out2, EDX→*out3
 */
static __inline__ void
vmware_hypercall7(unsigned long cmd, unsigned long in1, unsigned long in2,
    unsigned long in3, unsigned long in4,
    uint32_t *out1, uint32_t *out2, uint32_t *out3)
{
	uint32_t ebx, ecx, edx;

	__asm__ __volatile__(
		"inl (%%dx)"
		: "=b" (ebx), "=c" (ecx), "=d" (edx)
		: "a" (VMWARE_HYPERVISOR_MAGIC),
		  "b" (in1),
		  "c" (cmd),
		  "d" (in2),
		  "S" (in3),
		  "D" (in4)
		: "memory"
	);
	*out1 = ebx;
	*out2 = ecx;
	*out3 = edx;
}

/*
 * vmware_hypercall_hb_out — high-bandwidth backdoor out.
 * Inputs:  EAX=magic, EBX=cmd(HB), ECX=in1(len), EDX=port_hb,
 *          ESI=in2(addr), EBP=in3, EDI=in4
 * Output:  EBX → *out1
 */
static __inline__ void
vmware_hypercall_hb_out(unsigned long cmd, unsigned long in1,
    unsigned long in2, unsigned long addr, unsigned long in3,
    unsigned long in4, uint32_t *out1)
{
	uint32_t ebx;

	__asm__ __volatile__(
		"push %%rbp\n\t"
		"movq %7, %%rbp\n\t"
		"cld\n\t"
		"rep outsb\n\t"		/* HB out: rep outsb from ESI */
		"pop %%rbp"
		: "=b" (ebx)
		: "a" (VMWARE_HYPERVISOR_MAGIC),
		  "b" (cmd),
		  "c" (in1),
		  "d" (VMWARE_HYPERVISOR_PORT_HB),
		  "S" (addr),
		  "D" (in2),
		  "r" (in3)
		: "memory", "cc"
	);
	*out1 = ebx;
}

/*
 * vmware_hypercall_hb_in — high-bandwidth backdoor in.
 * Inputs:  EAX=magic, EBX=cmd(HB), ECX=in1(len), EDX=port_hb,
 *          ESI=in2, EBP=in3, EDI=addr
 * Output:  EBX → *out1
 */
static __inline__ void
vmware_hypercall_hb_in(unsigned long cmd, unsigned long in1,
    unsigned long in2, unsigned long in3, unsigned long addr,
    unsigned long in4, uint32_t *out1)
{
	uint32_t ebx;

	__asm__ __volatile__(
		"push %%rbp\n\t"
		"movq %7, %%rbp\n\t"
		"cld\n\t"
		"rep insb\n\t"		/* HB in: rep insb to EDI */
		"pop %%rbp"
		: "=b" (ebx)
		: "a" (VMWARE_HYPERVISOR_MAGIC),
		  "b" (cmd),
		  "c" (in1),
		  "d" (VMWARE_HYPERVISOR_PORT_HB),
		  "S" (in2),
		  "D" (addr),
		  "r" (in3)
		: "memory", "cc"
	);
	*out1 = ebx;
}

#endif /* _ASM_VMWARE_H */
