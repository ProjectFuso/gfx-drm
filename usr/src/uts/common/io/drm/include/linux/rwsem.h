/* Public domain. */

#ifndef _LINUX_RWSEM_H
#define _LINUX_RWSEM_H

#include <sys/rwlock.h>
#include <linux/rwlock_types.h>		/* struct rw_semaphore { krwlock_t rw; } */

/* illumos: map Linux rwsem ops to krwlock_t via struct rw_semaphore wrapper */
#define down_read(rwl)			rw_enter(&(rwl)->rw, RW_READER)
#define down_read_trylock(rwl)		rw_tryenter(&(rwl)->rw, RW_READER)
#define down_write_trylock(rwl)		rw_tryenter(&(rwl)->rw, RW_WRITER)
#define up_read(rwl)			rw_exit(&(rwl)->rw)
#define down_write(rwl)			rw_enter(&(rwl)->rw, RW_WRITER)
#define down_write_nest_lock(rwl, x)	rw_enter(&(rwl)->rw, RW_WRITER)
#define up_write(rwl)			rw_exit(&(rwl)->rw)
#define downgrade_write(rwl)		rw_downgrade(&(rwl)->rw)

#define init_rwsem(rwl)			rw_init(&(rwl)->rw, NULL, RW_DEFAULT, NULL)
#define destroy_rwsem(rwl)		rw_destroy(&(rwl)->rw)
#define rwsem_is_locked(rwl)		RW_LOCK_HELD(&(rwl)->rw)

/* DECLARE_RWSEM: static instance; caller must call init_rwsem() before use */
#define DECLARE_RWSEM(rwl)		struct rw_semaphore rwl

/* no interface to check if another caller wants the lock */
#define rwsem_is_contended(rwl)		0

#endif
