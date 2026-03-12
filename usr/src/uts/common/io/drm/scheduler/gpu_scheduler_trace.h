/* Public domain. */
/* illumos: gpu_scheduler_trace.h stub */

#ifndef _GPU_SCHEDULER_TRACE_H
#define _GPU_SCHEDULER_TRACE_H

/* Tracepoints are no-ops on illumos */
#define trace_drm_sched_job(job, entity)		do {} while (0)
#define trace_drm_run_job(sched_job, entity)		do {} while (0)
#define trace_drm_sched_process_job(fence)		do {} while (0)
#define trace_drm_sched_job_wait_dep(job, fence)	do {} while (0)

#endif /* _GPU_SCHEDULER_TRACE_H */
