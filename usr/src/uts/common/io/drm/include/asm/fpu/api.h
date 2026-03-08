/* Public domain. */

#ifndef _ASM_FPU_API_H
#define _ASM_FPU_API_H

#include <linux/bottom_half.h>

/*
 * illumos: kernel FPU save/restore.
 * On illumos x86, kfpu_begin()/kfpu_end() save and restore FPU state
 * allowing kernel code to use SSE/AVX instructions.
 * Declared in <sys/x86_archext.h> on OpenIndiana/illumos-gate.
 */
#if defined(__i386__) || defined(__amd64__)

extern void kfpu_begin(void);
extern void kfpu_end(void);

#define kernel_fpu_begin()	kfpu_begin()
#define kernel_fpu_end()	kfpu_end()

#endif /* __i386__ || __amd64__ */

#endif /* _ASM_FPU_API_H */
