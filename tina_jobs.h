/*
	Copyright (c) 2021 Scott Lembcke

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

#ifndef TINA_JOBS_H
#define TINA_JOBS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque type for a scheduler.
typedef struct tina_scheduler tina_scheduler;
// Opaque type for a job.
typedef struct tina_job tina_job;

// Job body function prototype.
typedef void tina_job_func(tina_job* job);

typedef struct {
	// Job name. (optional)
	const char* name;
	// Job body function.
	tina_job_func* func;
	// User defined job context pointer. (optional)
	void* user_data;
	// User defined job index. (optional, useful for parallel-for constructs)
	uintptr_t user_idx;
	// Index of the queue to run the job on.
	unsigned queue_idx;
} tina_job_description;

// Get the scheduler for a job.
tina_scheduler* tina_job_get_scheduler(tina_job* job);
// Get the description for a job.
const tina_job_description* tina_job_get_description(tina_job* job);

// Counter used to signal when a group of jobs is done.
// Note: Must be zero-initialized before use.
typedef struct {
	// Private:
	tina_job* _job_list;
	unsigned _count;
} tina_group;

// Get the allocation size for a scheduler instance.
size_t tina_scheduler_size(unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size);
// Initialize memory for a scheduler. Use tina_scheduler_size() to figure out how much you need.
tina_scheduler* tina_scheduler_init(void* buffer, unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size);
// Destroy a scheduler. Any unfinished jobs will be lost. Flush your queues if you need them to finish gracefully.
void tina_scheduler_destroy(tina_scheduler* sched);

#ifndef TINA_NO_CRT
// Convenience constructor. Allocate and initialize a scheduler.
tina_scheduler* tina_scheduler_new(unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size);
// Convenience destructor. Destroy and free a scheduler.
void tina_scheduler_free(tina_scheduler* sched);
#endif

// Link a pair of queues for job prioritization. When the 'queue_idx' is empty it will steal jobs from 'fallback_idx'.
void tina_scheduler_queue_priority(tina_scheduler* sched, unsigned queue_idx, unsigned fallback_idx);

typedef enum {
	TINA_RUN_LOOP, // Run jobs from a queue until tina_scheduler_interrupt() is called.
	TINA_RUN_FLUSH, // Run jobs from a queue until empty, or until all remaing jobs are waiting.
	TINA_RUN_SINGLE, // Run a single non-waiting job from a queue.
} tina_run_mode;

// Run jobs in the given queue based on the mode, returns false if no jobs were run.
bool tina_scheduler_run(tina_scheduler* sched, unsigned queue_idx, tina_run_mode mode);
// Interrupt TINA_RUN_LOOP execution of a queue on all active threads as soon as their current jobs finish.
void tina_scheduler_interrupt(tina_scheduler* sched, unsigned queue_idx);

// Add jobs to the scheduler, optionally pass the address of a tina_group to track when the jobs have completed.
// If 'max_group_count' is non-zero, then 'count' will be adjusted based on the number of jobs already in the group.
// Returns the number of jobs added.
unsigned tina_scheduler_enqueue_batch(tina_scheduler* sched, const tina_job_description* list, unsigned count, tina_group* group, unsigned max_group_count);
// Yield the current job until the group has 'threshold' or fewer remaining jobs.
// 'threshold' is useful to throttle a producer job. Allowing it to keep a consumers busy without a lot of queued items.
unsigned tina_job_wait(tina_job* job, tina_group* group, unsigned threshold);
// Yield the current job and reschedule at the back of the queue.
void tina_job_yield(tina_job* job);
// Yield the current job and reschedule it at the back of a different queue.
// Returns the old queue the job was scheduled on.
unsigned tina_job_switch_queue(tina_job* job, unsigned queue_idx);

// Increment a group's value directly. Allows associating jobs (or some other unit of work) with multiple groups.
// Returns the count added which will be adjusted similarly to tina_scheduler_enqueue_batch().
unsigned tina_group_increment(tina_scheduler* scheduler, tina_group* group, unsigned count, unsigned max_count);
// Decrement a group's value directly to manually mark completion of some work.
void tina_group_decrement(tina_scheduler* scheduler, tina_group* group, unsigned count);

// Convenience method. Enqueue a single job.
static inline void tina_scheduler_enqueue(tina_scheduler* sched, const char* name, tina_job_func* func, void* user_data, uintptr_t user_idx, unsigned queue_idx, tina_group* group){
	tina_job_description desc = {.name = name, .func = func, .user_data = user_data, .user_idx = user_idx, .queue_idx = queue_idx};
	tina_scheduler_enqueue_batch(sched, &desc, 1, group, 0);
}

#ifdef TINA_JOBS_IMPLEMENTATION

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

#ifndef _TINA_PROFILE_ENTER
#define _TINA_PROFILE_ENTER(_JOB_)
#define _TINA_PROFILE_LEAVE(_JOB_, _STATUS_)
#endif

struct tina_job {
	tina_job_description desc;
	void* user_data;
	tina* fiber;
	tina_group* group;
	tina_job* wait_next;
	unsigned wait_threshold;
};

tina_scheduler* tina_job_get_scheduler(tina_job* job){return (tina_scheduler*)job->fiber->user_data;}
const tina_job_description* tina_job_get_description(tina_job* job){return &job->desc;}

typedef struct {
	void** arr;
	size_t count;
} _tina_stack;

// Simple power of two circular queues.
typedef struct _tina_queue _tina_queue;
struct _tina_queue{
	void** arr;
	size_t head, tail, mask;
	
	// Higher priority queue in the chain. Used for signaling worker threads.
	_tina_queue* parent;
	// Lower priority queue in the chain. Used as a fallback when this queue is empty.
	_tina_queue* fallback;
	// Semaphore to wait for more work in this queue.
	_TINA_COND_T semaphore_signal;
	unsigned semaphore_count;
	// Incremented each time the queue is interrupted.
	unsigned interrupt_stamp;
};

struct tina_scheduler {
	_TINA_MUTEX_T _lock;
	
	_tina_queue* _queues;
	size_t _queue_count;
	
	// Keep the jobs and fiber pools in a stack so recently used items are fresh in the cache.
	_tina_stack _fibers, _job_pool;
};

typedef enum {
	_TINA_STATUS_COMPLETED,
	_TINA_STATUS_WAITING,
	_TINA_STATUS_YIELDING,
} _tina_job_status;

static uintptr_t _tina_jobs_fiber(tina* fiber, uintptr_t value){
	while(true){
		tina_job* job = (tina_job*)value;
		job->desc.func(job);
		value = tina_yield(fiber, _TINA_STATUS_COMPLETED);
	}
	
	return 0; // Unreachable.
}

static inline size_t _tina_jobs_align(size_t n){return -(-n & -_TINA_MAX_ALIGN);}

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

typedef tina* _tina_fiber_factory(tina_scheduler* sched, unsigned fiber_idx, void* buffer, size_t stack_size, void* user_ptr);

static tina* _tina_jobs_default_fiber_factory(tina_scheduler* sched, unsigned fiber_idx, void* buffer, size_t stack_size, void* factory_data){
	return tina_init(buffer, stack_size, (tina_func*)factory_data, sched);
}

static tina_scheduler* _tina_scheduler_init2(void* buffer, unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size, _tina_fiber_factory* fiber_factory, void* factory_data){
	_TINA_ASSERT((job_count & (job_count - 1)) == 0, "Tina Jobs Error: Job count must be a power of two.");
	_TINA_ASSERT((stack_size & (stack_size - 1)) == 0, "Tina Jobs Error: Stack size must be a power of two.");
	uint8_t* cursor = (uint8_t*)buffer;
	
	// Sub allocate all of the memory for the various arrays.
	tina_scheduler* sched = (tina_scheduler*)cursor;
	cursor += _tina_jobs_align(sizeof(tina_scheduler));
	sched->_queues = (_tina_queue*)cursor;
	cursor += _tina_jobs_align(queue_count*sizeof(_tina_queue));
	sched->_fibers = (_tina_stack){.arr = (void**)cursor, .count = 0};
	cursor += _tina_jobs_align(fiber_count*sizeof(void*));
	sched->_job_pool = (_tina_stack){.arr = (void**)cursor, .count = 0};
	cursor += _tina_jobs_align(job_count*sizeof(void*));
	
	// Initialize the queues arrays.
	sched->_queue_count = queue_count;
	for(unsigned i = 0; i < queue_count; i++){
		_tina_queue* queue = &sched->_queues[i];
		queue->arr = (void**)cursor;
		queue->head = queue->tail = 0;
		queue->mask = job_count - 1;
		queue->parent = queue->fallback = NULL;
		_TINA_COND_INIT(queue->semaphore_signal);
		queue->semaphore_count = 0;
		
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
		sched->_fibers.arr[i] = fiber_factory(sched, i, cursor, stack_size, factory_data);
		cursor += stack_size;
	}
	
	_TINA_MUTEX_INIT(sched->_lock);
	return sched;
}

tina_scheduler* tina_scheduler_init(void* buffer, unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size){
	return _tina_scheduler_init2(buffer, job_count, queue_count, fiber_count, stack_size, _tina_jobs_default_fiber_factory, (void*)_tina_jobs_fiber);
}

void tina_scheduler_destroy(tina_scheduler* sched){
	_TINA_MUTEX_DESTROY(sched->_lock);
	for(unsigned i = 0; i < sched->_queue_count; i++) _TINA_COND_DESTROY(sched->_queues[i].semaphore_signal);
}

#ifndef TINA_NO_CRT
tina_scheduler* tina_scheduler_new(unsigned job_count, unsigned queue_count, unsigned fiber_count, size_t stack_size){
	void* buffer = malloc(tina_scheduler_size(job_count, queue_count, fiber_count, stack_size));
	return tina_scheduler_init(buffer, job_count, queue_count, fiber_count, stack_size);
}

void tina_scheduler_free(tina_scheduler* sched){
	tina_scheduler_destroy(sched);
	free(sched);
}
#endif

static inline _tina_queue* _tina_get_queue(tina_scheduler* sched, unsigned queue_idx){
	_TINA_ASSERT(queue_idx < sched->_queue_count, "Tina Jobs Error: Invalid queue index.");
	return &sched->_queues[queue_idx];
}

void tina_scheduler_queue_priority(tina_scheduler* sched, unsigned queue_idx, unsigned fallback_idx){
	_tina_queue* parent = _tina_get_queue(sched, queue_idx);
	_tina_queue* fallback = _tina_get_queue(sched, fallback_idx);
	_TINA_ASSERT(!parent->fallback, "Tina Jobs Error: Queue already has a fallback assigned.");
	_TINA_ASSERT(!fallback->parent, "Tina Jobs Error: Queue already has a fallback assigned.");

	parent->fallback = fallback;
	fallback->parent = parent;
}

static tina_job* _tina_queue_next_job(_tina_queue* queue){
	if(queue->head != queue->tail){
		return (tina_job*)queue->arr[queue->tail++ & queue->mask];
	} else if(queue->fallback){
		return _tina_queue_next_job(queue->fallback);
	} else {
		return NULL;
	}
}

static void _tina_queue_signal(_tina_queue* queue){
	if(queue->semaphore_count){
		_TINA_COND_SIGNAL(queue->semaphore_signal);
		queue->semaphore_count--;
	} else if(queue->parent){
		_tina_queue_signal(queue->parent);
	}
}

static tina_job* _tina_group_process_wait_list(tina_scheduler* sched, tina_group* group, tina_job* job){
	if(job){
		tina_job* next = _tina_group_process_wait_list(sched, group, job->wait_next);
		if(group->_count <= job->wait_threshold){
			// Push the waiting job to the back of it's queue.
			_tina_queue* queue = &sched->_queues[job->desc.queue_idx];
			queue->arr[queue->head++ & queue->mask] = job;
			_tina_queue_signal(queue);
			
			// Unlink from wait list.
			job->wait_next = NULL;
			return next;
		} else {
			job->wait_next = next;
		}
	}
	
	return job;
}

static inline unsigned _tina_group_increment(tina_group* group, unsigned count, unsigned max_count){
	if(max_count > 0){
		// Handle already full.
		if(group->_count >= max_count) return 0;
		// Adjust count.
		unsigned remaining = max_count - group->_count;
		if(count > remaining) count = remaining;
	}
	
	group->_count += count;
	return count;
}

static inline void _tina_group_decrement(tina_scheduler* sched, tina_group* group, unsigned count){
	group->_count -= count;
	group->_job_list = _tina_group_process_wait_list(sched, group, group->_job_list);
}

static inline void _tina_scheduler_execute_job(tina_scheduler* sched, tina_job* job){
	_TINA_ASSERT(sched->_fibers.count > 0, "Tina Jobs Error: Ran out of fibers.");
	// Assign a fiber and the thread data. (Jobs that are resuming already have a fiber)
	if(job->fiber == NULL) job->fiber = (tina*)sched->_fibers.arr[--sched->_fibers.count];
	
	// Unlock the scheduler while executing the job. Fibers re-lock it before yielding back.
	_TINA_MUTEX_UNLOCK(sched->_lock);
	
	_TINA_PROFILE_ENTER(job);
	_tina_job_status status = (_tina_job_status)tina_resume(job->fiber, (uintptr_t)job);
	_TINA_PROFILE_LEAVE(job, status);
	
	switch(status){
		case _TINA_STATUS_COMPLETED: {
			_TINA_MUTEX_LOCK(sched->_lock);
			// Return the components to the pools.
			sched->_job_pool.arr[sched->_job_pool.count++] = job;
			sched->_fibers.arr[sched->_fibers.count++] = job->fiber;
			
			// Did it have a group, and was it the last job being waited for?
			tina_group* group = job->group;
			if(group) _tina_group_decrement(sched, group, 1);
		} break;
		case _TINA_STATUS_YIELDING:{
			_TINA_MUTEX_LOCK(sched->_lock);
			// Push the job to the back of the queue.
			_tina_queue* queue = &sched->_queues[job->desc.queue_idx];
			queue->arr[queue->head++ & queue->mask] = job;
			_tina_queue_signal(queue);
		} break;
		case _TINA_STATUS_WAITING: {
			// Do nothing. The job will be re-enqueued when it's done waiting.
			// tina_job_wait() locks the scheduler before yielding.
		} break;
	}
}

bool tina_scheduler_run(tina_scheduler* sched, unsigned queue_idx, tina_run_mode mode){
	bool ran = false;
	_TINA_MUTEX_LOCK(sched->_lock); {
		_tina_queue* queue = _tina_get_queue(sched, queue_idx);
		
		// Keep looping until the interrupt stamp is incremented.
		unsigned stamp = queue->interrupt_stamp;
		while(mode != TINA_RUN_LOOP || queue->interrupt_stamp == stamp){
			tina_job* job = _tina_queue_next_job(queue);
			if(job){
				_tina_scheduler_execute_job(sched, job);
				ran = true;
				if(mode == TINA_RUN_SINGLE) break;
			} else if(mode == TINA_RUN_LOOP){
				// Sleep until more work is added to the queue.
				queue->semaphore_count++;
				_TINA_COND_WAIT(queue->semaphore_signal, sched->_lock);
			} else {
				break;
			}
		}
	} _TINA_MUTEX_UNLOCK(sched->_lock);
	return ran;
}

void tina_scheduler_interrupt(tina_scheduler* sched, unsigned queue_idx){
	_TINA_MUTEX_LOCK(sched->_lock); {
		_tina_queue* queue = _tina_get_queue(sched, queue_idx);
		queue->interrupt_stamp++;
		
		_TINA_COND_BROADCAST(queue->semaphore_signal);
		queue->semaphore_count = 0;
	} _TINA_MUTEX_UNLOCK(sched->_lock);
}

unsigned tina_scheduler_enqueue_batch(tina_scheduler* sched, const tina_job_description* list, unsigned count, tina_group* group, unsigned max_group_count){
	_TINA_MUTEX_LOCK(sched->_lock); {
		if(group) count = _tina_group_increment(group, count, max_group_count);
		
		_TINA_ASSERT(sched->_job_pool.count >= count, "Tina Jobs Error: Ran out of jobs.");
		for(size_t i = 0; i < count; i++){
			_TINA_ASSERT(list[i].func, "Tina Jobs Error: Job must have a body function.");
			
			// Pop a job from the pool.
			tina_job* job = (tina_job*)sched->_job_pool.arr[--sched->_job_pool.count];
			(*job) = (tina_job){.desc = list[i], .user_data = NULL, .fiber = NULL, .group = group, .wait_next = NULL, .wait_threshold = 0};
			
			// Push it to the proper queue.
			_tina_queue* queue = _tina_get_queue(sched, list[i].queue_idx);
			queue->arr[queue->head++ & queue->mask] = job;
			_tina_queue_signal(queue);
		}
	} _TINA_MUTEX_UNLOCK(sched->_lock);
	
	return count;
}

unsigned tina_job_wait(tina_job* job, tina_group* group, unsigned threshold){
	_TINA_MUTEX_LOCK(tina_job_get_scheduler(job)->_lock);
	
	// Check if we need to wait at all.
	unsigned count = group->_count;
	if(count > threshold){
		// Push onto wait list.
		job->wait_next = group->_job_list;
		group->_job_list = job;
		
		job->wait_threshold = threshold;
		// NOTE: Scheduler will be unlocked after yielding.
		tina_yield(job->fiber, _TINA_STATUS_WAITING);
		job->wait_threshold = 0;
		
		return group->_count;
	} else {
		_TINA_MUTEX_UNLOCK(tina_job_get_scheduler(job)->_lock);
		return count;
	}
}

void tina_job_yield(tina_job* job){
	tina_yield(job->fiber, _TINA_STATUS_YIELDING);
}

unsigned tina_job_switch_queue(tina_job* job, unsigned queue_idx){
	unsigned old_queue = job->desc.queue_idx;
	if(queue_idx == old_queue) return queue_idx;
	
	job->desc.queue_idx = queue_idx;
	tina_yield(job->fiber, _TINA_STATUS_YIELDING);
	return old_queue;
}

unsigned tina_group_increment(tina_scheduler* scheduler, tina_group* group, unsigned count, unsigned max_count){
	_TINA_MUTEX_LOCK(scheduler->_lock);
	count = _tina_group_increment(group, count, max_count);
	_TINA_MUTEX_UNLOCK(scheduler->_lock);
	return count;
}

void tina_group_decrement(tina_scheduler* scheduler, tina_group* group, unsigned count){
	_TINA_MUTEX_LOCK(scheduler->_lock);
	_TINA_ASSERT(group->_count >= count, "Tina Jobs Error: Group count underflow.");
	_tina_group_decrement(scheduler, group, count);
	_TINA_MUTEX_UNLOCK(scheduler->_lock);
}

#endif // TINA_JOB_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // TINA_JOBS_H
