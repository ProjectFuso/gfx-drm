/* Public domain. */

#ifndef _LINUX_FTRACE_H
#define _LINUX_FTRACE_H

#include <linux/kallsyms.h>
/* linux/interrupt.h excluded: pulls sys/taskq_impl.h->sys/user.h->sys/file.h,
 * which would conflict with our struct file in linux/fs.h */

#endif
