/*
 *
 * (C) COPYRIGHT 2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



/**
 * @file mali_kbase_js.h
 * Job Scheduler APIs.
 */

#ifndef _KBASE_JS_H_
#define _KBASE_JS_H_

#include <malisw/mali_malisw.h>
#include <osk/mali_osk.h>

#include "mali_kbase_js_defs.h"
#include "mali_kbase_js_policy.h"
#include "mali_kbase_defs.h"

/**
 * @addtogroup base_api
 * @{
 */

/**
 * @addtogroup base_kbase_api
 * @{
 */

/**
 * @addtogroup kbase_js Job Scheduler Internal APIs
 * @{
 *
 * These APIs are Internal to KBase and are available for use by the
 * @ref kbase_js_policy "Job Scheduler Policy APIs"
 */

/**
 * @brief Initialize the Job Scheduler
 *
 * The kbasep_js_device_data sub-structure of \a kbdev must be zero
 * initialized before passing to the kbasep_js_devdata_init() function. This is
 * to give efficient error path code.
 */
mali_error kbasep_js_devdata_init( kbase_device *kbdev );

/**
 * @brief Halt the Job Scheduler.
 *
 * It is safe to call this on \a kbdev even if it the kbasep_js_device_data
 * sub-structure was never initialized/failed initialization, to give efficient
 * error-path code.
 *
 * For this to work, the kbasep_js_device_data sub-structure of \a kbdev must
 * be zero initialized before passing to the kbasep_js_devdata_init()
 * function. This is to give efficient error path code.
 *
 * It is a Programming Error to call this whilst there are still kbase_context
 * structures registered with this scheduler.
 *
 */
void kbasep_js_devdata_halt( kbase_device * kbdev);

/**
 * @brief Terminate the Job Scheduler
 *
 * It is safe to call this on \a kbdev even if it the kbasep_js_device_data
 * sub-structure was never initialized/failed initialization, to give efficient
 * error-path code.
 *
 * For this to work, the kbasep_js_device_data sub-structure of \a kbdev must
 * be zero initialized before passing to the kbasep_js_devdata_init()
 * function. This is to give efficient error path code.
 *
 * It is a Programming Error to call this whilst there are still kbase_context
 * structures registered with this scheduler.
 */
void kbasep_js_devdata_term( kbase_device *kbdev );


/**
 * @brief Initialize the Scheduling Component of a kbase_context on the Job Scheduler.
 *
 * This effectively registers a kbase_context with a Job Scheduler.
 *
 * It does not register any jobs owned by the kbase_context with the scheduler.
 * Those must be separately registered by kbasep_js_add_job().
 *
 * The kbase_context must be zero intitialized before passing to the
 * kbase_js_init() function. This is to give efficient error path code.
 */
mali_error kbasep_js_kctx_init( kbase_context *kctx );

/**
 * @brief Terminate the Scheduling Component of a kbase_context on the Job Scheduler
 *
 * This effectively de-registers a kbase_context from its Job Scheduler
 *
 * It is safe to call this on a kbase_context that has never had or failed
 * initialization of its jctx.sched_info member, to give efficient error-path
 * code.
 *
 * For this to work, the kbase_context must be zero intitialized before passing
 * to the kbase_js_init() function.
 *
 * It is a Programming Error to call this whilst there are still jobs
 * registered with this context.
 */
void kbasep_js_kctx_term( kbase_context *kctx );

/**
 * @brief Add a job chain to the Job Scheduler, and take necessary actions to
 * schedule the context/run the job.
 *
 * This atomically does the following:
 * - Update the numbers of jobs information (including NSS state changes)
 * - Add the job to the run pool if necessary (part of init_job)
 *
 * Once this is done, then an appropriate action is taken:
 * - If the ctx is scheduled, it attempts to start the next job (which might be
 * this added job)
 * - Otherwise, and if this is the first job on the context, it enqueues it on
 * the Policy Queue
 *
 * The Policy's Queue can be updated by this in the following ways:
 * - In the above case that this is the first job on the context
 * - If the job is high priority and the context is not scheduled, then it
 * could cause the Policy to schedule out a low-priority context, allowing
 * this context to be scheduled in.
 *
 * If the context is already scheduled on the RunPool, then adding a job to it
 * is guarenteed not to update the Policy Queue. And so, the caller is
 * guarenteed to not need to try scheduling a context from the Run Pool - it
 * can safely assert that the result is MALI_FALSE.
 *
 * It is a programming error to have more than U32_MAX jobs in flight at a time.
 *
 * The following locking conditions are made on the caller:
 * - it must hold kbasep_js_kctx_info::ctx::jsctx_mutex.
 * - it must \em not hold kbasep_js_device_data::runpool_irq::lock (as this will be
 * obtained internally)
 * - it must \em not hold kbasep_js_device_data::runpool_mutex (as this will be
 * obtained internally)
 * - it must \em not hold kbasep_jd_device_data::queue_mutex (again, it's used internally).
 *
 * @return MALI_TRUE indicates that the Policy Queue was updated, and so the
 * caller will need to try scheduling a context onto the Run Pool.
 * @return MALI_FALSE indicates that no updates were made to the Policy Queue,
 * so no further action is required from the caller. This is \b always returned
 * when the context is currently scheduled.
 */
mali_bool kbasep_js_add_job( kbase_context *kctx, kbase_jd_atom *atom );

/**
 * @brief Remove a job chain from the Job Scheduler
 *
 * Removing a job from the Scheduler can cause an NSS/SS state transition. In
 * this case, slots that previously could not have jobs submitted to might now
 * be submittable to. For this reason, and NSS/SS state transition will cause
 * the Scheduler to try to submit new jobs on the jm_slots.
 *
 * It is a programming error to call this when:
 * - \a atom is not a job belonging to kctx.
 * - \a atom has already been removed from the Job Scheduler.
 * - \a atom is still in the runpool:
 *  - it has not been killed with kbasep_js_policy_kill_all_ctx_jobs()
 *  - or, it has not been removed with kbasep_js_policy_dequeue_job()
 *  - or, it has not been removed with kbasep_js_policy_dequeue_job_irq()
 *
 * The following locking conditions are made on the caller:
 * - it must hold kbasep_js_kctx_info::ctx::jsctx_mutex.
 * - it must \em not hold kbasep_js_device_data::runpool_irq::lock, (as this will be
 * obtained internally)
 * - it must \em not hold kbasep_js_device_data::runpool_mutex (as this could be
 obtained internally)
 * - it must \em not hold kbdev->jm_slots[ \a js ].lock (as this could be
 * obtained internally)
 *
 */
void kbasep_js_remove_job( kbase_context *kctx, kbase_jd_atom *atom );

/**
 * @brief Refcount a context as being busy, preventing it from being scheduled
 * out.
 *
 * @note This function can safely be called from IRQ context.
 *
 * The following locking conditions are made on the caller:
 * - it must \em not hold the kbasep_js_device_data::runpool_irq::lock, because
 * it will be used internally.
 *
 * @return value != MALI_FALSE if the retain succeeded, and the context will not be scheduled out.
 * @return MALI_FALSE if the retain failed (because the context is being/has been scheduled out). 
 */
mali_bool kbasep_js_runpool_retain_ctx( kbase_device *kbdev, kbase_context *kctx );

/**
 * @brief Refcount a context as being busy, preventing it from being scheduled
 * out.
 *
 * @note This function can safely be called from IRQ context.
 *
 * The following locks must be held by the caller:
 * - kbasep_js_device_data::runpool_irq::lock
 *
 * @return value != MALI_FALSE if the retain succeeded, and the context will not be scheduled out.
 * @return MALI_FALSE if the retain failed (because the context is being/has been scheduled out). 
 */
mali_bool kbasep_js_runpool_retain_ctx_nolock( kbase_device *kbdev, kbase_context *kctx );

/**
 * @brief Lookup a context in the Run Pool based upon its current address space
 * and ensure that is stays scheduled in.
 *
 * The context is refcounted as being busy to prevent it from scheduling
 * out. It must be released with kbasep_js_runpool_release_ctx() when it is no
 * longer required to stay scheduled in.
 *
 * @note This function can safely be called from IRQ context.
 *
 * The following locking conditions are made on the caller:
 * - it must \em not hold the kbasep_js_device_data::runpoool_irq::lock, because
 * it will be used internally.
 *
 * @return a valid kbase_context on success, which has been refcounted as being busy.
 * @return NULL on failure, indicating that no context was found in \a as_nr
 */
kbase_context* kbasep_js_runpool_lookup_ctx( kbase_device *kbdev, int as_nr );

/**
 * @brief Release a refcount of a context being busy, allowing it to be
 * scheduled out.
 *
 * When the refcount reaches zero and the context \em might be scheduled out
 * (depending on whether the Scheudling Policy has deemed it so, or if it has run
 * out of jobs).
 *
 * If the context does get scheduled out, then The following actions will be
 * taken as part of deschduling a context:
 * - For the context being descheduled:
 *  - If the context is in the processing of dying (all the jobs are being
 * removed from it), then descheduling also kills off any jobs remaining in the
 * context.
 *  - If the context is not dying, and any jobs remain after descheduling the
 * context then it is re-enqueued to the Policy's Queue.
 *  - Otherwise, the context is still known to the scheduler, but remains absent
 * from the Policy Queue until a job is next added to it.
 * - Once the context is descheduled, this also handles scheduling in a new
 * context (if one is available), and if necessary, running a job from that new
 * context.
 *
 * Unlike retaining a context in the runpool, this function \b cannot be called
 * from IRQ context.
 *
 * It is a programming error to call this on a \a kctx that is not currently
 * scheduled, or that already has a zero refcount.
 *
 * The following locking conditions are made on the caller:
 * - it must \em not hold the kbasep_js_device_data::runpool_irq::lock, because
 * it will be used internally.
 * - it must \em not hold kbasep_js_kctx_info::ctx::jsctx_mutex.
 * - it must \em not hold kbasep_js_device_data::runpool_mutex (as this will be
 * obtained internally)
 * - it must \em not hold the kbase_device::as[n].transaction_mutex (as this will be obtained internally)
 *
 */
void kbasep_js_runpool_release_ctx( kbase_device *kbdev, kbase_context *kctx );

/**
 * @brief Try to submit the next job on a \b particular slot whilst in IRQ
 * context, and whilst the caller already holds the job-slot IRQ spinlock.
 *
 * \a *submit_count will be checked against
 * KBASE_JS_MAX_JOB_SUBMIT_PER_SLOT_PER_IRQ to see whether too many jobs have
 * been submitted. This is to prevent the IRQ handler looping over lots of GPU
 * NULL jobs, which may complete whilst the IRQ handler is still processing. \a
 * submit_count itself should point to kbase_device::slot_submit_count_irq[ \a js ],
 * which is initialized to zero on entry to the IRQ handler.
 *
 * The following locks must be held by the caller:
 * - kbasep_js_device_data::runpool_irq::lock
 * - kbdev->jm_slots[ \a js ].lock
 *
 * @return truthful (i.e. != MALI_FALSE) if there was space to submit in the
 * GPU, but we couldn't get a job from the Run Pool. This may be because the
 * Run Pool needs maintenence outside of IRQ context. Therefore, this indicates
 * that submission should be retried from a work-queue, by using
 * kbasep_js_try_run_next_job_on_slot().
 * @return MALI_FALSE if submission had no problems: the GPU is either already
 * full of jobs in the HEAD and NEXT registers, or we were able to get enough
 * jobs from the Run Pool to fill the GPU's HEAD and NEXT registers.
 */
mali_bool kbasep_js_try_run_next_job_on_slot_irq_nolock( kbase_device *kbdev, int js, s8 *submit_count );

/**
 * @brief Try to submit the next job on a particular slot, outside of IRQ context
 *
 * This obtains the Job Slot lock for the duration of the call only.
 *
 * Unlike kbasep_js_try_run_next_job_on_slot_irq_nolock(), there is no limit on
 * submission, because eventually IRQ_THROTTLE will kick in to prevent us
 * getting stuck in a loop of submitting GPU NULL jobs. This is because the IRQ
 * handler will be delayed, and so this function will eventually fill up the
 * space in our software 'submitted' slot (kbase_jm_slot::submitted).
 *
 * In addition, there's no return value - we'll run the maintenence functions
 * on the Policy's Run Pool, but if there's nothing there after that, then the
 * Run Pool is truely empty, and so no more action need be taken.
 *
 * The following locking conditions are made on the caller:
 * - it must hold kbasep_js_device_data::runpool_mutex
 * - it must \em not hold kbasep_js_device_data::runpool_irq::lock (as this will be
 * obtained internally)
 * - it must \em not hold kbdev->jm_slots[ \a js ].lock (as this will be
 * obtained internally)
 *
 * @note The caller \em might be holding one of the
 * kbasep_js_kctx_info::ctx::jsctx_mutex locks.
 *
 */
void kbasep_js_try_run_next_job_on_slot( kbase_device *kbdev, int js );

/**
 * @brief Try to schedule the next context onto the Run Pool
 *
 * This checks whether there's space in the Run Pool to accommodate a new
 * context. If so, it attempts to dequeue a context from the Policy Queue, and
 * submit this to the Run Pool.
 *
 * If the scheduling succeeds, then it also makes a call to
 * kbasep_js_try_run_next_job(), in case the new context has jobs matching the
 * job slot requirements, but no other currently scheduled context has such
 * jobs.
 *
 * If any of these actions fail (Run Pool Full, Policy Queue empty, etc) then
 * the function just returns normally.
 * 
 * The following locking conditions are made on the caller:
 * - it must \em not hold the kbasep_js_device_data::runpool_irq::lock, because
 * it will be used internally.
 * - it must \em not hold kbasep_js_device_data::runpool_mutex (as this will be
 * obtained internally)
 * - it must \em not hold the kbase_device::as[n].transaction_mutex (as this will be obtained internally)
 * - it must \em not hold kbasep_jd_device_data::queue_mutex (again, it's used internally).
 * - it must \em not hold kbasep_js_kctx_info::ctx::jsctx_mutex, because it will
 * be used internally.
 * - it must \em not hold kbdev->jm_slots[ \a js ].lock (as this will be
 * obtained internally)
 *
 */
void kbasep_js_try_schedule_head_ctx( kbase_device *kbdev );

/**
 * @brief Handle the Job Scheduler component for the IRQ of a job finishing
 *
 * This atomically does the following:
 * - updates the runpool's notion of time spent by a running ctx
 * - determines whether a context should be marked for scheduling out
 * - tries to submit the next job on the slot (picking from all ctxs in the runpool)
 *
 * In addition, if submission from IRQ failed, then this sets a message on
 * katom that submission needs to be retried from the worker thread.
 *
 * NOTE: It's possible to move the first two steps (inc calculating job's time
 * used) into the worker (outside of IRQ context), but this may allow a context
 * to use up to twice as much timeslice as is allowed by the policy. For
 * policies that order by time spent, this is not a problem for overall
 * 'fairness', but can still increase latency between contexts.
 *
 * The following locking conditions are made on the caller:
 * - it must \em not hold the kbasep_js_device_data::runpoool_irq::lock, because
 * it will be used internally.
 * - it must hold kbdev->jm_slots[ \a s ].lock
 */
void kbasep_js_job_done_slot_irq( kbase_device *kbdev, int s, kbase_jd_atom *katom, kbasep_js_tick *end_timestamp );

/*
 * Helpers follow
 */

/**
 * @brief Check that a context is allowed to submit jobs on this policy
 *
 * The purpose of this abstraction is to hide the underlying data size, and wrap up
 * the long repeated line of code.
 *
 * As with any mali_bool, never test the return value with MALI_TRUE.
 *
 * The caller must hold kbasep_js_device_data::runpool_irq::lock.
 */
static INLINE mali_bool kbasep_js_is_submit_allowed( kbasep_js_device_data *js_devdata, kbase_context *kctx )
{
	u16 test_bit;

	/* Ensure context really is scheduled in */
	OSK_ASSERT( kctx->as_nr != KBASEP_AS_NR_INVALID );
	OSK_ASSERT( kctx->jctx.sched_info.ctx.is_scheduled != MALI_FALSE );

	test_bit = (u16)(1u << kctx->as_nr);

	return (mali_bool)(js_devdata->runpool_irq.submit_allowed & test_bit);
}

/**
 * @brief Allow a context to submit jobs on this policy
 *
 * The purpose of this abstraction is to hide the underlying data size, and wrap up
 * the long repeated line of code.
 *
 * The caller must hold kbasep_js_device_data::runpool_irq::lock.
 */
static INLINE void kbasep_js_set_submit_allowed( kbasep_js_device_data *js_devdata, kbase_context *kctx )
{
	u16 set_bit;

	/* Ensure context really is scheduled in */
	OSK_ASSERT( kctx->as_nr != KBASEP_AS_NR_INVALID );
	OSK_ASSERT( kctx->jctx.sched_info.ctx.is_scheduled != MALI_FALSE );

	set_bit = (u16)(1u << kctx->as_nr);

	OSK_PRINT_INFO(OSK_BASE_JM, "JS: Setting Submit Allowed on %p (as=%d)", kctx, kctx->as_nr );

	js_devdata->runpool_irq.submit_allowed |= set_bit;
}

/**
 * @brief Prevent a context from submitting more jobs on this policy
 *
 * The purpose of this abstraction is to hide the underlying data size, and wrap up
 * the long repeated line of code.
 *
 * The caller must hold kbasep_js_device_data::runpool_irq::lock.
 */
static INLINE void kbasep_js_clear_submit_allowed( kbasep_js_device_data *js_devdata, kbase_context *kctx )
{
	u16 clear_bit;
	u16 clear_mask;

	/* Ensure context really is scheduled in */
	OSK_ASSERT( kctx->as_nr != KBASEP_AS_NR_INVALID );
	OSK_ASSERT( kctx->jctx.sched_info.ctx.is_scheduled != MALI_FALSE );

	clear_bit = (u16)(1u << kctx->as_nr);
	clear_mask = ~clear_bit;

	OSK_PRINT_INFO(OSK_BASE_JM, "JS: Clearing Submit Allowed on %p (as=%d)", kctx, kctx->as_nr );

	js_devdata->runpool_irq.submit_allowed &= clear_mask;
}

/**
 * @brief Manage the 'retry_submit_on_slot' part of a kbase_jd_atom
 */
static INLINE void kbasep_js_clear_job_retry_submit( kbase_jd_atom *atom )
{
	atom->retry_submit_on_slot = -1;
}

static INLINE mali_bool kbasep_js_get_job_retry_submit_slot( kbase_jd_atom *atom, int *res )
{
	int js = atom->retry_submit_on_slot;
	*res = js;
	return (mali_bool)( js >= 0 );
}

static INLINE void kbasep_js_set_job_retry_submit_slot( kbase_jd_atom *atom, int js )
{
	OSK_ASSERT( 0 <= js && js <= BASE_JM_MAX_NR_SLOTS );

	atom->retry_submit_on_slot = js;
}

#if OSK_DISABLE_ASSERTS == 0
/**
 * Debug Check the refcount of a context. Only use within ASSERTs
 *
 * Obtains kbasep_js_device_data::runpool_irq::lock
 *
 * @return negative value if the context is not scheduled in
 * @return current refcount of the context if it is scheduled in. The refcount
 * is not guarenteed to be kept constant.
 */
static INLINE int kbasep_js_debug_check_ctx_refcount( kbase_device *kbdev, kbase_context *kctx )
{
	kbasep_js_device_data *js_devdata;
	int result = -1;
	int as_nr;

	OSK_ASSERT( kbdev != NULL );
	OSK_ASSERT( kctx != NULL );
	js_devdata = &kbdev->js_data;

	osk_spinlock_irq_lock( &js_devdata->runpool_irq.lock );
	as_nr = kctx->as_nr;
	if ( as_nr != KBASEP_AS_NR_INVALID )
	{
		result = js_devdata->runpool_irq.per_as_data[as_nr].as_busy_refcount;
	}
	osk_spinlock_irq_unlock( &js_devdata->runpool_irq.lock );

	return result;
}
#endif /* OSK_DISABLE_ASSERTS == 0 */

/**
 * @brief Variant of kbasep_js_runpool_lookup_ctx() that can be used when the
 * context is guarenteed to be already previously retained.
 *
 * It is a programming error to supply the \a as_nr of a context that has not
 * been previously retained/has a busy refcount of zero. The only exception is
 * when there is no ctx in \a as_nr (NULL returned).
 *
 * The following locking conditions are made on the caller:
 * - it must \em not hold the kbasep_js_device_data::runpoool_irq::lock, because
 * it will be used internally.
 *
 * @return a valid kbase_context on success, with a refcount that is guarenteed
 * to be non-zero and unmodified by this function.
 * @return NULL on failure, indicating that no context was found in \a as_nr
 */
static INLINE kbase_context* kbasep_js_runpool_lookup_ctx_noretain( kbase_device *kbdev, int as_nr )
{
	kbasep_js_device_data *js_devdata;
	kbase_context *found_kctx;
	kbasep_js_per_as_data *js_per_as_data;

	OSK_ASSERT( kbdev != NULL );
	OSK_ASSERT( 0 <= as_nr && as_nr < BASE_MAX_NR_AS );
	js_devdata = &kbdev->js_data;
	js_per_as_data = &js_devdata->runpool_irq.per_as_data[as_nr];

	osk_spinlock_irq_lock( &js_devdata->runpool_irq.lock );

	found_kctx = js_per_as_data->kctx;
	OSK_ASSERT( found_kctx == NULL || js_per_as_data->as_busy_refcount > 0 );

	osk_spinlock_irq_unlock( &js_devdata->runpool_irq.lock );

	return found_kctx;
}


/**
 * @note MIDBASE-769: OSK to add high resolution timer
 */
static INLINE kbasep_js_tick kbasep_js_get_js_ticks( void )
{
	return osk_time_now();
}

/**
 * Supports about an hour worth of time difference, allows the underlying
 * clock to be more/less accurate than microseconds
 *
 * @note MIDBASE-769: OSK to add high resolution timer
 */
static INLINE u32 kbasep_js_convert_js_ticks_to_us( kbasep_js_tick js_tick )
{
	return (js_tick*10000u)/osk_time_mstoticks(10u);
}

/**
 * Supports about an hour worth of time difference, allows the underlying
 * clock to be more/less accurate than microseconds
 *
 * @note MIDBASE-769: OSK to add high resolution timer
 */
static INLINE kbasep_js_tick kbasep_js_convert_js_us_to_ticks( u32 us )
{
	return (us*osk_time_mstoticks(1000u))/1000000u;
}
/**
 * Determine if ticka comes after tickb
 *
 * @note MIDBASE-769: OSK to add high resolution timer
 */
static INLINE mali_bool kbasep_js_ticks_after( kbasep_js_tick ticka, kbasep_js_tick tickb )
{
	kbasep_js_tick tick_diff = ticka - tickb;
	const kbasep_js_tick wrapvalue = ((kbasep_js_tick)1u) << ((sizeof(kbasep_js_tick)*8)-1);

	return (mali_bool)(tick_diff < wrapvalue);
}

/**
 * This will provide a conversion from time (us) to ticks of the gpu clock
 * based on the minimum available gpu frequency.
 * This is usually good to compute best/worst case (where the use of current
 * frequency is not valid due to DVFS).
 * e.g.: when you need the number of cycles to guarantee you won't wait for
 * longer than 'us' time (you might have a shorter wait).
 */
static INLINE kbasep_js_gpu_tick kbasep_js_convert_us_to_gpu_ticks_min_freq( kbase_device *kbdev, u32 us )
{
	u32 gpu_freq = kbdev->gpu_props.props.core_props.gpu_freq_khz_min;
	OSK_ASSERT( 0!= gpu_freq );
	return (us * (gpu_freq / 1000));
}

/**
 * This will provide a conversion from time (us) to ticks of the gpu clock
 * based on the maximum available gpu frequency.
 * This is usually good to compute best/worst case (where the use of current
 * frequency is not valid due to DVFS).
 * e.g.: When you need the number of cycles to guarantee you'll wait at least
 * 'us' amount of time (but you might wait longer).
 */
static INLINE kbasep_js_gpu_tick kbasep_js_convert_us_to_gpu_ticks_max_freq( kbase_device *kbdev, u32 us )
{
	u32 gpu_freq = kbdev->gpu_props.props.core_props.gpu_freq_khz_max;
	OSK_ASSERT( 0!= gpu_freq );
	return (us * (gpu_freq / 1000));
}

/**
 * This will provide a conversion from ticks of the gpu clock to time (us)
 * based on the minimum available gpu frequency.
 * This is usually good to compute best/worst case (where the use of current
 * frequency is not valid due to DVFS).
 * e.g.: When you need to know the worst-case wait that 'ticks' cycles will
 * take (you guarantee that you won't wait any longer than this, but it may
 * be shorter).
 */
static INLINE u32 kbasep_js_convert_gpu_ticks_to_us_min_freq( kbase_device *kbdev, kbasep_js_gpu_tick ticks )
{
	u32 gpu_freq = kbdev->gpu_props.props.core_props.gpu_freq_khz_min;
	OSK_ASSERT( 0!= gpu_freq );
	return (ticks / gpu_freq * 1000);
}

/**
 * This will provide a conversion from ticks of the gpu clock to time (us)
 * based on the maximum available gpu frequency.
 * This is usually good to compute best/worst case (where the use of current
 * frequency is not valid due to DVFS).
 * e.g.: When you need to know the best-case wait for 'tick' cycles (you
 * guarantee to be waiting for at least this long, but it may be longer).
 */
static INLINE u32 kbasep_js_convert_gpu_ticks_to_us_max_freq( kbase_device *kbdev, kbasep_js_gpu_tick ticks )
{
	u32 gpu_freq = kbdev->gpu_props.props.core_props.gpu_freq_khz_max;
	OSK_ASSERT( 0!= gpu_freq );
	return (ticks / gpu_freq * 1000);
}
/** @} */ /* end group kbase_js */
/** @} */ /* end group base_kbase_api */
/** @} */ /* end group base_api */

#endif /* _KBASE_JS_H_ */