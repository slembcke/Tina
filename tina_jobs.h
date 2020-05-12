/*
Copyright (c) 2019 Scott Lembcke

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdalign.h>

#ifndef TINA_JOBS_H
#define TINA_JOBS_H

#ifdef __cplusplus
extern "C" {
#endif

// Opaque type for a scheduler.
typedef struct tina_scheduler tina_scheduler;
// Opaque type for a job.
typedef struct tina_job tina_job;
// Opaque type for a job group.
typedef struct tina_group tina_group;

// Job function prototype.
// 'job' is a reference to the job to use with the yield/switch/abort functions.
// 'user_data' is the pointer you supplied when creating the job.
// 'thread_data' is a handle for the pointer passed to the tina_scheduler_run() call that is processing this job.
typedef void tina_job_func(tina_job* job, void* user_data, void** thread_data);

typedef struct {
	// Job name. (optional)
	const char* name;
	// Job body function.
	tina_job_func* func;
	// Job context pointer. (optional)
	void* user_data;
	// Index of the queue to run the job on.
	uint8_t queue_idx;
} tina_job_description;

// Counter used to signal when a group of jobs is done.
// Can be allocated anywhere (stack, in an object, etc), and does not need to be freed.
struct tina_group {
	tina_job* _job;
	uint32_t _count;
	uint32_t _magic;
};

// Get the allocation size for a jobs instance.
size_t tina_scheduler_size(unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size);
// Initialize memory for a scheduler. Use tina_scheduler_size() to figure out how much you need.
tina_scheduler* tina_scheduler_init(void* buffer, unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size);
// Destroy a scheduler. Any unfinished jobs will be lost. Flush your queues if you need them to finish gracefully.
void tina_scheduler_destroy(tina_scheduler* sched);

// Convenience constructor. Allocate and initialize a scheduler.
tina_scheduler* tina_scheduler_new(unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size);
// Convenience destructor. Destroy and free a scheduler.
void tina_scheduler_free(tina_scheduler* sched);

// Set link a pair of queues for job prioritization. When the main queue is empty it will steal jobs from the fallback.
void tina_scheduler_queue_priority(tina_scheduler* sched, unsigned queue_idx, unsigned fallback_idx);

// Execute jobs continuously on the current thread.
// Only returns if tina_scheduler_pause() is called, or if the queue becomes empty and 'flush' is true.
// You can run this continuously on worker threads or use it to explicitly flush certain queues.
// 'thread_data' is a user context pointer passed through tina_job.thread_data to provide thread local functionality such as memory pooling.
void tina_scheduler_run(tina_scheduler* sched, unsigned queue_idx, bool flush, void* thread_data);
// Pause execution of jobs on all threads as soon as their current jobs finish.
void tina_scheduler_pause(tina_scheduler* sched);

// Groups must be initialized before use.
void tina_group_init(tina_group* group);

// Add jobs to the scheduler, optionally pass the address of a tina_group to track when the jobs have completed.
void tina_scheduler_enqueue_batch(tina_scheduler* sched, const tina_job_description* list, size_t count, tina_group* group);
// Add jobs to the scheduler, but don't allow more than 'max_count' jobs in 'group'. Returns the number of jobs added.
size_t tina_scheduler_enqueue_throttled(tina_scheduler* sched, const tina_job_description* list, size_t count, tina_group* group, size_t max_count);
// Yield the current job until the group has 'threshold' or less remaining jobs.
// 'threshold' is useful to throttle a producer job. Allowing it to keep a pipeline full without overflowing it.
void tina_job_wait(tina_job* job, tina_group* group, unsigned threshold);
// Yield the current job and reschedule it to run again later.
void tina_job_yield(tina_job* job);
// Yield the current job and reschedule it to run on a different queue.
void tina_job_switch_queue(tina_job* job, unsigned queue_idx);
// Immediately abort the execution of a job and mark it as completed.
void tina_job_abort(tina_job* job);

// NOTE: tina_job_yield() and tina_job_abort() must be called from within the actual job.
// Very bad, stack corrupting things will happen if you call it from the outside.

// Convenience method. Enqueue a single job.
static inline void tina_scheduler_enqueue(tina_scheduler* sched, const char* name, tina_job_func* func, void* user_data, unsigned queue_idx, tina_group* group){
	tina_job_description desc = {.name = name, .func = func, .user_data = user_data, .queue_idx = (uint8_t)queue_idx};
	tina_scheduler_enqueue_batch(sched, &desc, 1, group);
}
// Convenience method. Enqueue some jobs and wait for them all to finish.
void tina_scheduler_join(tina_scheduler* sched, const tina_job_description* list, size_t count, tina_job* job);

// Like 'tina_job_wait()' but for external threads. Blocks the current thread until the threshold is satisfied.
// Don't run this from a job! It will block the runner thread and probably cause a deadlock.
void tina_scheduler_wait_blocking(tina_scheduler* sched, tina_group* group, unsigned threshold);


#ifdef TINA_JOBS_IMPLEMENTATION

// Minimum alignment when packing allocations.
#define _TINA_JOBS_MIN_ALIGN 16

// Override these. Based on C11 primitives.
// Save yourself some trouble and grab https://github.com/tinycthread/tinycthread
#ifndef _TINA_MUTEX_T
#define _TINA_MUTEX_T mtx_t
#define _TINA_MUTEX_INIT(_LOCK_) mtx_init(&_LOCK_, mtx_plain)
#define _TINA_MUTEX_DESTROY(_LOCK_) mtx_destroy(&_LOCK_)
#define _TINA_MUTEX_LOCK(_LOCK_) mtx_lock(&_LOCK_)
#define _TINA_MUTEX_UNLOCK(_LOCK_) mtx_unlock(&_LOCK_)
#define _TINA_COND_T cnd_t
#define _TINA_COND_INIT(_SIG_) cnd_init(&_SIG_)
#define _TINA_COND_DESTROY(_SIG_) cnd_destroy(&_SIG_)
#define _TINA_COND_WAIT(_SIG_, _LOCK_) cnd_wait(&_SIG_, &_LOCK_);
#define _TINA_COND_SIGNAL(_SIG_) cnd_signal(&_SIG_)
#define _TINA_COND_BROADCAST(_SIG_) cnd_broadcast(&_SIG_)
#endif

struct tina_job {
	tina_job_description desc;
	tina_scheduler* scheduler;
	tina* fiber;
	void* thread_data;
	tina_group* group;
};

typedef struct {
	void** arr;
	size_t count;
} _tina_stack;

// Simple power of two circular queues.
typedef struct _tina_queue _tina_queue;
struct _tina_queue{
	void** arr;
	_tina_queue* fallback;
	size_t head, tail, count, mask;
};

struct tina_scheduler {
	// Thread control variables.
	bool _pause;
	_TINA_MUTEX_T _lock;
	_TINA_COND_T _wakeup;
	
	_tina_queue* _queues;
	size_t _queue_count;
	
	// Keep the jobs and fiber pools in a stack so recently used items are fresh in the cache.
	_tina_stack _fibers, _job_pool;
};

enum _TINA_STATUS {
	_TINA_STATUS_COMPLETE,
	_TINA_STATUS_WAITING,
	_TINA_STATUS_YIELDING,
	_TINA_STATUS_ABORTED,
};

static uintptr_t _tina_jobs_fiber(tina* fiber, uintptr_t value){
	tina_scheduler* sched = (tina_scheduler*)fiber->user_data;
	while(true){
		// Unlock the mutex while executing a job.
		_TINA_MUTEX_UNLOCK(sched->_lock); {
			tina_job* job = (tina_job*)value;
			job->desc.func(job, job->desc.user_data, &job->thread_data);
		} _TINA_MUTEX_LOCK(sched->_lock);
		
		// Yield the completed status back to the scheduler, and recieve the next job.
		value = tina_yield(fiber, _TINA_STATUS_COMPLETE);
	}
	
	// Unreachable.
	return 0;
}

static inline uint _tina_jobs_align(size_t n){return ((n - 1)/_TINA_JOBS_MIN_ALIGN + 1)*_TINA_JOBS_MIN_ALIGN;}

size_t tina_scheduler_size(unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size){
	size_t size = 0;
	// Size of scheduler.
	size += _tina_jobs_align(sizeof(tina_scheduler));
	// Size of queues.
	size += _tina_jobs_align(queue_count*sizeof(_tina_queue));
	// Size of fiber pool array.
	size += _tina_jobs_align(fiber_count*sizeof(void*));
	// Size of job pool array.
	size += _tina_jobs_align(job_count*sizeof(void*));
	// Size of queue arrays.
	size += queue_count*_tina_jobs_align(job_count*sizeof(void*));
	// Size of jobs.
	size += job_count*_tina_jobs_align(sizeof(tina_job));
	// Size of fibers.
	size += fiber_count*stack_size;
	return size;
}

tina_scheduler* tina_scheduler_init(void* _buffer, unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size){
	_TINA_ASSERT((job_count & (job_count - 1)) == 0, "Tina Jobs Error: Job count must be a power of two.");
	_TINA_ASSERT((stack_size & (stack_size - 1)) == 0, "Tina Jobs Error: Stack size must be a power of two.");
	uint8_t* cursor = (uint8_t*)_buffer;
	
	// Sub allocate all of the memory for the various arrays.
	tina_scheduler* sched = (tina_scheduler*)cursor;
	cursor += _tina_jobs_align(sizeof(tina_scheduler));
	sched->_queues = (_tina_queue*)cursor;
	cursor += _tina_jobs_align(queue_count*sizeof(_tina_queue));
	sched->_fibers = (_tina_stack){.arr = (void**)cursor};
	cursor += _tina_jobs_align(fiber_count*sizeof(void*));
	sched->_job_pool = (_tina_stack){.arr = (void**)cursor};
	cursor += _tina_jobs_align(job_count*sizeof(void*));
	
	// Initialize the queues arrays.
	sched->_queue_count = queue_count;
	for(unsigned i = 0; i < queue_count; i++){
		sched->_queues[i] = (_tina_queue){.arr = (void**)cursor, .mask = job_count - 1};
		cursor += _tina_jobs_align(job_count*sizeof(void*));
	}
	
	// Fill the job pool.
	sched->_job_pool.count = job_count;
	for(unsigned i = 0; i < job_count; i++){
		sched->_job_pool.arr[i] = cursor;
		cursor += _tina_jobs_align(sizeof(tina_job));
	}
	
	// Initialize the fibers and fill the pool.
	sched->_fibers.count = fiber_count;
	for(unsigned i = 0; i < fiber_count; i++){
		tina* fiber = tina_init(cursor, stack_size, _tina_jobs_fiber, &sched);
		fiber->name = "TINA JOB FIBER";
		fiber->user_data = sched;
		sched->_fibers.arr[i] = fiber;
		cursor += stack_size;
	}
	
	// Initialize the control variables.
	_TINA_MUTEX_INIT(sched->_lock);
	_TINA_COND_INIT(sched->_wakeup);
	
	return sched;
}

void tina_scheduler_destroy(tina_scheduler* sched){
	_TINA_MUTEX_DESTROY(sched->_lock);
	_TINA_COND_DESTROY(sched->_wakeup);
}

tina_scheduler* tina_scheduler_new(unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size){
	void* buffer = malloc(tina_scheduler_size(job_count, queue_count, fiber_count, stack_size));
	return tina_scheduler_init(buffer, job_count, queue_count, fiber_count, stack_size);
}

void tina_scheduler_free(tina_scheduler* sched){
	tina_scheduler_destroy(sched);
	free(sched);
}

void tina_scheduler_queue_priority(tina_scheduler* sched, unsigned queue_idx, unsigned fallback_idx){
	_TINA_ASSERT(queue_idx < sched->_queue_count, "Tina Jobs Error: Invalid queue index.");
	_TINA_ASSERT(fallback_idx < sched->_queue_count, "Tina Jobs Error: Invalid queue index.");
	
	_tina_queue* queue = &sched->_queues[queue_idx];
	_TINA_ASSERT(queue->fallback == NULL, "Tina Jobs Error: Queue already has a fallback assigned.");
	
	queue->fallback = &sched->_queues[fallback_idx];
}

static inline tina_job* _tina_scheduler_next_job(tina_scheduler* sched, _tina_queue* queue){
	if(queue->count > 0){
		queue->count--;
		return (tina_job*)queue->arr[queue->tail++ & queue->mask];
	} else if(queue->fallback){
		return _tina_scheduler_next_job(sched, queue->fallback);
	} else {
		return NULL;
	}
}

void tina_scheduler_run(tina_scheduler* sched, unsigned queue_idx, bool flush, void* thread_data){
	// Job loop is only unlocked while running a job or waiting for a wakeup.
	_TINA_MUTEX_LOCK(sched->_lock); {
		sched->_pause = false;
		
		_TINA_ASSERT(queue_idx < sched->_queue_count, "Tina Jobs Error: Invalid queue index.");
		_tina_queue* queue = &sched->_queues[queue_idx];
		
		// If not in flush mode, keep looping until the scheduler is paused.
		while(flush || !sched->_pause){
			tina_job* job = _tina_scheduler_next_job(sched, queue);
			if(job){
				_TINA_ASSERT(sched->_fibers.count > 0, "Tina Jobs Error: Ran out of fibers.");
				// Assign a fiber and the thread data. (Jobs that are resuming already have a fiber)
				if(job->fiber == NULL) job->fiber = (tina*)sched->_fibers.arr[--sched->_fibers.count];
				job->thread_data = thread_data;
				
				// Yield to the job's fiber to run it.
				switch(tina_yield(job->fiber, (uintptr_t)job)){
					case _TINA_STATUS_ABORTED: {
						// Worker fiber state not reset with a clean exit. Need to do it explicitly.
						tina_init(job->fiber, job->fiber->size, _tina_jobs_fiber, sched);
					}; // FALLTHROUGH
					case _TINA_STATUS_COMPLETE: {
						// Return the components to the pools.
						sched->_job_pool.arr[sched->_job_pool.count++] = job;
						sched->_fibers.arr[sched->_fibers.count++] = job->fiber;
						
						// Did it have a group, and was it the last job being waited for?
						tina_group* group = job->group;
						if(group && --group->_count == 0){
							// Push the waiting job to the front of it's queue.
							_tina_queue* queue = &sched->_queues[group->_job->desc.queue_idx];
							queue->arr[--queue->tail & queue->mask] = group->_job;
							queue->count++;
							_TINA_COND_SIGNAL(sched->_wakeup);
							// TODO is pushing it to the front the best thing to do?
						}
					} break;
					case _TINA_STATUS_YIELDING:{
						// Push the job to the back of the queue.
						_tina_queue* queue = &sched->_queues[job->desc.queue_idx];
						queue->arr[queue->head++ & queue->mask] = job;
						queue->count++;
						_TINA_COND_SIGNAL(sched->_wakeup);
					} break;
					case _TINA_STATUS_WAITING: {
						// Do nothing. The job will be re-enqueued when it's done waiting.
					} break;
				}
			} else if(flush){
				// No more tasks so we are done if run in flush mode.
				break;
			} else {
				// Sleep until more work is added to the queue.
				_TINA_COND_WAIT(sched->_wakeup, sched->_lock);
			}
		}
	} _TINA_MUTEX_UNLOCK(sched->_lock);
}

void tina_scheduler_pause(tina_scheduler* sched){
	_TINA_MUTEX_LOCK(sched->_lock); {
		sched->_pause = true;
		_TINA_COND_BROADCAST(sched->_wakeup);
	} _TINA_MUTEX_UNLOCK(sched->_lock);
}

void tina_group_init(tina_group* group){
	// Count is initailized to 1 because tina_job_wait() also decrements the count for symmetry reasons.
	(*group) = (tina_group){._count = 1, ._magic = _TINA_MAGIC};
}

static void _tina_scheduler_enqueue_batch_nolock(tina_scheduler* sched, const tina_job_description* list, size_t count, tina_group* group){
	if(group){
		_TINA_ASSERT(group->_magic == _TINA_MAGIC, "Tina Jobs Error: Group is corrupt or uninitialized");
		group->_count += count;
	}
	
	_TINA_ASSERT(sched->_job_pool.count >= count, "Tina Jobs Error: Ran out of jobs.");
	for(size_t i = 0; i < count; i++){
		_TINA_ASSERT(list[i].func, "Tina Jobs Error: Job must have a body function.");
		_TINA_ASSERT(list[i].queue_idx < sched->_queue_count, "Tina Jobs Error: Invalid queue index.");
		
		// Pop a job from the pool.
		tina_job* job = (tina_job*)sched->_job_pool.arr[--sched->_job_pool.count];
		(*job) = (tina_job){.desc = list[i], .scheduler = sched, .group = group};
		
		// Push it to the proper queue.
		_tina_queue* queue = &sched->_queues[list[i].queue_idx];
		queue->arr[queue->head++ & queue->mask] = job;
		queue->count++;
		_TINA_COND_SIGNAL(sched->_wakeup);
	}
}

void tina_scheduler_enqueue_batch(tina_scheduler* sched, const tina_job_description* list, size_t count, tina_group* group){
	_TINA_MUTEX_LOCK(sched->_lock); {
		_tina_scheduler_enqueue_batch_nolock(sched, list, count, group);
	} _TINA_MUTEX_UNLOCK(sched->_lock);
}

size_t tina_scheduler_enqueue_throttled(tina_scheduler* sched, const tina_job_description* list, size_t count, tina_group* group, size_t max_count){
	_TINA_MUTEX_LOCK(sched->_lock); {
		if(group->_count < max_count){
			// Adjust count if necessary.
			size_t allowed = max_count - group->_count;
			if(count > allowed) count = allowed;
			_tina_scheduler_enqueue_batch_nolock(sched, list, count, group);
		} else {
			// Group is already full. Can't enqueue any jobs.
			count = 0;
		}
	} _TINA_MUTEX_UNLOCK(sched->_lock);
	
	return count;
}

void tina_job_wait(tina_job* job, tina_group* group, unsigned threshold){
	tina_scheduler* sched = job->scheduler;
	_TINA_MUTEX_LOCK(sched->_lock); {
		_TINA_ASSERT(group->_magic == _TINA_MAGIC, "Tina Jobs Error: Group is corrupt or uninitialized");
		group->_job = job;
		
		// Check if we need to wait at all.
		if(--group->_count > threshold){
			group->_count -= threshold;
			// Yield until the counter hits zero.
			tina_yield(group->_job->fiber, _TINA_STATUS_WAITING);
			// Restore the counter for the remaining jobs.
			group->_count += threshold;
		}
		
		// Make the group ready to use again.
		group->_count++;
		group->_job = NULL;
	} _TINA_MUTEX_UNLOCK(sched->_lock);
}

void tina_job_yield(tina_job* job){
	tina_scheduler* sched = job->scheduler;
	_TINA_MUTEX_LOCK(sched->_lock); {
		tina_yield(job->fiber, _TINA_STATUS_YIELDING);
	} _TINA_MUTEX_UNLOCK(sched->_lock);
}

void tina_job_switch_queue(tina_job* job, unsigned queue_idx){
	tina_scheduler* sched = job->scheduler;
	_TINA_MUTEX_LOCK(sched->_lock); {
		job->desc.queue_idx = queue_idx;
		tina_yield(job->fiber, _TINA_STATUS_YIELDING);
	} _TINA_MUTEX_UNLOCK(sched->_lock);
}

void tina_job_abort(tina_job* job){
	tina_scheduler* sched = job->scheduler;
	_TINA_MUTEX_LOCK(sched->_lock); {
		tina_yield(job->fiber, _TINA_STATUS_ABORTED);
	} _TINA_MUTEX_UNLOCK(sched->_lock);
}

void tina_scheduler_join(tina_scheduler* sched, const tina_job_description* list, size_t count, tina_job* job){
	tina_group group; tina_group_init(&group);
	tina_scheduler_enqueue_batch(sched, list, count, &group);
	tina_job_wait(job, &group, 0);
}

typedef struct {
	tina_group* group;
	unsigned threshold;
	_TINA_COND_T wakeup;
} _tina_wakeup_ctx;

static void _tina_scheduler_sleep_wakeup(tina_job* job, void* user_data, void** thread_data){
	_tina_wakeup_ctx* ctx = (_tina_wakeup_ctx*)user_data;
	tina_job_wait(job, ctx->group, ctx->threshold);
	
	_TINA_MUTEX_LOCK(job->scheduler->_lock); {
		_TINA_COND_SIGNAL(ctx->wakeup);
	} _TINA_MUTEX_UNLOCK(job->scheduler->_lock);
}

void tina_scheduler_wait_blocking(tina_scheduler* sched, tina_group* group, unsigned threshold){
	_tina_wakeup_ctx ctx = {.group = group, .threshold = threshold};
	_TINA_COND_INIT(ctx.wakeup);
	
	_TINA_MUTEX_LOCK(sched->_lock);_TINA_MUTEX_LOCK(sched->_lock); {
		tina_job_description desc = {.func = _tina_scheduler_sleep_wakeup, .user_data = &ctx};
		_tina_scheduler_enqueue_batch_nolock(sched, &desc, 1, group);
		_TINA_COND_WAIT(ctx.wakeup, sched->_lock);
	} _TINA_MUTEX_UNLOCK(sched->_lock);
	
	_TINA_COND_DESTROY(ctx.wakeup);
}

#endif // TINA_JOB_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // TINA_JOBS_H
