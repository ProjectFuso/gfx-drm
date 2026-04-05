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
	struct drm_illumos_irq_vector *vec = arg;

	vec->thread_fn(0, vec->dev_id);
}

static uint_t
drm_illumos_ddi_intr_handler(caddr_t arg1, caddr_t arg2)
{
	struct drm_illumos_irq_vector *vec = (void *)arg1;
	struct drm_illumos_irq_state *irq = (void *)arg2;
	irqreturn_t ret;

	ret = vec->handler(0, vec->dev_id);
	if (ret == IRQ_NONE)
		return (DDI_INTR_UNCLAIMED);

	if (ret == IRQ_WAKE_THREAD && vec->thread_fn != NULL && irq->tq != NULL)
		(void) taskq_dispatch(irq->tq, drm_illumos_irq_thread,
		    vec, TQ_NOSLEEP);

	return (DDI_INTR_CLAIMED);
}

int
drm_illumos_irq_install_multivector(dev_info_t *dip,
    struct drm_illumos_irq_state *irq, const char *taskq_name,
    uint_t nvec_requested, uint_t *nvec_actual,
    const struct drm_illumos_irq_vector *vectors)
{
	int actual = 0, nintrs = 0, ret;
	int types[] = { DDI_INTR_TYPE_MSIX, DDI_INTR_TYPE_MSI,
	    DDI_INTR_TYPE_FIXED };
	int i, j;

	if (dip == NULL || irq == NULL || vectors == NULL ||
	    nvec_requested == 0 || nvec_actual == NULL)
		return (ENODEV);
	if (irq->registered)
		return (0);

	for (i = 0; i < 3; i++) {
		ret = ddi_intr_get_nintrs(dip, types[i], &nintrs);
		if (ret == DDI_SUCCESS && nintrs > 0) {
			irq->intr_type = types[i];
			break;
		}
	}
	if (i == 3)
		return (ENODEV);

	if (nvec_requested > (uint_t)nintrs)
		nvec_requested = (uint_t)nintrs;

	irq->intr_hdls = kmem_zalloc(sizeof (ddi_intr_handle_t) * nvec_requested,
	    KM_SLEEP);
	ret = ddi_intr_alloc(dip, irq->intr_hdls, irq->intr_type, 0,
	    nvec_requested, &actual, DDI_INTR_ALLOC_NORMAL);
	if (ret != DDI_SUCCESS || actual < 1) {
		kmem_free(irq->intr_hdls,
		    sizeof (ddi_intr_handle_t) * nvec_requested);
		irq->intr_hdls = NULL;
		return (ENODEV);
	}

	irq->nvec = actual;
	irq->vectors = kmem_zalloc(sizeof (struct drm_illumos_irq_vector) *
	    actual, KM_SLEEP);
	bcopy(vectors, irq->vectors,
	    sizeof (struct drm_illumos_irq_vector) * actual);

	irq->tq = taskq_create(taskq_name != NULL ? taskq_name : "drm_irq",
	    actual, maxclsyspri, 1, actual, TASKQ_PREPOPULATE);
	if (irq->tq == NULL) {
		/* Cleanup below */
		ret = ENOMEM;
		goto fail;
	}

	for (j = 0; j < actual; j++) {
		ret = ddi_intr_add_handler(irq->intr_hdls[j],
		    drm_illumos_ddi_intr_handler, (caddr_t)&irq->vectors[j],
		    (caddr_t)irq);
		if (ret != DDI_SUCCESS)
			goto fail;
	}

	for (j = 0; j < actual; j++) {
		ret = ddi_intr_enable(irq->intr_hdls[j]);
		if (ret != DDI_SUCCESS)
			goto fail;
	}

	*nvec_actual = (uint_t)actual;
	irq->registered = true;
	return (0);

fail:
	for (j = 0; j < actual; j++) {
		(void) ddi_intr_disable(irq->intr_hdls[j]);
		(void) ddi_intr_remove_handler(irq->intr_hdls[j]);
	}
	for (j = 0; j < actual; j++) {
		(void) ddi_intr_free(irq->intr_hdls[j]);
	}
	kmem_free(irq->intr_hdls, sizeof (ddi_intr_handle_t) * nvec_requested);
	irq->intr_hdls = NULL;
	if (irq->vectors != NULL) {
		kmem_free(irq->vectors,
		    sizeof (struct drm_illumos_irq_vector) * actual);
		irq->vectors = NULL;
	}
	if (irq->tq != NULL) {
		taskq_destroy(irq->tq);
		irq->tq = NULL;
	}
	return (ret == DDI_SUCCESS ? ENODEV : ret);
}

int
drm_illumos_irq_install(dev_info_t *dip, struct drm_illumos_irq_state *irq,
    const char *taskq_name, irq_handler_t handler, irq_handler_t thread_fn,
    void *dev_id)
{
	struct drm_illumos_irq_vector vec;
	uint_t actual = 0;

	vec.handler = handler;
	vec.thread_fn = thread_fn;
	vec.dev_id = dev_id;
	vec.name = taskq_name;

	return (drm_illumos_irq_install_multivector(dip, irq, taskq_name,
	    1, &actual, &vec));
}

void
drm_illumos_irq_uninstall(struct drm_illumos_irq_state *irq)
{
	int j;

	if (irq == NULL || !irq->registered)
		return;

	for (j = 0; j < irq->nvec; j++) {
		(void) ddi_intr_disable(irq->intr_hdls[j]);
		(void) ddi_intr_remove_handler(irq->intr_hdls[j]);
		(void) ddi_intr_free(irq->intr_hdls[j]);
	}

	kmem_free(irq->intr_hdls, sizeof (ddi_intr_handle_t) * irq->nvec);
	irq->intr_hdls = NULL;

	if (irq->vectors != NULL) {
		kmem_free(irq->vectors,
		    sizeof (struct drm_illumos_irq_vector) * irq->nvec);
		irq->vectors = NULL;
	}

	if (irq->tq != NULL) {
		taskq_destroy(irq->tq);
		irq->tq = NULL;
	}

	irq->registered = false;
}
