/* Public domain. */

#ifndef _LINUX_MM_TYPES_H
#define _LINUX_MM_TYPES_H

/* illumos: workqueue/completion/rwsem not needed for VM_FAULT_* macros */

#define VM_FAULT_NOPAGE		1
#define VM_FAULT_SIGBUS		2
#define VM_FAULT_RETRY		3
#define VM_FAULT_OOM		4

#endif
