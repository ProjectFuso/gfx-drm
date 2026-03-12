/* Public domain. */

#ifndef _LINUX_MM_TYPES_H
#define _LINUX_MM_TYPES_H

/* vm_fault_t and VM_FAULT_* flags (match Linux bit values) */
typedef unsigned int vm_fault_t;

#define VM_FAULT_OOM		0x000001
#define VM_FAULT_SIGBUS		0x000002
#define VM_FAULT_MAJOR		0x000004
#define VM_FAULT_HWPOISON	0x000010
#define VM_FAULT_FALLBACK	0x000800
#define VM_FAULT_RETRY		0x000400
#define VM_FAULT_NOPAGE		0x000100
#define VM_FAULT_LOCKED		0x000200
#define VM_FAULT_ERROR		(VM_FAULT_OOM | VM_FAULT_SIGBUS | \
				 VM_FAULT_HWPOISON | VM_FAULT_FALLBACK)
#define VM_FAULT_DONE_COW	0x001000
#define VM_FAULT_NEEDDSYNC	0x002000

/* VMA flags */
#define VM_READ		0x00000001
#define VM_WRITE	0x00000002
#define VM_EXEC		0x00000004
#define VM_SHARED	0x00000008
#define VM_RAND_READ	0x00000040
#define VM_SEQ_READ	0x00000080
#define VM_DONTCOPY	0x00000100
#define VM_DONTEXPAND	0x00000200
#define VM_IO		0x00004000
#define VM_PFNMAP	0x00000400
#define VM_MIXEDMAP	0x00000800
#define VM_NORESERVE	0x00200000
#define VM_WIPEONFORK	0x02000000
#define VM_DONTDUMP	0x04000000
#define VM_SOFTDIRTY	0x00000000	/* stub — no soft-dirty on illumos */

/* Fault flags passed in vmf->flags */
#define FAULT_FLAG_WRITE		(1U << 0)
#define FAULT_FLAG_MKWRITE		(1U << 1)
#define FAULT_FLAG_ALLOW_RETRY		(1U << 2)
#define FAULT_FLAG_RETRY_NOWAIT		(1U << 3)
#define FAULT_FLAG_KILLABLE		(1U << 4)
#define FAULT_FLAG_TRIED		(1U << 5)
#define FAULT_FLAG_USER			(1U << 6)
#define FAULT_FLAG_REMOTE		(1U << 7)
#define FAULT_FLAG_INSTRUCTION		(1U << 8)
#define FAULT_FLAG_INTERRUPTIBLE	(1U << 9)
#define FAULT_FLAG_UNSHARE		(1U << 10)
#define FAULT_FLAG_ORIG_PTE_VALID	(1U << 11)

/* pte_t — page table entry (x86-64 is just a uint64) */
typedef uint64_t pte_t;

/* Forward declarations to resolve ordering */
struct vm_area_struct;
struct vm_fault;
struct address_space;
struct file;
struct vm_operations_struct;

/* vm_fault is used by mmap fault handlers */
struct vm_fault {
	struct vm_area_struct	*vma;
	void			*virtual_address;
	unsigned long		pgoff;
	unsigned int		flags;
};

struct vm_area_struct {
	unsigned long			vm_start;
	unsigned long			vm_end;
	unsigned long			vm_flags;
	void				*vm_private_data;
	const struct vm_operations_struct *vm_ops;
	unsigned long			vm_pgoff;
	pgprot_t			vm_page_prot;
	struct file			*vm_file;
};

struct vm_operations_struct {
	void	(*open)(struct vm_area_struct *vma);
	void	(*close)(struct vm_area_struct *vma);
	vm_fault_t (*fault)(struct vm_fault *vmf);
	vm_fault_t (*huge_fault)(struct vm_fault *vmf, unsigned int order);
	vm_fault_t (*page_mkwrite)(struct vm_fault *vmf);
	vm_fault_t (*pfn_mkwrite)(struct vm_fault *vmf);
	int	(*access)(struct vm_area_struct *vma, unsigned long addr,
			  void *buf, int len, int write);
};

/* vm_get_page_prot — Phase 1: always return write-enabled user protection */
static inline pgprot_t
vm_get_page_prot(unsigned long vm_flags)
{
	return (pgprot_t)0;		/* illumos Phase 1 stub */
}

/* Mapping range helpers — Phase 1 stubs */
/*
 * clean_record_shared_mapping_range — walks mapping pages and records
 * dirty range. Phase 1: no page cache, returns 0 pages marked.
 */
static inline pgoff_t
clean_record_shared_mapping_range(struct address_space *mapping,
    pgoff_t first_index, pgoff_t nr, pgoff_t lock_start,
    unsigned long *bitmap, pgoff_t *start, pgoff_t *end)
{
	return 0;
}

/*
 * wp_shared_mapping_range — write-protect pages in a shared mapping range.
 * Phase 1: returns 0.
 */
static inline pgoff_t
wp_shared_mapping_range(struct address_space *mapping,
    pgoff_t first_index, pgoff_t nr)
{
	return 0;
}

static inline void
unmap_shared_mapping_range(struct address_space *mapping,
    loff_t const holebegin, loff_t const holelen)
{
}

#endif
