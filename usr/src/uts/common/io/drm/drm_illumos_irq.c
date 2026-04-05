/*
 * Copyright (c) 2024, illumos contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/illumos_page_compat.h>

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sunddi.h>

#include <drm/drm_illumos.h>

static void
drm_illumos_irq_thread(void *arg)
{
	struct drm_illumos_irq_state *irq = arg;

	irq->thread_fn(0, irq->dev_id);
}

static uint_t
drm_illumos_ddi_intr_handler(caddr_t arg1, caddr_t arg2)
{
	struct drm_illumos_irq_state *irq = (void *)arg1;
	irqreturn_t ret;

	(void)arg2;

	ret = irq->handler(0, irq->dev_id);
	if (ret == IRQ_NONE)
		return (DDI_INTR_UNCLAIMED);

	if (ret == IRQ_WAKE_THREAD && irq->thread_fn != NULL && irq->tq != NULL)
		(void) taskq_dispatch(irq->tq, drm_illumos_irq_thread,
		    irq, TQ_NOSLEEP);

	return (DDI_INTR_CLAIMED);
}

int
drm_illumos_irq_install(dev_info_t *dip, struct drm_illumos_irq_state *irq,
    const char *taskq_name, irq_handler_t handler, irq_handler_t thread_fn,
    void *dev_id)
{
	int actual = 0, nintrs = 0, ret;

	if (dip == NULL || irq == NULL || handler == NULL)
		return (-ENODEV);
	if (irq->registered)
		return (0);

	ret = ddi_intr_get_nintrs(dip, DDI_INTR_TYPE_FIXED, &nintrs);
	if (ret != DDI_SUCCESS || nintrs < 1)
		return (-ENODEV);

	if (thread_fn != NULL) {
		irq->tq = taskq_create(taskq_name != NULL ? taskq_name : "drm_irq",
		    1, maxclsyspri, 1, 1, TASKQ_PREPOPULATE);
		if (irq->tq == NULL)
			return (-ENOMEM);
	}

	ret = ddi_intr_alloc(dip, &irq->intr_hdl, DDI_INTR_TYPE_FIXED, 0, 1,
	    &actual, DDI_INTR_ALLOC_NORMAL);
	if (ret != DDI_SUCCESS || actual < 1) {
		if (irq->tq != NULL) {
			taskq_destroy(irq->tq);
			irq->tq = NULL;
		}
		return (-ENODEV);
	}

	irq->handler = handler;
	irq->thread_fn = thread_fn;
	irq->dev_id = dev_id;

	ret = ddi_intr_add_handler(irq->intr_hdl, drm_illumos_ddi_intr_handler,
	    (caddr_t)irq, NULL);
	if (ret != DDI_SUCCESS) {
		ddi_intr_free(irq->intr_hdl);
		if (irq->tq != NULL) {
			taskq_destroy(irq->tq);
			irq->tq = NULL;
		}
		return (-ENODEV);
	}

	ret = ddi_intr_enable(irq->intr_hdl);
	if (ret != DDI_SUCCESS) {
		ddi_intr_remove_handler(irq->intr_hdl);
		ddi_intr_free(irq->intr_hdl);
		if (irq->tq != NULL) {
			taskq_destroy(irq->tq);
			irq->tq = NULL;
		}
		return (-ENODEV);
	}

	irq->registered = true;
	return (0);
}

void
drm_illumos_irq_uninstall(struct drm_illumos_irq_state *irq)
{
	if (irq == NULL || !irq->registered)
		return;

	ddi_intr_disable(irq->intr_hdl);
	ddi_intr_remove_handler(irq->intr_hdl);
	ddi_intr_free(irq->intr_hdl);

	if (irq->tq != NULL) {
		taskq_destroy(irq->tq);
		irq->tq = NULL;
	}

	irq->handler = NULL;
	irq->thread_fn = NULL;
	irq->dev_id = NULL;
	irq->registered = false;
}
