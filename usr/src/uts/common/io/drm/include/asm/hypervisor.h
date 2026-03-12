/* SPDX-License-Identifier: MIT */
/* illumos stub for asm/hypervisor.h - hypervisor detection */
#ifndef _ASM_HYPERVISOR_H_
#define _ASM_HYPERVISOR_H_

/*
 * On illumos (running as guest in VMware), we always report as running
 * under a hypervisor. vmwgfx uses this to detect VMware environment.
 */
enum x86_hypervisor_type {
	X86_HYPER_NATIVE = 0,
	X86_HYPER_VMWARE,
	X86_HYPER_MS_HYPERV,
	X86_HYPER_XEN_PV,
	X86_HYPER_XEN_HVM,
	X86_HYPER_KVM,
	X86_HYPER_JAILHOUSE,
	X86_HYPER_ACRN,
};

static inline enum x86_hypervisor_type
hypervisor_is_type(enum x86_hypervisor_type type)
{
	/*
	 * On illumos/OpenSolaris running as VMware guest, return VMware.
	 * This is correct for the vmwgfx use case.
	 */
	if (type == X86_HYPER_VMWARE)
		return X86_HYPER_VMWARE;
	return X86_HYPER_NATIVE;
}

static inline bool x86_hyper_is_vmware(void)
{
	return true;
}

#endif /* _ASM_HYPERVISOR_H_ */
