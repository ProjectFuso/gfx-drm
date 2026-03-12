/*
 * Copyright (c) 2013 Jonathan Gray <jsg@openbsd.org>
 * Copyright (c) 2015, 2016 Mark Kettenis <kettenis@openbsd.org>
 * illumos port: see porting notes
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * illumos DRM Linux compat layer.
 * Provides Linux kernel API emulation for the modern DRM subsystem.
 *
 * Phase 1 stubs: kmap/vmap/page alloc, DMA-BUF, sync_file, ACPI, i2c.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/rwlock.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tsd.h>
#include <sys/taskq.h>
#include <sys/callout.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/pci.h>
#include <sys/atomic.h>
#include <sys/sysmacros.h>
#include <sys/varargs.h>
#include <sys/systm.h>
#include <sys/debug.h>

#include <linux/dma-buf.h>
#include <linux/mod_devicetable.h>
#include <linux/acpi.h>
#include <linux/pagevec.h>
#include <linux/dma-fence-array.h>
#include <linux/dma-fence-chain.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/idr.h>
#include <linux/scatterlist.h>
#include <linux/i2c.h>
#include <linux/pci.h>
#include <linux/notifier.h>
#include <linux/backlight.h>
#include <linux/shrinker.h>
#include <linux/fb.h>
#include <linux/xarray.h>
#include <linux/interval_tree.h>
#include <linux/kthread.h>
#include <linux/processor.h>
#include <linux/sync_file.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <linux/component.h>
#include <linux/iommu.h>

#include <drm/drm_device.h>
#include <drm/drm_connector.h>
#include <drm/drm_print.h>
#include <drm/drm_drv.h>

/* ===== Tasklets ===== */

void
tasklet_unlock_wait(struct tasklet_struct *ts)
{
	while (test_bit(TASKLET_STATE_RUN, &ts->state))
		cpu_relax();
}

void
tasklet_unlock_spin_wait(struct tasklet_struct *ts)
{
	while (test_bit(TASKLET_STATE_RUN, &ts->state))
		cpu_relax();
}

void
tasklet_run(void *arg)
{
	struct tasklet_struct *ts = arg;

	clear_bit(TASKLET_STATE_SCHED, &ts->state);
	if (tasklet_trylock(ts)) {
		if (!atomic_read(&ts->count)) {
			if (ts->use_callback)
				ts->callback(ts);
			else
				ts->func(ts->data);
		}
		tasklet_unlock(ts);
	}
}

/* ===== task_struct / current ===== */

/*
 * TSD key for per-thread struct task_struct.
 * Allocated in drm_linux_init(), freed in drm_linux_fini().
 */
uint_t drm_task_tsd_key;

static void
drm_task_tsd_destructor(void *data)
{
	if (data != NULL)
		kmem_free(data, sizeof(struct task_struct));
}

/*
 * drm_get_current — return a per-thread task_struct, lazily allocated.
 * group_leader is set to (struct task_struct *)curproc so pointer
 * comparisons across threads of the same process are stable.
 */
struct task_struct *
drm_get_current(void)
{
	struct task_struct *t;
	proc_t *p = curproc;

	t = tsd_get(drm_task_tsd_key);
	if (t == NULL) {
		t = kmem_zalloc(sizeof(*t), KM_SLEEP);
		(void) tsd_set(drm_task_tsd_key, t);
	}

	t->pid          = p->p_pid;
	t->flags        = (p->p_flag & SEXITING) ? PF_EXITING : 0;
	t->exit_code    = 0;
	t->group_leader = (struct task_struct *)p;
	(void) strlcpy(t->comm, PTOU(p)->u_comm, sizeof(t->comm));

	return t;
}

/* ===== Current-state / schedule ===== */

void
set_current_state(int state)
{
	/* illumos: no per-thread state bits; no-op */
}

void
__set_current_state(int state)
{
	/* illumos: no-op */
}

void
schedule(void)
{
	schedule_timeout(1);
}

long
schedule_timeout(long timeout)
{
	if (timeout == MAX_SCHEDULE_TIMEOUT) {
		delay(drv_usectohz(1000000)); /* yield 1 second */
		return (long)MAX_SCHEDULE_TIMEOUT;
	}
	if (timeout > 0)
		delay(timeout);
	return 0;
}

long
schedule_timeout_uninterruptible(long timeout)
{
	if (timeout > 0)
		delay(timeout);
	return 0;
}

int
wake_up_process(struct task_struct *t)
{
	/* cv_broadcast in wake_up() already handles waking; no-op here */
	(void)t;
	return 1;
}

int
autoremove_wake_function(struct wait_queue_entry *wqe, unsigned int mode,
    int sync, void *key)
{
	list_del_init(&wqe->entry);
	return 0;
}

int
woken_wake_function(struct wait_queue_entry *wqe, unsigned int mode,
    int sync, void *key)
{
	smp_mb();
	wqe->flags |= WQ_FLAG_WOKEN;
	list_del_init(&wqe->entry);
	return 0;
}

void
prepare_to_wait(wait_queue_head_t *wqh, wait_queue_entry_t *wqe, int state)
{
	mutex_enter(&wqh->lock);
	if (list_empty(&wqe->entry))
		__add_wait_queue(wqh, wqe);
	mutex_exit(&wqh->lock);
}

void
finish_wait(wait_queue_head_t *wqh, wait_queue_entry_t *wqe)
{
	mutex_enter(&wqh->lock);
	if (!list_empty(&wqe->entry))
		list_del_init(&wqe->entry);
	mutex_exit(&wqh->lock);
}

/* ===== Workqueue flush ===== */

void
flush_workqueue(struct workqueue_struct *wq)
{
	if (wq)
		taskq_wait((taskq_t *)wq);
}

bool
flush_work(struct work_struct *work)
{
	if (work->tq)
		taskq_wait(work->tq);
	return false;
}

bool
flush_delayed_work(struct delayed_work *dwork)
{
	bool ret = false;

	while (callout_active(&dwork->to)) {
		delay(drv_usectohz(1000));
		ret = true;
	}

	if (dwork->tq)
		taskq_wait(dwork->tq);
	return ret;
}

/* ===== Kthreads ===== */

/*
 * Internal kthread wrapper.  We maintain a list so kthread_stop() can
 * look up the wrapper from the kthread_t handle.
 */
struct kthread_wrapper {
	int		(*func)(void *);
	void		*data;
	kthread_t	*thread;
	kmutex_t	lock;
	kcondvar_t	cv;
	volatile uint32_t flags;
#define KTW_SHOULDSTOP	0x01
#define KTW_STOPPED	0x02
#define KTW_SHOULDPARK	0x04
#define KTW_PARKED	0x08
	struct kthread_wrapper *next;
};

static kmutex_t kthread_list_lock;
static struct kthread_wrapper *kthread_list_head;

static struct kthread_wrapper *
kthread_wrapper_lookup(kthread_t *t)
{
	struct kthread_wrapper *kw;

	mutex_enter(&kthread_list_lock);
	for (kw = kthread_list_head; kw != NULL; kw = kw->next) {
		if (kw->thread == t)
			break;
	}
	mutex_exit(&kthread_list_lock);
	return kw;
}

static void
kthread_wrapper_func(void *arg)
{
	struct kthread_wrapper *kw = arg;
	kw->func(kw->data);

	mutex_enter(&kw->lock);
	kw->flags |= KTW_STOPPED;
	cv_broadcast(&kw->cv);
	mutex_exit(&kw->lock);
	thread_exit();
}

kthread_t *
kthread_run(void (*func)(void *), void *data, const char *name)
{
	struct kthread_wrapper *kw;

	kw = kmem_zalloc(sizeof(*kw), KM_SLEEP);
	kw->func = (int (*)(void *))func;
	kw->data = data;
	kw->flags = 0;
	mutex_init(&kw->lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&kw->cv, NULL, CV_DRIVER, NULL);

	kw->thread = thread_create(NULL, 0, kthread_wrapper_func, kw, 0,
	    &p0, TS_RUN, minclsyspri);
	if (kw->thread == NULL) {
		cv_destroy(&kw->cv);
		mutex_destroy(&kw->lock);
		kmem_free(kw, sizeof(*kw));
		return (kthread_t *)ERR_PTR(-ENOMEM);
	}

	mutex_enter(&kthread_list_lock);
	kw->next = kthread_list_head;
	kthread_list_head = kw;
	mutex_exit(&kthread_list_lock);

	return kw->thread;
}

void
kthread_stop(kthread_t *t)
{
	struct kthread_wrapper *kw = kthread_wrapper_lookup(t);
	struct kthread_wrapper **pp;

	if (kw == NULL)
		return;

	mutex_enter(&kw->lock);
	kw->flags |= KTW_SHOULDSTOP;
	kw->flags &= ~KTW_SHOULDPARK;
	cv_broadcast(&kw->cv);
	while ((kw->flags & KTW_STOPPED) == 0)
		cv_wait(&kw->cv, &kw->lock);
	mutex_exit(&kw->lock);

	/* Remove from list */
	mutex_enter(&kthread_list_lock);
	for (pp = &kthread_list_head; *pp != NULL; pp = &(*pp)->next) {
		if (*pp == kw) {
			*pp = kw->next;
			break;
		}
	}
	mutex_exit(&kthread_list_lock);

	cv_destroy(&kw->cv);
	mutex_destroy(&kw->lock);
	kmem_free(kw, sizeof(*kw));
}

int
kthread_should_stop(void)
{
	struct kthread_wrapper *kw = kthread_wrapper_lookup(curthread);
	if (kw == NULL)
		return 0;
	return (kw->flags & KTW_SHOULDSTOP) != 0;
}

int
kthread_should_park(void)
{
	struct kthread_wrapper *kw = kthread_wrapper_lookup(curthread);
	if (kw == NULL)
		return 0;
	return (kw->flags & KTW_SHOULDPARK) != 0;
}

void
kthread_parkme(void)
{
	struct kthread_wrapper *kw = kthread_wrapper_lookup(curthread);
	if (kw == NULL)
		return;

	mutex_enter(&kw->lock);
	while (kw->flags & KTW_SHOULDPARK) {
		kw->flags |= KTW_PARKED;
		cv_broadcast(&kw->cv);
		cv_wait(&kw->cv, &kw->lock);
		kw->flags &= ~KTW_PARKED;
	}
	mutex_exit(&kw->lock);
}

void
kthread_park(kthread_t *t)
{
	struct kthread_wrapper *kw = kthread_wrapper_lookup(t);
	if (kw == NULL)
		return;

	mutex_enter(&kw->lock);
	kw->flags |= KTW_SHOULDPARK;
	cv_broadcast(&kw->cv);
	while ((kw->flags & KTW_PARKED) == 0)
		cv_wait(&kw->cv, &kw->lock);
	mutex_exit(&kw->lock);
}

void
kthread_unpark(kthread_t *t)
{
	struct kthread_wrapper *kw = kthread_wrapper_lookup(t);
	if (kw == NULL)
		return;

	mutex_enter(&kw->lock);
	kw->flags &= ~KTW_SHOULDPARK;
	cv_broadcast(&kw->cv);
	mutex_exit(&kw->lock);
}

/* ===== kthread_worker ===== */

struct kthread_worker *
kthread_create_worker(unsigned int flags, const char *fmt, ...)
{
	char name[64];
	va_list ap;
	struct kthread_worker *w;

	w = kmalloc(sizeof(*w), GFP_KERNEL);
	if (w == NULL)
		return NULL;
	va_start(ap, fmt);
	(void) vsnprintf(name, sizeof(name), fmt, ap);
	va_end(ap);
	w->tq = taskq_create(name, 1, minclsyspri, 1, INT_MAX,
	    TASKQ_PREPOPULATE);
	return w;
}

void
kthread_destroy_worker(struct kthread_worker *worker)
{
	taskq_destroy(worker->tq);
	kfree(worker);
}

void
kthread_init_work(struct kthread_work *work,
    void (*func)(struct kthread_work *))
{
	work->tq = NULL;
	work->func = func;
}

bool
kthread_queue_work(struct kthread_worker *worker, struct kthread_work *work)
{
	void (*fn)(struct kthread_work *) = work->func;
	work->tq = worker->tq;
	work->id = taskq_dispatch(work->tq, (void (*)(void *))fn, work,
	    TQ_SLEEP);
	return work->id != TASKQID_INVALID;
}

bool
kthread_cancel_work_sync(struct kthread_work *work)
{
	if (work->tq != NULL && work->id != TASKQID_INVALID) {
		taskq_cancel_id(work->tq, work->id);
		work->id = TASKQID_INVALID;
		return true;
	}
	return false;
}

void
kthread_flush_work(struct kthread_work *work)
{
	if (work->tq)
		taskq_wait(work->tq);
}

void
kthread_flush_worker(struct kthread_worker *worker)
{
	if (worker->tq)
		taskq_wait(worker->tq);
}

/* ===== DMI / SMBIOS ===== */

/*
 * Phase 1: DMI matching is stubbed.  Return false for all slots.
 * A future phase can wire this to illumos SMBIOS via smbios(7D).
 */
bool
dmi_match(int slot, const char *str)
{
	return false;
}

static bool
dmi_found(const struct dmi_system_id *dsi)
{
	int i, slot;

	for (i = 0; i < ARRAY_SIZE(dsi->matches); i++) {
		slot = dsi->matches[i].slot;
		if (slot == DMI_NONE)
			break;
		if (!dmi_match(slot, dsi->matches[i].substr))
			return false;
	}
	return true;
}

const struct dmi_system_id *
dmi_first_match(const struct dmi_system_id *sysid)
{
	const struct dmi_system_id *dsi;

	for (dsi = sysid; dsi->matches[0].slot != 0; dsi++) {
		if (dmi_found(dsi))
			return dsi;
	}
	return NULL;
}

const char *
dmi_get_system_info(int slot)
{
	return NULL;
}

int
dmi_check_system(const struct dmi_system_id *sysid)
{
	const struct dmi_system_id *dsi;
	int num = 0;

	for (dsi = sysid; dsi->matches[0].slot != 0; dsi++) {
		if (dmi_found(dsi)) {
			num++;
			if (dsi->callback && dsi->callback(dsi))
				break;
		}
	}
	return num;
}

/* ===== Page allocation / kmap (Phase 1 stubs) ===== */

/*
 * illumos Phase 1: alloc_pages/kmap/vmap are stubs.
 * Full implementation requires hat_kpm_mapin / vmem_xalloc + hat_devload.
 */

struct page *
alloc_pages(unsigned int gfp_mask, unsigned int order)
{
	/* Phase 1: not implemented */
	return NULL;
}

void
__free_pages(struct page *page, unsigned int order)
{
	/* Phase 1: not implemented */
}

void
__pagevec_release(struct pagevec *pvec)
{
	pagevec_reinit(pvec);
}

void *
kmap(struct page *pg)
{
	/* Phase 1: not implemented */
	return NULL;
}

void
kunmap_va(void *addr)
{
	/* Phase 1: not implemented */
}

void *
kmap_atomic_prot(struct page *pg, pgprot_t prot)
{
	/* Phase 1: not implemented */
	return NULL;
}

void
kunmap_atomic(void *addr)
{
	/* Phase 1: not implemented */
}

void *
vmap(struct page **pages, unsigned int npages, unsigned long flags,
    pgprot_t prot)
{
	/* Phase 1: not implemented */
	return NULL;
}

void *
vmap_pfn(unsigned long *pfns, unsigned int npfn, pgprot_t prot)
{
	/* Phase 1: not implemented */
	return NULL;
}

void
vunmap(void *addr, size_t size)
{
	/* Phase 1: not implemented */
}

bool
is_vmalloc_addr(const void *p)
{
	/* Conservative: assume yes; drivers use this to decide mapping */
	return true;
}

/* ===== print_hex_dump / memchr_inv ===== */

void
print_hex_dump(const char *level, const char *prefix_str, int prefix_type,
    int rowsize, int groupsize, const void *buf, size_t len, bool ascii)
{
	const uint8_t *cbuf = buf;
	int i;

	for (i = 0; i < len; i++) {
		if ((i % rowsize) == 0)
			cmn_err(CE_CONT, "%s", prefix_str);
		cmn_err(CE_CONT, "%02x", cbuf[i]);
		if ((i % rowsize) == (rowsize - 1))
			cmn_err(CE_CONT, "\n");
		else
			cmn_err(CE_CONT, " ");
	}
}

void *
memchr_inv(const void *s, int c, size_t n)
{
	if (n != 0) {
		const unsigned char *p = s;
		do {
			if (*p++ != (unsigned char)c)
				return ((void *)(p - 1));
		} while (--n != 0);
	}
	return NULL;
}

/* ===== RB tree (linux_root) ===== */

int
panic_cmp(struct rb_node *a, struct rb_node *b)
{
	panic(__func__);
	return 0;
}

#undef RB_ROOT
#define RB_ROOT(head)	(head)->rbh_root

RB_GENERATE(linux_root, rb_node, __entry, panic_cmp);

/* ===== IDR (splay tree, kmem_cache-backed) ===== */

/*
 * A fairly minimal implementation of the Linux "idr" API.
 * Uses a SPLAY tree for O(log n) ops.
 */

int idr_cmp(struct idr_entry *, struct idr_entry *);
SPLAY_PROTOTYPE(idr_tree, idr_entry, entry, idr_cmp);

static kmem_cache_t *idr_cache;
static struct idr_entry *idr_entry_cache; /* pre-loaded entry */

void
idr_init(struct idr *idr)
{
	SPLAY_INIT(&idr->tree);
}

void
idr_destroy(struct idr *idr)
{
	struct idr_entry *id;

	while ((id = SPLAY_MIN(idr_tree, &idr->tree))) {
		SPLAY_REMOVE(idr_tree, &idr->tree, id);
		kmem_cache_free(idr_cache, id);
	}
}

void
idr_preload(unsigned int gfp_mask)
{
	int flags = (gfp_mask & GFP_NOWAIT) ? KM_NOSLEEP : KM_SLEEP;

	if (idr_entry_cache == NULL)
		idr_entry_cache = kmem_cache_alloc(idr_cache, flags);
}

/* [start, end) */
int
idr_alloc(struct idr *idr, void *ptr, int start, int end, gfp_t gfp_mask)
{
	int flags = (gfp_mask & GFP_NOWAIT) ? KM_NOSLEEP : KM_SLEEP;
	struct idr_entry *id;
	int begin;

	if (idr_entry_cache) {
		id = idr_entry_cache;
		idr_entry_cache = NULL;
	} else {
		id = kmem_cache_alloc(idr_cache, flags);
		if (id == NULL)
			return -ENOMEM;
	}

	if (end <= 0)
		end = INT_MAX;

	id->id = begin = start;
	while (SPLAY_INSERT(idr_tree, &idr->tree, id)) {
		if (id->id == end)
			id->id = start;
		else
			id->id++;
		if (id->id == begin) {
			kmem_cache_free(idr_cache, id);
			return -ENOSPC;
		}
	}
	id->ptr = ptr;
	return id->id;
}

void *
idr_replace(struct idr *idr, void *ptr, unsigned long id)
{
	struct idr_entry find, *res;
	void *old;

	find.id = id;
	res = SPLAY_FIND(idr_tree, &idr->tree, &find);
	if (res == NULL)
		return ERR_PTR(-ENOENT);
	old = res->ptr;
	res->ptr = ptr;
	return old;
}

void *
idr_remove(struct idr *idr, unsigned long id)
{
	struct idr_entry find, *res;
	void *ptr = NULL;

	find.id = id;
	res = SPLAY_FIND(idr_tree, &idr->tree, &find);
	if (res) {
		SPLAY_REMOVE(idr_tree, &idr->tree, res);
		ptr = res->ptr;
		kmem_cache_free(idr_cache, res);
	}
	return ptr;
}

void *
idr_find(struct idr *idr, unsigned long id)
{
	struct idr_entry find, *res;

	find.id = id;
	res = SPLAY_FIND(idr_tree, &idr->tree, &find);
	if (res == NULL)
		return NULL;
	return res->ptr;
}

void *
idr_get_next(struct idr *idr, int *id)
{
	struct idr_entry *res;

	SPLAY_FOREACH(res, idr_tree, &idr->tree) {
		if (res->id >= *id) {
			*id = res->id;
			return res->ptr;
		}
	}
	return NULL;
}

int
idr_cmp(struct idr_entry *a, struct idr_entry *b)
{
	return (a->id < b->id) ? -1 : (a->id > b->id) ? 1 : 0;
}

SPLAY_GENERATE(idr_tree, idr_entry, entry, idr_cmp);

/* ===== XArray (splay tree, kmem_cache-backed) ===== */

int xa_cmp(struct xarray_entry *, struct xarray_entry *);
SPLAY_PROTOTYPE(xarray_tree, xarray_entry, entry, xa_cmp);

static kmem_cache_t *xa_cache;

void
xa_init_flags(struct xarray *xa, gfp_t flags)
{
	xa->xa_flags = flags;
	mutex_init(&xa->xa_lock, NULL, MUTEX_DRIVER, NULL);
	SPLAY_INIT(&xa->xa_tree);
}

void
xa_destroy(struct xarray *xa)
{
	struct xarray_entry *res;

	while ((res = SPLAY_MIN(xarray_tree, &xa->xa_tree))) {
		SPLAY_REMOVE(xarray_tree, &xa->xa_tree, res);
		kmem_cache_free(xa_cache, res);
	}
	mutex_destroy(&xa->xa_lock);
}

int
__xa_alloc(struct xarray *xa, u32 *id, void *entry, struct xarray_range r,
    gfp_t gfp)
{
	struct xarray_entry *res;
	u32 begin;

	ASSERT(MUTEX_HELD(&xa->xa_lock));

	res = kmem_cache_alloc(xa_cache,
	    (gfp & GFP_NOWAIT) ? KM_NOSLEEP : KM_SLEEP);
	if (res == NULL)
		return -ENOMEM;

	res->id = begin = r.start;
	while (SPLAY_INSERT(xarray_tree, &xa->xa_tree, res)) {
		if (res->id == r.end)
			res->id = r.start;
		else
			res->id++;
		if (res->id == begin) {
			kmem_cache_free(xa_cache, res);
			return -ENOSPC;
		}
	}
	res->ptr = entry;
	*id = res->id;
	return 0;
}

int
__xa_alloc_cyclic(struct xarray *xa, u32 *id, void *entry,
    struct xarray_range r, u32 *next, gfp_t gfp)
{
	int ret;

	/* Simple: ignore cyclic hint for Phase 1 */
	ret = __xa_alloc(xa, id, entry, r, gfp);
	if (ret == 0 && next != NULL)
		*next = *id + 1;
	return ret;
}

void *
__xa_erase(struct xarray *xa, unsigned long index)
{
	struct xarray_entry find, *res;
	void *ptr = NULL;

	ASSERT(MUTEX_HELD(&xa->xa_lock));

	find.id = index;
	res = SPLAY_FIND(xarray_tree, &xa->xa_tree, &find);
	if (res) {
		SPLAY_REMOVE(xarray_tree, &xa->xa_tree, res);
		ptr = res->ptr;
		kmem_cache_free(xa_cache, res);
	}
	return ptr;
}

void *
__xa_load(struct xarray *xa, unsigned long index)
{
	struct xarray_entry find, *res;

	find.id = index;
	res = SPLAY_FIND(xarray_tree, &xa->xa_tree, &find);
	if (res == NULL)
		return NULL;
	return res->ptr;
}

void *
__xa_store(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp)
{
	struct xarray_entry find, *res;
	void *prev;

	ASSERT(MUTEX_HELD(&xa->xa_lock));

	if (entry == NULL)
		return __xa_erase(xa, index);

	find.id = index;
	res = SPLAY_FIND(xarray_tree, &xa->xa_tree, &find);
	if (res != NULL) {
		prev = res->ptr;
		res->ptr = entry;
		return prev;
	}

	res = kmem_cache_alloc(xa_cache,
	    (gfp & GFP_NOWAIT) ? KM_NOSLEEP : KM_SLEEP);
	if (res == NULL)
		return XA_ERROR(-ENOMEM);
	res->id = index;
	res->ptr = entry;
	if (SPLAY_INSERT(xarray_tree, &xa->xa_tree, res) != NULL)
		return XA_ERROR(-EINVAL);
	return NULL;
}

void *
xa_get_next(struct xarray *xa, unsigned long *index)
{
	struct xarray_entry *res;

	SPLAY_FOREACH(res, xarray_tree, &xa->xa_tree) {
		if (res->id >= *index) {
			*index = res->id;
			return res->ptr;
		}
	}
	return NULL;
}

int
xa_cmp(struct xarray_entry *a, struct xarray_entry *b)
{
	return (a->id < b->id) ? -1 : (a->id > b->id) ? 1 : 0;
}

SPLAY_GENERATE(xarray_tree, xarray_entry, entry, xa_cmp);

/* ===== sg_alloc_table / sg_free_table ===== */

int
sg_alloc_table(struct sg_table *table, unsigned int nents, gfp_t gfp_mask)
{
	table->sgl = kcalloc(nents, sizeof(struct scatterlist), gfp_mask);
	if (table->sgl == NULL)
		return -ENOMEM;
	table->nents = table->orig_nents = nents;
	sg_mark_end(&table->sgl[nents - 1]);
	return 0;
}

void
sg_free_table(struct sg_table *table)
{
	kfree(table->sgl);
	table->orig_nents = 0;
	table->sgl = NULL;
}

/* ===== I2C (Phase 1 stubs) ===== */

/*
 * Phase 1: I2C implementation uses OpenBSD iic_exec which doesn't exist on
 * illumos.  Stub everything to return an error.  A future phase will wire
 * to the illumos I2C DDI.
 */

int
i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	if (adap->algo && adap->algo->master_xfer)
		return adap->algo->master_xfer(adap, msgs, num);
	return -ENOTSUP;
}

int
__i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	return i2c_transfer(adap, msgs, num);
}

int
i2c_bit_add_bus(struct i2c_adapter *adap)
{
	adap->retries = 3;
	return 0;
}

/* ===== VGA bridge (Phase 1 stubs) ===== */

void
vga_get_uninterruptible(struct pci_dev *pdev, int rsrc)
{
	/* Phase 1: stub */
}

void
vga_put(struct pci_dev *pdev, int rsrc)
{
	/* Phase 1: stub */
}

/* ===== PM / suspend ===== */

suspend_state_t pm_suspend_target_state;

/* ===== ACPI (Phase 1 stubs) ===== */

acpi_status
acpi_get_table(const char *sig, int instance, struct acpi_table_header **hdr)
{
	return AE_NOT_FOUND;
}

void
acpi_put_table(struct acpi_table_header *hdr)
{
}

acpi_status
acpi_get_handle(acpi_handle node, const char *name, acpi_handle *rnode)
{
	return AE_NOT_FOUND;
}

acpi_status
acpi_get_name(acpi_handle node, int type, struct acpi_buffer *buffer)
{
	return AE_NOT_FOUND;
}

acpi_status
acpi_evaluate_object(acpi_handle node, const char *name,
    struct acpi_object_list *params, struct acpi_buffer *result)
{
	return AE_NOT_FOUND;
}

static SLIST_HEAD(drm_linux_acpi_notify_list, notifier_block)
    drm_linux_acpi_notify_list_head =
    SLIST_HEAD_INITIALIZER(drm_linux_acpi_notify_list_head);

int
register_acpi_notifier(struct notifier_block *nb)
{
	SLIST_INSERT_HEAD(&drm_linux_acpi_notify_list_head, nb, link);
	return 0;
}

int
unregister_acpi_notifier(struct notifier_block *nb)
{
	struct notifier_block *tmp;

	SLIST_FOREACH(tmp, &drm_linux_acpi_notify_list_head, link) {
		if (tmp == nb) {
			SLIST_REMOVE(&drm_linux_acpi_notify_list_head, nb,
			    notifier_block, link);
			return 0;
		}
	}
	return -ENOENT;
}

const char *
acpi_format_exception(acpi_status status)
{
	switch (status) {
	case AE_NOT_FOUND:    return "not found";
	case AE_BAD_PARAMETER: return "bad parameter";
	default:              return "unknown";
	}
}

int
acpi_target_system_state(void)
{
	return 0;
}

enum acpi_backlight_type
acpi_video_get_backlight_type(void)
{
	return acpi_backlight_native;
}

/* ===== Backlight (Phase 1 stubs) ===== */

static SLIST_HEAD(backlight_list, backlight_device)
    backlight_device_list_head = SLIST_HEAD_INITIALIZER(backlight_device_list_head);

struct backlight_device *
backlight_device_register(const char *name, void *kdev, void *data,
    const struct backlight_ops *ops, const struct backlight_properties *props)
{
	struct backlight_device *bd;

	bd = kzalloc(sizeof(*bd), GFP_KERNEL);
	if (bd == NULL)
		return NULL;
	bd->ops = ops;
	bd->props = *props;
	bd->data = data;
	bd->name = name;
	SLIST_INSERT_HEAD(&backlight_device_list_head, bd, next);
	return bd;
}

void
backlight_device_unregister(struct backlight_device *bd)
{
	SLIST_REMOVE(&backlight_device_list_head, bd, backlight_device, next);
	kfree(bd);
}

void
backlight_schedule_update_status(struct backlight_device *bd)
{
	backlight_update_status(bd);
}

int
backlight_enable(struct backlight_device *bd)
{
	if (bd == NULL)
		return 0;
	bd->props.power = BACKLIGHT_POWER_ON;
	return bd->ops->update_status(bd);
}

int
backlight_disable(struct backlight_device *bd)
{
	if (bd == NULL)
		return 0;
	bd->props.power = BACKLIGHT_POWER_OFF;
	return bd->ops->update_status(bd);
}

struct backlight_device *
backlight_device_get_by_name(const char *name)
{
	struct backlight_device *bd;

	SLIST_FOREACH(bd, &backlight_device_list_head, next) {
		if (strcmp(name, bd->name) == 0)
			return bd;
	}
	return NULL;
}

/* ===== dev_set/get_drvdata ===== */

struct drvdata_entry {
	struct device *dev;
	void *data;
	LIST_ENTRY(drvdata_entry) next;
};

static LIST_HEAD(drvdata_list, drvdata_entry)
    drvdata_list_head = LIST_HEAD_INITIALIZER(drvdata_list_head);
static kmutex_t drvdata_lock;

void
dev_set_drvdata(struct device *dev, void *data)
{
	struct drvdata_entry *de;

	mutex_enter(&drvdata_lock);
	LIST_FOREACH(de, &drvdata_list_head, next) {
		if (de->dev == dev) {
			de->data = data;
			mutex_exit(&drvdata_lock);
			return;
		}
	}

	if (data == NULL) {
		mutex_exit(&drvdata_lock);
		return;
	}

	de = kmem_alloc(sizeof(*de), KM_SLEEP);
	de->dev = dev;
	de->data = data;
	LIST_INSERT_HEAD(&drvdata_list_head, de, next);
	mutex_exit(&drvdata_lock);
}

void *
dev_get_drvdata(struct device *dev)
{
	struct drvdata_entry *de;

	mutex_enter(&drvdata_lock);
	LIST_FOREACH(de, &drvdata_list_head, next) {
		if (de->dev == dev) {
			mutex_exit(&drvdata_lock);
			return de->data;
		}
	}
	mutex_exit(&drvdata_lock);
	return NULL;
}

/* ===== DMA fence ===== */

struct dma_fence *
dma_fence_get(struct dma_fence *fence)
{
	if (fence)
		kref_get(&fence->refcount);
	return fence;
}

struct dma_fence *
dma_fence_get_rcu(struct dma_fence *fence)
{
	if (fence)
		kref_get(&fence->refcount);
	return fence;
}

struct dma_fence *
dma_fence_get_rcu_safe(struct dma_fence **dfp)
{
	struct dma_fence *fence;
	if (dfp == NULL)
		return NULL;
	fence = *dfp;
	if (fence)
		kref_get(&fence->refcount);
	return fence;
}

void
dma_fence_release(struct kref *ref)
{
	struct dma_fence *fence = container_of(ref, struct dma_fence, refcount);
	if (fence->ops && fence->ops->release)
		fence->ops->release(fence);
	else
		kfree(fence);
}

void
dma_fence_put(struct dma_fence *fence)
{
	if (fence)
		kref_put(&fence->refcount, dma_fence_release);
}

int
dma_fence_signal_timestamp_locked(struct dma_fence *fence, ktime_t timestamp)
{
	struct dma_fence_cb *cur, *tmp;
	struct list_head cb_list;

	if (fence == NULL)
		return -EINVAL;

	if (test_and_set_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return -EINVAL;

	list_replace(&fence->cb_list, &cb_list);

	fence->timestamp = timestamp;
	set_bit(DMA_FENCE_FLAG_TIMESTAMP_BIT, &fence->flags);

	list_for_each_entry_safe(cur, tmp, &cb_list, node) {
		INIT_LIST_HEAD(&cur->node);
		cur->func(fence, cur);
	}

	return 0;
}

int
dma_fence_signal(struct dma_fence *fence)
{
	int r;

	if (fence == NULL)
		return -EINVAL;

	mutex_enter(fence->lock);
	r = dma_fence_signal_timestamp_locked(fence, ktime_get());
	mutex_exit(fence->lock);

	return r;
}

int
dma_fence_signal_locked(struct dma_fence *fence)
{
	if (fence == NULL)
		return -EINVAL;

	return dma_fence_signal_timestamp_locked(fence, ktime_get());
}

int
dma_fence_signal_timestamp(struct dma_fence *fence, ktime_t timestamp)
{
	int r;

	if (fence == NULL)
		return -EINVAL;

	mutex_enter(fence->lock);
	r = dma_fence_signal_timestamp_locked(fence, timestamp);
	mutex_exit(fence->lock);

	return r;
}

bool
dma_fence_is_signaled(struct dma_fence *fence)
{
	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return true;

	if (fence->ops->signaled && fence->ops->signaled(fence)) {
		dma_fence_signal(fence);
		return true;
	}

	return false;
}

bool
dma_fence_is_signaled_locked(struct dma_fence *fence)
{
	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return true;

	if (fence->ops->signaled && fence->ops->signaled(fence)) {
		dma_fence_signal_locked(fence);
		return true;
	}

	return false;
}

ktime_t
dma_fence_timestamp(struct dma_fence *fence)
{
	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		while (!test_bit(DMA_FENCE_FLAG_TIMESTAMP_BIT, &fence->flags))
			CPU_BUSY_CYCLE();
		return fence->timestamp;
	} else {
		return ktime_get();
	}
}

long
dma_fence_wait_timeout(struct dma_fence *fence, bool intr, long timeout)
{
	if (timeout < 0)
		return -EINVAL;

	if (fence->ops->wait)
		return fence->ops->wait(fence, intr, timeout);
	else
		return dma_fence_default_wait(fence, intr, timeout);
}

long
dma_fence_wait(struct dma_fence *fence, bool intr)
{
	long ret;

	ret = dma_fence_wait_timeout(fence, intr, MAX_SCHEDULE_TIMEOUT);
	if (ret < 0)
		return ret;

	return 0;
}

void
dma_fence_enable_sw_signaling(struct dma_fence *fence)
{
	if (!test_and_set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags) &&
	    !test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags) &&
	    fence->ops->enable_signaling) {
		mutex_enter(fence->lock);
		if (!fence->ops->enable_signaling(fence))
			dma_fence_signal_locked(fence);
		mutex_exit(fence->lock);
	}
}

void
dma_fence_init(struct dma_fence *fence, const struct dma_fence_ops *ops,
    spinlock_t *lock, uint64_t context, uint64_t seqno)
{
	fence->ops = ops;
	fence->lock = lock;
	fence->context = context;
	fence->seqno = seqno;
	fence->flags = 0;
	fence->error = 0;
	kref_init(&fence->refcount);
	INIT_LIST_HEAD(&fence->cb_list);
}

int
dma_fence_add_callback(struct dma_fence *fence, struct dma_fence_cb *cb,
    dma_fence_func_t func)
{
	int ret = 0;
	bool was_set;

	if (WARN_ON(!fence || !func))
		return -EINVAL;

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		INIT_LIST_HEAD(&cb->node);
		return -ENOENT;
	}

	mutex_enter(fence->lock);

	was_set = test_and_set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
	    &fence->flags);

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		ret = -ENOENT;
	else if (!was_set && fence->ops->enable_signaling) {
		if (!fence->ops->enable_signaling(fence)) {
			dma_fence_signal_locked(fence);
			ret = -ENOENT;
		}
	}

	if (!ret) {
		cb->func = func;
		list_add_tail(&cb->node, &fence->cb_list);
	} else
		INIT_LIST_HEAD(&cb->node);
	mutex_exit(fence->lock);

	return ret;
}

bool
dma_fence_remove_callback(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	bool ret;

	mutex_enter(fence->lock);

	ret = !list_empty(&cb->node);
	if (ret)
		list_del_init(&cb->node);

	mutex_exit(fence->lock);

	return ret;
}

static atomic64_t drm_fence_context_count = ATOMIC64_INIT(1);

uint64_t
dma_fence_context_alloc(unsigned int num)
{
	return atomic64_add_return(num, &drm_fence_context_count) - num;
}

/*
 * dma_fence_default_wait: wait for a fence to signal.
 *
 * We use a per-wait-cb cv+mutex rather than per-proc sleep so we don't
 * need per-thread sleep channels.  The callback sets signaled=true and
 * broadcasts on the cv.
 */
struct default_wait_cb {
	struct dma_fence_cb	base;
	kmutex_t		mtx;
	kcondvar_t		cv;
	bool			signaled;
};

static void
dma_fence_default_wait_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct default_wait_cb *wait =
	    container_of(cb, struct default_wait_cb, base);

	mutex_enter(&wait->mtx);
	wait->signaled = true;
	cv_broadcast(&wait->cv);
	mutex_exit(&wait->mtx);
}

long
dma_fence_default_wait(struct dma_fence *fence, bool intr, signed long timeout)
{
	struct default_wait_cb cb;
	long ret = timeout ? timeout : 1;
	bool was_set;
	hrtime_t deadline;
	int cv_ret;

	ASSERT(timeout <= INT_MAX);

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return ret;

	mutex_init(&cb.mtx, NULL, MUTEX_DRIVER, NULL);
	cv_init(&cb.cv, NULL, CV_DRIVER, NULL);
	cb.signaled = false;

	mutex_enter(fence->lock);

	was_set = test_and_set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
	    &fence->flags);

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		goto out_locked;

	if (!was_set && fence->ops->enable_signaling) {
		if (!fence->ops->enable_signaling(fence)) {
			dma_fence_signal_locked(fence);
			goto out_locked;
		}
	}

	if (timeout == 0) {
		ret = 0;
		goto out_locked;
	}

	cb.base.func = dma_fence_default_wait_cb;
	list_add(&cb.base.node, &fence->cb_list);
	mutex_exit(fence->lock);

	if (timeout != MAX_SCHEDULE_TIMEOUT) {
		/* Convert jiffies to hrtime deadline */
		deadline = gethrtime() +
		    (hrtime_t)drv_hztousec(timeout) * (hrtime_t)NANOSEC /
		    (hrtime_t)1000000;
	}

	mutex_enter(&cb.mtx);
	if (timeout == MAX_SCHEDULE_TIMEOUT) {
		while (!cb.signaled) {
			if (intr) {
				cv_ret = cv_wait_sig(&cb.cv, &cb.mtx);
				if (cv_ret == 0) {
					ret = -ERESTARTSYS;
					break;
				}
			} else {
				cv_wait(&cb.cv, &cb.mtx);
			}
		}
	} else {
		while (!cb.signaled && ret > 0) {
			if (intr)
				cv_ret = cv_timedwait_sig(&cb.cv, &cb.mtx,
				    ddi_get_lbolt() + ret);
			else
				cv_ret = cv_timedwait(&cb.cv, &cb.mtx,
				    ddi_get_lbolt() + ret);
			if (cv_ret == -1) {
				ret = 0;
				break;
			}
			if (cv_ret == 0 && intr) {
				ret = -ERESTARTSYS;
				break;
			}
		}
		if (cb.signaled && ret >= 0)
			ret = MAX(ret, 1L);
	}
	mutex_exit(&cb.mtx);

	dma_fence_remove_callback(fence, &cb.base);
	goto out;

out_locked:
	mutex_exit(fence->lock);
out:
	cv_destroy(&cb.cv);
	mutex_destroy(&cb.mtx);
	return ret;
}

/*
 * dma_fence_wait_any_timeout: wait for any one of N fences to signal.
 */
struct wait_any_shared {
	kmutex_t	mtx;
	kcondvar_t	cv;
	bool		done;
};

struct wait_any_cb {
	struct dma_fence_cb	cb;
	struct wait_any_shared	*shared;
};

static void
dma_fence_wait_any_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct wait_any_cb *w = container_of(cb, struct wait_any_cb, cb);

	mutex_enter(&w->shared->mtx);
	w->shared->done = true;
	cv_broadcast(&w->shared->cv);
	mutex_exit(&w->shared->mtx);
}

static bool
dma_fence_test_signaled_any(struct dma_fence **fences, uint32_t count,
    uint32_t *idx)
{
	uint32_t i;

	for (i = 0; i < count; ++i) {
		if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fences[i]->flags)) {
			if (idx)
				*idx = i;
			return true;
		}
	}
	return false;
}

long
dma_fence_wait_any_timeout(struct dma_fence **fences, uint32_t count,
    bool intr, long timeout, uint32_t *idx)
{
	struct wait_any_shared shared;
	struct wait_any_cb *cb;
	long ret = timeout;
	uint32_t i;
	int cv_ret;

	ASSERT(timeout <= INT_MAX);

	if (timeout == 0) {
		for (i = 0; i < count; i++) {
			if (dma_fence_is_signaled(fences[i])) {
				if (idx)
					*idx = i;
				return 1;
			}
		}
		return 0;
	}

	cb = kcalloc(count, sizeof(*cb), GFP_KERNEL);
	if (cb == NULL)
		return -ENOMEM;

	mutex_init(&shared.mtx, NULL, MUTEX_DRIVER, NULL);
	cv_init(&shared.cv, NULL, CV_DRIVER, NULL);
	shared.done = false;

	for (i = 0; i < count; i++) {
		cb[i].shared = &shared;
		if (dma_fence_add_callback(fences[i], &cb[i].cb,
		    dma_fence_wait_any_cb)) {
			if (idx)
				*idx = i;
			ret = 1; /* already signaled */
			goto cb_cleanup;
		}
	}

	mutex_enter(&shared.mtx);
	if (timeout == MAX_SCHEDULE_TIMEOUT) {
		while (!shared.done &&
		    !dma_fence_test_signaled_any(fences, count, idx)) {
			if (intr) {
				cv_ret = cv_wait_sig(&shared.cv, &shared.mtx);
				if (cv_ret == 0) {
					ret = -ERESTARTSYS;
					break;
				}
			} else {
				cv_wait(&shared.cv, &shared.mtx);
			}
		}
	} else {
		while (!shared.done && ret > 0 &&
		    !dma_fence_test_signaled_any(fences, count, idx)) {
			if (intr)
				cv_ret = cv_timedwait_sig(&shared.cv,
				    &shared.mtx, ddi_get_lbolt() + ret);
			else
				cv_ret = cv_timedwait(&shared.cv,
				    &shared.mtx, ddi_get_lbolt() + ret);
			if (cv_ret == -1) {
				ret = 0;
				break;
			}
			if (cv_ret == 0 && intr) {
				ret = -ERESTARTSYS;
				break;
			}
		}
		if (dma_fence_test_signaled_any(fences, count, idx) && ret >= 0)
			ret = MAX(ret, 1L);
	}
	mutex_exit(&shared.mtx);

cb_cleanup:
	while (i-- > 0)
		dma_fence_remove_callback(fences[i], &cb[i].cb);

	cv_destroy(&shared.cv);
	mutex_destroy(&shared.mtx);
	kfree(cb);
	return ret;
}

void
dma_fence_set_deadline(struct dma_fence *f, ktime_t t)
{
	if (f->ops->set_deadline == NULL)
		return;
	if (!dma_fence_is_signaled(f))
		f->ops->set_deadline(f, t);
}

/* dma_fence_stub: a pre-signaled stub fence */

static struct dma_fence dma_fence_stub;
static spinlock_t dma_fence_stub_mtx; /* initialized in drm_linux_init() */

static const char *
dma_fence_stub_get_name(struct dma_fence *fence)
{
	return "stub";
}

static const struct dma_fence_ops dma_fence_stub_ops = {
	.get_driver_name  = dma_fence_stub_get_name,
	.get_timeline_name = dma_fence_stub_get_name,
};

struct dma_fence *
dma_fence_get_stub(void)
{
	mutex_enter(&dma_fence_stub_mtx);
	if (dma_fence_stub.ops == NULL) {
		dma_fence_init(&dma_fence_stub, &dma_fence_stub_ops,
		    &dma_fence_stub_mtx, 0, 0);
		dma_fence_signal_locked(&dma_fence_stub);
	}
	mutex_exit(&dma_fence_stub_mtx);

	return dma_fence_get(&dma_fence_stub);
}

struct dma_fence *
dma_fence_allocate_private_stub(ktime_t ts)
{
	struct dma_fence *f = kzalloc(sizeof(*f), GFP_KERNEL);
	if (f == NULL)
		return NULL;
	dma_fence_init(f, &dma_fence_stub_ops, &dma_fence_stub_mtx, 0, 0);
	dma_fence_signal_timestamp(f, ts);
	return f;
}

/* ===== DMA fence array ===== */

static const char *
dma_fence_array_get_driver_name(struct dma_fence *fence)
{
	return "dma_fence_array";
}

static const char *
dma_fence_array_get_timeline_name(struct dma_fence *fence)
{
	return "unbound";
}

static void
irq_dma_fence_array_work(void *arg)
{
	struct dma_fence_array *dfa = (struct dma_fence_array *)arg;
	dma_fence_signal(&dfa->base);
	dma_fence_put(&dfa->base);
}

static void
dma_fence_array_cb_func(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct dma_fence_array_cb *array_cb =
	    container_of(cb, struct dma_fence_array_cb, cb);
	struct dma_fence_array *dfa = array_cb->array;

	if (atomic_dec_and_test(&dfa->num_pending))
		callout_reset(&dfa->to, 1, irq_dma_fence_array_work, dfa);
	else
		dma_fence_put(&dfa->base);
}

static bool
dma_fence_array_enable_signaling(struct dma_fence *fence)
{
	struct dma_fence_array *dfa = to_dma_fence_array(fence);
	struct dma_fence_array_cb *cb = (void *)(&dfa[1]);
	int i;

	for (i = 0; i < dfa->num_fences; ++i) {
		cb[i].array = dfa;
		dma_fence_get(&dfa->base);
		if (dma_fence_add_callback(dfa->fences[i], &cb[i].cb,
		    dma_fence_array_cb_func)) {
			dma_fence_put(&dfa->base);
			if (atomic_dec_and_test(&dfa->num_pending))
				return false;
		}
	}

	return true;
}

static bool
dma_fence_array_signaled(struct dma_fence *fence)
{
	struct dma_fence_array *dfa = to_dma_fence_array(fence);

	return atomic_read(&dfa->num_pending) <= 0;
}

static void
dma_fence_array_release(struct dma_fence *fence)
{
	struct dma_fence_array *dfa = to_dma_fence_array(fence);
	int i;

	for (i = 0; i < dfa->num_fences; ++i)
		dma_fence_put(dfa->fences[i]);

	kfree(dfa->fences);
	dma_fence_free(fence);
}

struct dma_fence_array *
dma_fence_array_create(int num_fences, struct dma_fence **fences, u64 context,
    unsigned seqno, bool signal_on_any)
{
	struct dma_fence_array *dfa;

	dfa = kzalloc(sizeof(*dfa) +
	    (num_fences * sizeof(struct dma_fence_array_cb)), GFP_KERNEL);
	if (dfa == NULL)
		return NULL;

	mutex_init(&dfa->lock, NULL, MUTEX_DRIVER, NULL);
	dma_fence_init(&dfa->base, &dma_fence_array_ops, &dfa->lock,
	    context, seqno);
	callout_init(&dfa->to, 0);

	dfa->num_fences = num_fences;
	atomic_set(&dfa->num_pending, signal_on_any ? 1 : num_fences);
	dfa->fences = fences;

	return dfa;
}

struct dma_fence *
dma_fence_array_first(struct dma_fence *f)
{
	struct dma_fence_array *dfa;

	if (f == NULL)
		return NULL;

	if ((dfa = to_dma_fence_array(f)) == NULL)
		return f;

	if (dfa->num_fences > 0)
		return dfa->fences[0];

	return NULL;
}

struct dma_fence *
dma_fence_array_next(struct dma_fence *f, unsigned int i)
{
	struct dma_fence_array *dfa;

	if (f == NULL)
		return NULL;

	if ((dfa = to_dma_fence_array(f)) == NULL)
		return NULL;

	if (i < dfa->num_fences)
		return dfa->fences[i];

	return NULL;
}

const struct dma_fence_ops dma_fence_array_ops = {
	.get_driver_name  = dma_fence_array_get_driver_name,
	.get_timeline_name = dma_fence_array_get_timeline_name,
	.enable_signaling = dma_fence_array_enable_signaling,
	.signaled         = dma_fence_array_signaled,
	.release          = dma_fence_array_release,
};

/* ===== DMA fence chain ===== */

int
dma_fence_chain_find_seqno(struct dma_fence **df, uint64_t seqno)
{
	struct dma_fence_chain *chain;
	struct dma_fence *fence;

	if (seqno == 0)
		return 0;

	if ((chain = to_dma_fence_chain(*df)) == NULL)
		return -EINVAL;

	fence = &chain->base;
	if (fence->seqno < seqno)
		return -EINVAL;

	dma_fence_chain_for_each(*df, fence) {
		if ((*df)->context != fence->context)
			break;

		chain = to_dma_fence_chain(*df);
		if (chain->prev_seqno < seqno)
			break;
	}
	dma_fence_put(fence);

	return 0;
}

void
dma_fence_chain_init(struct dma_fence_chain *chain, struct dma_fence *prev,
    struct dma_fence *fence, uint64_t seqno)
{
	uint64_t context;

	chain->fence = fence;
	chain->prev = prev;
	mutex_init(&chain->lock, NULL, MUTEX_DRIVER, NULL);

	if (to_dma_fence_chain(prev) != NULL) {
		if (__dma_fence_is_later(seqno, prev->seqno, prev->ops)) {
			chain->prev_seqno = prev->seqno;
			context = prev->context;
		} else {
			chain->prev_seqno = 0;
			context = dma_fence_context_alloc(1);
			seqno = prev->seqno;
		}
	} else {
		chain->prev_seqno = 0;
		context = dma_fence_context_alloc(1);
	}

	dma_fence_init(&chain->base, &dma_fence_chain_ops, &chain->lock,
	    context, seqno);
}

static const char *
dma_fence_chain_get_driver_name(struct dma_fence *fence)
{
	return "dma_fence_chain";
}

static const char *
dma_fence_chain_get_timeline_name(struct dma_fence *fence)
{
	return "unbound";
}

static bool dma_fence_chain_enable_signaling(struct dma_fence *);

static void
dma_fence_chain_timo(void *arg)
{
	struct dma_fence_chain *chain = (struct dma_fence_chain *)arg;

	if (dma_fence_chain_enable_signaling(&chain->base) == false)
		dma_fence_signal(&chain->base);
	dma_fence_put(&chain->base);
}

static void
dma_fence_chain_cb(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct dma_fence_chain *chain =
	    container_of(cb, struct dma_fence_chain, cb);

	/* cb and to share a union; after callback fires, reuse as callout */
	callout_reset(&chain->to, 1, dma_fence_chain_timo, chain);
	dma_fence_put(f);
}

static bool
dma_fence_chain_enable_signaling(struct dma_fence *fence)
{
	struct dma_fence_chain *chain, *h;
	struct dma_fence *f;

	h = to_dma_fence_chain(fence);
	dma_fence_get(&h->base);
	dma_fence_chain_for_each(fence, &h->base) {
		chain = to_dma_fence_chain(fence);
		if (chain == NULL)
			f = fence;
		else
			f = chain->fence;

		dma_fence_get(f);
		if (!dma_fence_add_callback(f, &h->cb, dma_fence_chain_cb)) {
			dma_fence_put(fence);
			return true;
		}
		dma_fence_put(f);
	}
	dma_fence_put(&h->base);
	return false;
}

static bool
dma_fence_chain_signaled(struct dma_fence *fence)
{
	struct dma_fence_chain *chain;
	struct dma_fence *f;

	dma_fence_chain_for_each(fence, fence) {
		chain = to_dma_fence_chain(fence);
		if (chain == NULL)
			f = fence;
		else
			f = chain->fence;

		if (!dma_fence_is_signaled(f)) {
			dma_fence_put(fence);
			return false;
		}
	}
	return true;
}

static void
dma_fence_chain_release(struct dma_fence *fence)
{
	struct dma_fence_chain *chain = to_dma_fence_chain(fence);
	struct dma_fence_chain *prev_chain;
	struct dma_fence *prev;

	for (prev = chain->prev; prev != NULL; prev = chain->prev) {
		if (kref_read(&prev->refcount) > 1)
			break;
		if ((prev_chain = to_dma_fence_chain(prev)) == NULL)
			break;
		chain->prev = prev_chain->prev;
		prev_chain->prev = NULL;
		dma_fence_put(prev);
	}
	dma_fence_put(prev);
	dma_fence_put(chain->fence);
	dma_fence_free(fence);
}

struct dma_fence *
dma_fence_chain_walk(struct dma_fence *fence)
{
	struct dma_fence_chain *chain = to_dma_fence_chain(fence), *prev_chain;
	struct dma_fence *prev, *new_prev, *tmp;

	if (chain == NULL) {
		dma_fence_put(fence);
		return NULL;
	}

	while ((prev = dma_fence_get(chain->prev)) != NULL) {
		prev_chain = to_dma_fence_chain(prev);
		if (prev_chain != NULL) {
			if (!dma_fence_is_signaled(prev_chain->fence))
				break;
			new_prev = dma_fence_get(prev_chain->prev);
		} else {
			if (!dma_fence_is_signaled(prev))
				break;
			new_prev = NULL;
		}
		tmp = atomic_cas_ptr(&chain->prev, prev, new_prev);
		dma_fence_put(tmp == prev ? prev : new_prev);
		dma_fence_put(prev);
	}

	dma_fence_put(fence);
	return prev;
}

const struct dma_fence_ops dma_fence_chain_ops = {
	.get_driver_name  = dma_fence_chain_get_driver_name,
	.get_timeline_name = dma_fence_chain_get_timeline_name,
	.enable_signaling = dma_fence_chain_enable_signaling,
	.signaled         = dma_fence_chain_signaled,
	.release          = dma_fence_chain_release,
	.use_64bit_seqno  = true,
};

bool
dma_fence_is_container(struct dma_fence *fence)
{
	return (fence->ops == &dma_fence_chain_ops) ||
	    (fence->ops == &dma_fence_array_ops);
}

/* ===== DMA-BUF (Phase 1 stubs) ===== */

struct dma_buf *
dma_buf_export(const struct dma_buf_export_info *info)
{
	/* Phase 1: DMA-BUF fd infrastructure not yet wired to illumos */
	return ERR_PTR(-ENOTSUP);
}

struct dma_buf *
dma_buf_get(int fd)
{
	return ERR_PTR(-ENOTSUP);
}

void
dma_buf_put(struct dma_buf *dmabuf)
{
}

int
dma_buf_fd(struct dma_buf *dmabuf, int flags)
{
	return -ENOTSUP;
}

void
get_dma_buf(struct dma_buf *dmabuf)
{
}

/* ===== PCIe link speed / width / ASPM ===== */

enum pci_bus_speed
pcie_get_speed_cap(struct pci_dev *pdev)
{
	ddi_acc_handle_t hdl;
	uint32_t xcap, lnkcap = 0, lnkcap2 = 0;
	enum pci_bus_speed cap = PCI_SPEED_UNKNOWN;
	int pos;

	if (pdev == NULL)
		return PCI_SPEED_UNKNOWN;

	hdl = pdev->config_handle;
	pos = 0;
	if (__pci_find_capability(hdl, PCI_CAP_ID_EXP, &pos) != 0)
		return PCI_SPEED_UNKNOWN;

	xcap    = pci_config_get32(hdl, pos + PCI_PCIE_XCAP);
	lnkcap  = pci_config_get32(hdl, pos + PCI_PCIE_LCAP);
	if (PCI_PCIE_XCAP_VER(xcap) >= 2)
		lnkcap2 = pci_config_get32(hdl, pos + PCI_PCIE_LCAP2);

	lnkcap  &= 0x0f;
	lnkcap2 &= 0xfe;

	if (lnkcap2) { /* PCIE GEN 3.0+ */
		if (lnkcap2 & 0x02)  cap = PCIE_SPEED_2_5GT;
		if (lnkcap2 & 0x04)  cap = PCIE_SPEED_5_0GT;
		if (lnkcap2 & 0x08)  cap = PCIE_SPEED_8_0GT;
		if (lnkcap2 & 0x10)  cap = PCIE_SPEED_16_0GT;
		if (lnkcap2 & 0x20)  cap = PCIE_SPEED_32_0GT;
		if (lnkcap2 & 0x40)  cap = PCIE_SPEED_64_0GT;
	} else {
		if (lnkcap & 0x01)   cap = PCIE_SPEED_2_5GT;
		if (lnkcap & 0x02)   cap = PCIE_SPEED_5_0GT;
	}

	return cap;
}

enum pcie_link_width
pcie_get_width_cap(struct pci_dev *pdev)
{
	ddi_acc_handle_t hdl;
	uint32_t lnkcap = 0;
	int pos;

	if (pdev == NULL)
		return PCIE_LNK_WIDTH_UNKNOWN;

	hdl = pdev->config_handle;
	pos = 0;
	if (__pci_find_capability(hdl, PCI_CAP_ID_EXP, &pos) != 0)
		return PCIE_LNK_WIDTH_UNKNOWN;

	lnkcap = pci_config_get32(hdl, pos + PCI_PCIE_LCAP);

	if (lnkcap)
		return (lnkcap & 0x3f0) >> 4;
	return PCIE_LNK_WIDTH_UNKNOWN;
}

bool
pcie_aspm_enabled(struct pci_dev *pdev)
{
	ddi_acc_handle_t hdl;
	uint32_t lcsr;
	int pos;

	if (pdev == NULL)
		return false;

	hdl = pdev->config_handle;
	pos = 0;
	if (__pci_find_capability(hdl, PCI_CAP_ID_EXP, &pos) != 0)
		return false;

	lcsr = pci_config_get32(hdl, pos + PCI_PCIE_LCSR);
	if ((lcsr & (PCI_PCIE_LCSR_ASPM_L0S | PCI_PCIE_LCSR_ASPM_L1)) != 0)
		return true;

	return false;
}

/* pci_resize_resource: resize a BAR via PCIe extended capability */

#define PCIE_ECAP_RESIZE_BAR	0x15
#define RBCAP0			0x04
#define RBCTRL0			0x08
#define RBCTRL_BARINDEX_MASK	0x07
#define RBCTRL_BARSIZE_MASK	0x1f00
#define RBCTRL_BARSIZE_SHIFT	8

int
pci_resize_resource(struct pci_dev *pdev, int bar, int nsize)
{
	ddi_acc_handle_t hdl = pdev->config_handle;
	uint32_t reg;
	uint16_t offset = PCI_PCIE_ECAP;
	uint16_t capid;

	ASSERT(bar == 0);

	do {
		reg = pci_config_get32(hdl, offset);
		capid = (uint16_t)(reg & 0xffff);
		if (capid == PCIE_ECAP_RESIZE_BAR)
			break;
		offset = (uint16_t)((reg >> 20) & 0xffc);
	} while (offset != 0 && capid != 0);

	if (capid == 0) {
		cmn_err(CE_WARN, "pci_resize_resource: no resize BAR cap");
		return -ENOTSUP;
	}

	reg = pci_config_get32(hdl, offset + RBCAP0);
	if ((reg & (1 << (nsize + 4))) == 0) {
		cmn_err(CE_WARN, "pci_resize_resource: size not supported");
		return -ENOTSUP;
	}

	reg = pci_config_get32(hdl, offset + RBCTRL0);
	if ((reg & RBCTRL_BARINDEX_MASK) != 0) {
		cmn_err(CE_WARN, "pci_resize_resource: BAR index not 0");
		return -EINVAL;
	}

	reg &= ~RBCTRL_BARSIZE_MASK;
	reg |= (nsize << RBCTRL_BARSIZE_SHIFT) & RBCTRL_BARSIZE_MASK;
	pci_config_put32(hdl, offset + RBCTRL0, reg);

	return 0;
}

/* ===== Shrinker ===== */

static TAILQ_HEAD(shrinkers, shrinker) shrinker_list =
    TAILQ_HEAD_INITIALIZER(shrinker_list);
static kmutex_t shrinker_lock;

struct shrinker *
shrinker_alloc(u_int flags, const char *format, ...)
{
	struct shrinker *s;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (s)
		s->seeks = DEFAULT_SEEKS;
	return s;
}

void
shrinker_register(struct shrinker *shrinker)
{
	mutex_enter(&shrinker_lock);
	TAILQ_INSERT_TAIL(&shrinker_list, shrinker, next);
	mutex_exit(&shrinker_lock);
}

void
shrinker_free(struct shrinker *shrinker)
{
	mutex_enter(&shrinker_lock);
	TAILQ_REMOVE(&shrinker_list, shrinker, next);
	mutex_exit(&shrinker_lock);
	kfree(shrinker);
}

unsigned long
drmbackoff(long npages)
{
	struct shrink_control sc;
	struct shrinker *shrinker;
	u_long ret, freed = 0;

	mutex_enter(&shrinker_lock);
	shrinker = TAILQ_FIRST(&shrinker_list);
	while (shrinker && npages > 0) {
		sc.nr_to_scan = npages;
		ret = shrinker->scan_objects(shrinker, &sc);
		if (ret == SHRINK_STOP)
			break;
		npages -= ret;
		freed += ret;
		shrinker = TAILQ_NEXT(shrinker, next);
	}
	mutex_exit(&shrinker_lock);

	return freed;
}

/* ===== Bitmap ===== */

void *
bitmap_zalloc(u_int n, gfp_t flags)
{
	return kcalloc(BITS_TO_LONGS(n), sizeof(long), flags);
}

void
bitmap_free(void *p)
{
	kfree(p);
}

/* ===== atomic_dec_and_mutex_lock ===== */

int
atomic_dec_and_mutex_lock(volatile int *v, struct mutex *lock)
{
	if (atomic_add_unless(v, -1, 1))
		return 0;

	mutex_enter(lock);
	if (atomic_dec_return(v) == 0)
		return 1;
	mutex_exit(lock);
	return 0;
}

/* ===== printk ===== */

int
printk(const char *fmt, ...)
{
	int level;
	va_list ap;

	if (fmt != NULL && *fmt == '\001') {
		level = fmt[1];
#ifndef DRMDEBUG
		if (level >= KERN_INFO[1] && level <= '9')
			return 0;
#endif
		fmt += 2;
	}

	va_start(ap, fmt);
	vcmn_err(CE_CONT, fmt, ap);
	va_end(ap);

	return 0;
}

/* ===== Interval tree ===== */

#define START(node) ((node)->start)
#define LAST(node)  ((node)->last)

struct interval_tree_node *
interval_tree_iter_first(struct rb_root_cached *root, unsigned long start,
    unsigned long last)
{
	struct interval_tree_node *node;
	struct rb_node *rb;

	for (rb = rb_first_cached(root); rb; rb = rb_next(rb)) {
		node = rb_entry(rb, typeof(*node), rb);
		if (LAST(node) >= start && START(node) <= last)
			return node;
	}
	return NULL;
}

void
interval_tree_remove(struct interval_tree_node *node,
    struct rb_root_cached *root)
{
	rb_erase_cached(&node->rb, root);
}

void
interval_tree_insert(struct interval_tree_node *node,
    struct rb_root_cached *root)
{
	struct rb_node **iter = &root->rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct interval_tree_node *iter_node;

	while (*iter) {
		parent = *iter;
		iter_node = rb_entry(*iter, struct interval_tree_node, rb);

		if (node->start < iter_node->start)
			iter = &(*iter)->rb_left;
		else
			iter = &(*iter)->rb_right;
	}

	rb_link_node(&node->rb, parent, iter);
	rb_insert_color_cached(&node->rb, root, false);
}

/* ===== Sync file (Phase 1 stubs) ===== */

struct sync_file *
sync_file_create(struct dma_fence *fence)
{
	/* Phase 1: sync_file fd infrastructure not yet wired to illumos */
	return NULL;
}

struct dma_fence *
sync_file_get_fence(int fd)
{
	return NULL;
}

/* ===== fd_install / fput / get_unused_fd_flags / put_unused_fd (stubs) ===== */

void
fd_install(int fd, struct file *fp)
{
	/* Phase 1: stub */
}

void
fput(struct file *fp)
{
	/* Phase 1: stub */
}

int
get_unused_fd_flags(unsigned int flags)
{
	return -1;
}

void
put_unused_fd(int fd)
{
}

/* ===== memremap / memunmap ===== */

void *
memremap(phys_addr_t phys_addr, size_t size, int flags)
{
	return NULL;
}

void
memunmap(void *addr)
{
}

/* ===== kfree_const ===== */

void
kfree_const(const void *addr)
{
	kfree((void *)addr);
}

/* ===== DMA coherent alloc (Phase 1 stub) ===== */

/*
 * Phase 1: Use plain kmem_zalloc and a fake DMA address.
 * Full implementation requires ddi_dma_mem_alloc.
 */
void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle,
    int gfp)
{
	void *mem = kmem_zalloc(size, KM_SLEEP);
	*dma_handle = (dma_addr_t)((uintptr_t)mem); /* fake: use VA as PA */
	return mem;
}

void
dma_free_coherent(struct device *dev, size_t size, void *cpu_addr,
    dma_addr_t dma_handle)
{
	if (cpu_addr)
		kmem_free(cpu_addr, size);
}

int
dma_get_sgtable(struct device *dev, struct sg_table *sgt, void *cpu_addr,
    dma_addr_t dma_addr, size_t size)
{
	int ret;

	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (ret)
		return ret;

	sg_set_page(sgt->sgl, NULL, size, 0);
	sgt->sgl->dma_address = dma_addr;
	return 0;
}

dma_addr_t
dma_map_resource(struct device *dev, phys_addr_t phys_addr, size_t size,
    enum dma_data_direction dir, u_long attr)
{
	return (dma_addr_t)phys_addr; /* identity map for Phase 1 */
}

/* ===== IOMMU (Phase 1 stubs) ===== */

size_t
iommu_map_sgtable(struct iommu_domain *domain, u_long iova,
    struct sg_table *sgt, int prot)
{
	return 0;
}

size_t
iommu_unmap(struct iommu_domain *domain, u_long iova, size_t size)
{
	return 0;
}

struct iommu_domain *
iommu_get_domain_for_dev(struct device *dev)
{
	return NULL;
}

phys_addr_t
iommu_iova_to_phys(struct iommu_domain *domain, dma_addr_t iova)
{
	return 0;
}

struct iommu_domain *
iommu_domain_alloc(struct bus_type *type)
{
	return kzalloc(sizeof(struct iommu_domain), GFP_KERNEL);
}

int
iommu_attach_device(struct iommu_domain *domain, struct device *dev)
{
	return 0;
}

/* ===== Component framework ===== */

struct component {
	struct device *dev;
	struct device *adev;
	const struct component_ops *ops;
	LIST_ENTRY(component) next;
};

static LIST_HEAD(component_list_head, component) component_list =
    LIST_HEAD_INITIALIZER(component_list);
static kmutex_t component_lock;

int
component_add(struct device *dev, const struct component_ops *ops)
{
	struct component *comp;

	comp = kzalloc(sizeof(*comp), GFP_KERNEL);
	if (comp == NULL)
		return -ENOMEM;
	comp->dev = dev;
	comp->ops = ops;
	mutex_enter(&component_lock);
	LIST_INSERT_HEAD(&component_list, comp, next);
	mutex_exit(&component_lock);
	return 0;
}

int
component_add_typed(struct device *dev, const struct component_ops *ops,
    int type)
{
	return component_add(dev, ops);
}

int
component_bind_all(struct device *dev, void *data)
{
	struct component *comp;
	int ret = 0;

	mutex_enter(&component_lock);
	LIST_FOREACH(comp, &component_list, next) {
		if (comp->adev == dev) {
			ret = comp->ops->bind(comp->dev, NULL, data);
			if (ret)
				break;
		}
	}
	mutex_exit(&component_lock);

	return ret;
}

struct component_match_entry {
	int (*compare)(struct device *, void *);
	void *data;
};

struct component_match {
	struct component_match_entry match[4];
	int nmatches;
};

int
component_master_add_with_match(struct device *dev,
    const struct component_master_ops *ops, struct component_match *match)
{
	struct component *comp;
	int found = 0;
	int i, ret;

	mutex_enter(&component_lock);
	LIST_FOREACH(comp, &component_list, next) {
		for (i = 0; i < match->nmatches; i++) {
			struct component_match_entry *m = &match->match[i];
			if (m->compare(comp->dev, m->data)) {
				comp->adev = dev;
				found = 1;
				break;
			}
		}
	}
	mutex_exit(&component_lock);

	if (found) {
		ret = ops->bind(dev);
		if (ret)
			return ret;
	}

	return 0;
}

/* ===== wait_on_bit / wake_up_bit ===== */

static wait_queue_head_t bit_waitq;
wait_queue_head_t var_waitq;
static kmutex_t wait_bit_mtx;   /* initialized in drm_linux_init() */
static kcondvar_t wait_bit_cv;  /* initialized in drm_linux_init() */

int
wait_on_bit(unsigned long *word, int bit, unsigned mode)
{
	if (!test_bit(bit, word))
		return 0;

	mutex_enter(&wait_bit_mtx);
	while (test_bit(bit, word)) {
		if (mode & TASK_INTERRUPTIBLE) {
			if (cv_wait_sig(&wait_bit_cv, &wait_bit_mtx) == 0) {
				mutex_exit(&wait_bit_mtx);
				return 1;
			}
		} else {
			cv_wait(&wait_bit_cv, &wait_bit_mtx);
		}
	}
	mutex_exit(&wait_bit_mtx);
	return 0;
}

int
wait_on_bit_timeout(unsigned long *word, int bit, unsigned mode, int timo)
{
	clock_t deadline;

	if (!test_bit(bit, word))
		return 0;

	deadline = ddi_get_lbolt() + timo;
	mutex_enter(&wait_bit_mtx);
	while (test_bit(bit, word)) {
		int r;
		if (mode & TASK_INTERRUPTIBLE)
			r = cv_timedwait_sig(&wait_bit_cv, &wait_bit_mtx,
			    deadline);
		else
			r = cv_timedwait(&wait_bit_cv, &wait_bit_mtx, deadline);
		if (r == -1 || r == 0) {
			mutex_exit(&wait_bit_mtx);
			return 1;
		}
	}
	mutex_exit(&wait_bit_mtx);
	return 0;
}

void
wake_up_bit(void *word, int bit)
{
	mutex_enter(&wait_bit_mtx);
	cv_broadcast(&wait_bit_cv);
	mutex_exit(&wait_bit_mtx);
}

void
clear_and_wake_up_bit(int bit, void *word)
{
	clear_bit(bit, word);
	wake_up_bit(word, bit);
}

wait_queue_head_t *
bit_waitqueue(void *word, int bit)
{
	return &bit_waitq;
}

wait_queue_head_t *
__var_waitqueue(void *p)
{
	return &var_waitq;
}

/* ===== Global workqueues and tasklets ===== */

struct workqueue_struct *system_wq;
struct workqueue_struct *system_highpri_wq;
struct workqueue_struct *system_unbound_wq;
struct workqueue_struct *system_long_wq;
taskq_t *taskletq;

/* ===== drm_linux_init / drm_linux_exit ===== */

void
drm_linux_init(void)
{
	/* Per-thread task_struct TSD */
	tsd_create(&drm_task_tsd_key, drm_task_tsd_destructor);

	/* Global workqueues */
	system_wq = (struct workqueue_struct *)
	    taskq_create("drmwq", 4, minclsyspri, 1, INT_MAX,
	    TASKQ_PREPOPULATE);
	system_highpri_wq = (struct workqueue_struct *)
	    taskq_create("drmhpwq", 4, minclsyspri, 1, INT_MAX,
	    TASKQ_PREPOPULATE);
	system_unbound_wq = (struct workqueue_struct *)
	    taskq_create("drmubwq", 4, minclsyspri, 1, INT_MAX,
	    TASKQ_PREPOPULATE);
	system_long_wq = (struct workqueue_struct *)
	    taskq_create("drmlwq", 4, minclsyspri, 1, INT_MAX,
	    TASKQ_PREPOPULATE);

	taskletq = taskq_create("drmtskl", 1, minclsyspri, 1, INT_MAX,
	    TASKQ_PREPOPULATE);

	/* bit wait queue */
	mutex_init(&wait_bit_mtx, NULL, MUTEX_DRIVER, NULL);
	cv_init(&wait_bit_cv, NULL, CV_DRIVER, NULL);
	init_waitqueue_head(&bit_waitq);
	init_waitqueue_head(&var_waitq);

	/* IDR and XArray caches */
	idr_cache = kmem_cache_create("idr_entry", sizeof(struct idr_entry),
	    0, 0, NULL);
	xa_cache = kmem_cache_create("xa_entry", sizeof(struct xarray_entry),
	    0, 0, NULL);

	/* dma_fence_stub lock */
	mutex_init(&dma_fence_stub_mtx, NULL, MUTEX_DRIVER, NULL);

	/* kthread list lock */
	mutex_init(&kthread_list_lock, NULL, MUTEX_DRIVER, NULL);

	/* drvdata lock */
	mutex_init(&drvdata_lock, NULL, MUTEX_DRIVER, NULL);

	/* shrinker lock */
	mutex_init(&shrinker_lock, NULL, MUTEX_DRIVER, NULL);

	/* component lock */
	mutex_init(&component_lock, NULL, MUTEX_DRIVER, NULL);
}

void
drm_linux_exit(void)
{
	tsd_destroy(&drm_task_tsd_key);

	mutex_destroy(&component_lock);
	mutex_destroy(&shrinker_lock);
	mutex_destroy(&drvdata_lock);
	mutex_destroy(&kthread_list_lock);
	mutex_destroy(&dma_fence_stub_mtx);

	kmem_cache_destroy(xa_cache);
	kmem_cache_destroy(idr_cache);

	destroy_waitqueue_head(&var_waitq);
	destroy_waitqueue_head(&bit_waitq);
	cv_destroy(&wait_bit_cv);
	mutex_destroy(&wait_bit_mtx);

	taskq_destroy(taskletq);
	taskq_destroy((taskq_t *)system_long_wq);
	taskq_destroy((taskq_t *)system_unbound_wq);
	taskq_destroy((taskq_t *)system_highpri_wq);
	taskq_destroy((taskq_t *)system_wq);
}
