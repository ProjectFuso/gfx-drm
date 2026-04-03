/* Public domain. */

#ifndef _LINUX_FS_H
#define _LINUX_FS_H

#include <sys/types.h>
#include <sys/fcntl.h>
#include <linux/capability.h>
#include <linux/linkage.h>
#include <linux/uuid.h>
#include <linux/pid.h>
#include <linux/radix-tree.h>
#include <linux/wait_bit.h>
#include <linux/err.h>
#include <linux/sched/signal.h>	/* via percpu-rwsem.h -> rcuwait.h */
#include <linux/slab.h>
#include <linux/atomic.h>

struct seq_file;

/*
 * struct address_space — minimal stub; used for page cache management.
 * On illumos Phase 1, page cache ops are stubs.
 */
struct address_space {
	unsigned long	nrpages;
};

/*
 * struct inode — minimal stub matching what vmwgfx needs.
 */
struct inode {
	struct address_space	*i_mapping;
	struct address_space	i_data;
	unsigned long		i_ino;
	umode_t			i_mode;
};

struct file_operations;		/* forward decl — defined below */

/*
 * struct file — Linux kernel file descriptor stub.
 * vmwgfx uses filp->private_data to get drm_file.
 */
struct file {
	void					*private_data;
	struct dentry				*f_dentry;
	unsigned int				 f_flags;
	loff_t					 f_pos;
	atomic_long_t				 f_count;
	struct address_space			*f_mapping;
	const struct file_operations		*f_op;
};

struct poll_table_struct;
struct vm_area_struct;

struct file_operations {
	void *owner;
	unsigned int fop_flags;
	int (*open)(struct inode *, struct file *);
	int (*release)(struct inode *, struct file *);
	long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
	long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
	int (*mmap)(struct file *, struct vm_area_struct *);
	unsigned int (*poll)(struct file *, struct poll_table_struct *);
	ssize_t (*read)(struct file *, char *, size_t, loff_t *);
	ssize_t (*write)(struct file *, const char *, size_t, loff_t *);
	loff_t (*llseek)(struct file *, loff_t, int);
	int (*flush)(struct file *, void *);
	int (*fsync)(struct file *, loff_t, loff_t, int);
};

struct dentry {
	struct inode		*d_inode;
};

#define DEFINE_SIMPLE_ATTRIBUTE(a, b, c, d)
#define MINORBITS	8

/* FOP_UNSIGNED_OFFSET: flag for file_operations.fop_flags (Linux 6.x) */
#define FOP_UNSIGNED_OFFSET	(1U << 0)

static inline loff_t
noop_llseek(struct file *file, loff_t offset, int whence)
{
	return file->f_pos;
}

/* file descriptor allocation stub */
static inline int
get_unused_fd_flags(unsigned flags)
{
	return -EMFILE;	/* illumos: fd allocation from kernel not supported */
}

static inline void
put_unused_fd(int fd)
{
}

static inline void
fd_install(int fd, struct file *file)
{
}

static inline struct file *
anon_inode_getfile(const char *name, const struct file_operations *fops,
    void *priv, int flags)
{
	return NULL;
}

static inline int
anon_inode_getfd(const char *name, const struct file_operations *fops,
    void *priv, int flags)
{
	return -ENOSYS;
}

static inline void
fput(struct file *f)
{
}

#endif
