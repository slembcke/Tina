#include <stdbool.h>
#include <stdint.h>

#ifndef TINA_TASKS_H
#define TINA_TASKS_H

typedef struct tina_tasks tina_tasks;
typedef struct tina_task tina_task;
typedef struct tina_group tina_group;

// Task function prototype.
typedef void tina_task_func(tina_tasks* tasks, tina_task* task);

struct tina_task {
	// Task name. (optional)
	const char* name;
	// Task body function.
	tina_task_func* func;
	// Task context pointer. (optional)
	void* user_data;
	// Index of the queue to run the task on.
	uint8_t queue_idx;
	
	// Context pointer passed to the tina_tasks_run(). (readonly)
	// Intended to be used like thread local storage, but without needing a global variable.
	void* thread_data;
	
	// Private fields.
	tina* _coro;
	tina_group* _group;
};

// Counter used to signal when a group of tasks is done.
// Can be allocated anywhere (stack, in an object, etc), and does not need to be freed.
struct tina_group {
	tina_task* _task;
	uint32_t _count;
	uint32_t _magic;
};

// Get the allocation size for a tasks instance.
size_t tina_tasks_size(unsigned task_count, unsigned queue_count, unsigned coroutine_count, size_t stack_size);
// Initialize memory for a task system. Use tina_tasks_size() to figure out how much you need.
tina_tasks* tina_tasks_init(void* buffer, unsigned task_count, unsigned queue_count, unsigned coroutine_count, size_t stack_size);
// Destroy a task system. Any unfinished tasks will be lost. Flush your queues if you need them to finish gracefully.
void tina_tasks_destroy(tina_tasks* tasks);

// Convenience constructor. Allocate and initialize a task system.
tina_tasks* tina_tasks_new(unsigned task_count, unsigned queue_count, unsigned coroutine_count, size_t stack_size);
// Convenience destructor. Destroy and free a task system.
void tina_tasks_free(tina_tasks* tasks);

// Set link a pair of queues for task prioritization. When the main queue is empty it will steal tasks from the fallback.
void tina_tasks_queue_priority(tina_tasks* tasks, unsigned queue_idx, unsigned fallback_idx);

// Execute tasks continuously on the current thread.
// Only returns if tina_tasks_pause() is called, or if the queue becomes empty and 'flush' is true.
// You can run this continuously on worker threads or use it to explicitly flush certain queues.
// 'thread_data' is a user context pointer passed through tina_task.thread_data to provide thread local functionality such as memory pooling.
void tina_tasks_run(tina_tasks* tasks, unsigned queue_idx, bool flush, void* thread_data);
// Pause execution of tasks on all threads as soon as their current tasks finish.
void tina_tasks_pause(tina_tasks* tasks);

// Groups must be initialized before use.
void tina_group_init(tina_group* group);

// Add tasks to the task system, optional pass the address of a tina_group to track when the tasks have completed.
void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_group* group);
// Yield the current task until the group has 'threshold' or less remaining tasks.
// 'threshold' is useful to throttle a producer task. Allowing it to keep a pipeline full without overflowing it.
void tina_tasks_wait(tina_tasks* tasks, tina_task* task, tina_group* group, unsigned threshold);
// Yield the current task and reschedule it to run again later.
void tina_tasks_yield_current(tina_tasks* tasks, tina_task* task);
// Immediately abort the execution of a task and mark it as completed.
void tina_tasks_abort_current(tina_tasks* tasks, tina_task* task);

// NOTE: tina_tasks_yield_current() and tina_tasks_abort_current() must be called from within the actual task.
// Very bad, stack corrupting things will happen if you call it from the outside.

// Convenience method. Enqueue some tasks and wait for them all to finish.
void tina_tasks_join(tina_tasks* tasks, const tina_task* list, size_t count, tina_task* task);

// Like 'tina_tasks_wait()' but for external threads. Blocks the current thread until the threshold is satisfied.
// Don't run this from a task! It will block the runner thread and probably cause a deadlock.
void tina_tasks_wait_blocking(tina_tasks* tasks, tina_group* group, unsigned threshold);

#ifdef TINA_TASKS_IMPLEMENTATION

// TODO associate groups with tasks?
// TODO tina_tasks_run take a number of tasks to run maybe?

// Override these. Based on C11 primitives.
#ifndef _TINA_MUTEX_T
#define _TINA_MUTEX_T mtx_t
#define _TINA_MUTEX_INIT(_LOCK_) mtx_init(&_LOCK_, mtx_plain)
#define _TINA_MUTEX_DESTROY(_LOCK_) mtx_destroy(&_LOCK_)
#define _TINA_MUTEX_LOCK(_LOCK_) mtx_lock(&_LOCK_)
#define _TINA_MUTEX_UNLOCK(_LOCK_) mtx_unlock(&_LOCK_)
#define _TINA_SIGNAL_T cnd_t
#define _TINA_SIGNAL_INIT(_SIG_) cnd_init(&_SIG_)
#define _TINA_SIGNAL_DESTROY(_SIG_) cnd_destroy(&_SIG_)
#define _TINA_SIGNAL_WAIT(_SIG_, _LOCK_) cnd_wait(&_SIG_, &_LOCK_);
#define _TINA_SIGNAL_BROADCAST(_SIG_) cnd_broadcast(&_SIG_)
#endif

// TODO This probably doesn't compile as C++.
// TODO allocate and set up queues explicitly.

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

struct tina_tasks {
	// Thread control variables.
	bool _pause;
	_TINA_MUTEX_T _lock;
	_TINA_SIGNAL_T _wakeup;
	
	_tina_queue* _queues;
	size_t _queue_count;
	
	// Keep the tasks and coroutine pools in a stack so recently used items are fresh in the cache.
	_tina_stack _coro, _pool;
};

enum _TINA_STATUS {
	_TINA_STATUS_COMPLETE,
	_TINA_STATUS_WAITING,
	_TINA_STATUS_YIELDING,
	_TINA_STATUS_ABORTED,
};

static uintptr_t _tina_tasks_worker(tina* coro, uintptr_t value){
	tina_tasks* tasks = (tina_tasks*)coro->user_data;
	while(true){
		// Unlock the mutex while executing a task.
		_TINA_MUTEX_UNLOCK(tasks->_lock); {
			tina_task* task = (tina_task*)value;
			task->func(tasks, task);
		} _TINA_MUTEX_LOCK(tasks->_lock);
		
		// Yield true (task completed) back to the tasks system, and recieve the next task.
		value = tina_yield(coro, _TINA_STATUS_COMPLETE);
	}
	
	// Unreachable.
	return 0;
}

size_t tina_tasks_size(unsigned task_count, unsigned queue_count, unsigned coroutine_count, size_t stack_size){
	// Size of task.
	size_t size = sizeof(tina_tasks);
	// Size of queues.
	size += queue_count*sizeof(_tina_queue);
	// Size of queue arrays.
	size += queue_count*task_count*sizeof(void*);
	// Size of stack arrays for the pools.
	size += (task_count + coroutine_count)*sizeof(void*);
	// Size of coroutines.
	size += coroutine_count*stack_size;
	// Size of tasks.
	size += task_count*sizeof(tina_task);
	return size;
}

tina_tasks* tina_tasks_init(void* buffer, unsigned task_count, unsigned queue_count, unsigned coroutine_count, size_t stack_size){
	_TINA_ASSERT((task_count & (task_count - 1)) == 0, "Tina Task Error: Task count must be a power of two.");
	
	// Sub allocate all of the memory for the various arrays.
	tina_tasks* tasks = buffer;
	buffer += sizeof(tina_tasks);
	tasks->_queues = buffer;
	buffer += queue_count*sizeof(_tina_queue);
	tasks->_coro = (_tina_stack){.arr = buffer};
	buffer += coroutine_count*sizeof(void*);
	tasks->_pool = (_tina_stack){.arr = buffer};
	buffer += task_count*sizeof(void*);
	
	// Initialize the queues.
	tasks->_queue_count = queue_count;
	for(unsigned i = 0; i < queue_count; i++){
		tasks->_queues[i] = (_tina_queue){.arr = buffer, .mask = task_count - 1};
		buffer += task_count*sizeof(void*);
	}
	
	// Initialize the coroutines and fill the pool.
	tasks->_coro.count = coroutine_count;
	for(unsigned i = 0; i < coroutine_count; i++){
		tina* coro = tina_init(buffer, stack_size, _tina_tasks_worker, &tasks);
		coro->name = "TINA TASK WORKER";
		coro->user_data = tasks;
		tasks->_coro.arr[i] = coro;
		buffer += stack_size;
	}
	
	// Fill the task pool.
	tasks->_pool.count = task_count;
	for(unsigned i = 0; i < task_count; i++){
		tasks->_pool.arr[i] = buffer;
		buffer += sizeof(tina_task);
	}
	
	// Initialize the control variables.
	_TINA_MUTEX_INIT(tasks->_lock);
	_TINA_SIGNAL_INIT(tasks->_wakeup);
	
	return tasks;
}

void tina_tasks_destroy(tina_tasks* tasks){
	_TINA_MUTEX_DESTROY(tasks->_lock);
	_TINA_SIGNAL_DESTROY(tasks->_wakeup);
}

tina_tasks* tina_tasks_new(unsigned task_count, unsigned queue_count, unsigned coroutine_count, size_t stack_size){
	void* tasks_buffer = malloc(tina_tasks_size(task_count, queue_count, coroutine_count, stack_size));
	return tina_tasks_init(tasks_buffer, task_count, queue_count, coroutine_count, stack_size);
}

void tina_tasks_free(tina_tasks* tasks){
	tina_tasks_destroy(tasks);
	free(tasks);
}

void tina_tasks_queue_priority(tina_tasks* tasks, unsigned queue_idx, unsigned fallback_idx){
	_TINA_ASSERT(queue_idx < tasks->_queue_count, "Tina Tasks Error: Invalid queue index.");
	_TINA_ASSERT(fallback_idx < tasks->_queue_count, "Tina Tasks Error: Invalid queue index.");
	
	_tina_queue* queue = &tasks->_queues[queue_idx];
	_TINA_ASSERT(queue->fallback == NULL, "Tina Tasks Error: Queue already has a fallback assigned.");
	
	queue->fallback = &tasks->_queues[fallback_idx];
}

static inline tina_task* _tina_tasks_next_task(tina_tasks* tasks, _tina_queue* queue){
	if(queue->count > 0){
		queue->count--;
		return queue->arr[queue->tail++ & queue->mask];
	} else if(queue->fallback){
		return _tina_tasks_next_task(tasks, queue->fallback);
	} else {
		return NULL;
	}
}

void tina_tasks_run(tina_tasks* tasks, unsigned queue_idx, bool flush, void* thread_data){
	// Task loop is only unlocked while running a task or waiting for a wakeup.
	_TINA_MUTEX_LOCK(tasks->_lock); {
		tasks->_pause = false;
		
		_TINA_ASSERT(queue_idx < tasks->_queue_count, "Tina Task Error: Invalid queue index.");
		_tina_queue* queue = &tasks->_queues[queue_idx];
		while(!tasks->_pause){
			tina_task* task = _tina_tasks_next_task(tasks, queue);
			if(task){
				_TINA_ASSERT(tasks->_coro.count > 0, "Tina Task Error: Ran out of coroutines.");
				// Assign a coroutine and the thread data.
				if(task->_coro == NULL) task->_coro = tasks->_coro.arr[--tasks->_coro.count];
				task->thread_data = thread_data;
				
				// Yield to the task's coroutine to run it.
				switch(tina_yield(task->_coro, (uintptr_t)task)){
					case _TINA_STATUS_ABORTED: {
						// Worker coroutine state not reset with a clean exit. Need to do it explicitly.
						tina_init(task->_coro, task->_coro->size, _tina_tasks_worker, tasks);
					}; // FALLTHROUGH
					case _TINA_STATUS_COMPLETE: {
						// This task has completed. Return it to the pool.
						tasks->_pool.arr[tasks->_pool.count++] = task;
						tasks->_coro.arr[tasks->_coro.count++] = task->_coro;
						
						// Did it have a group, and was it the last task being waited for?
						tina_group* group = task->_group;
						if(group && --group->_count == 0){
							// Push the waiting task to the front of it's queue.
							_tina_queue* queue = &tasks->_queues[group->_task->queue_idx];
							queue->arr[--queue->tail & queue->mask] = group->_task;
							queue->count++;
						}
					} break;
					case _TINA_STATUS_YIELDING:{
						// Push the task to the back of the queue.
						_tina_queue* queue = &tasks->_queues[task->queue_idx];
						queue->arr[queue->head++ & queue->mask] = task;
						queue->count++;
					} break;
					case _TINA_STATUS_WAITING: {
						// Do nothing. The task will be re-enqueued when it's done waiting.
					} break;
				}
			} else if(flush){
				break;
			} else {
				_TINA_SIGNAL_WAIT(tasks->_wakeup, tasks->_lock);
			}
		}
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
}

void tina_tasks_pause(tina_tasks* tasks){
	_TINA_MUTEX_LOCK(tasks->_lock); {
		tasks->_pause = true;
		_TINA_SIGNAL_BROADCAST(tasks->_wakeup);
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
}

void tina_group_init(tina_group* group){
	// Count is initailized to 1 because tina_tasks_wait() also decrements the count for symmetry reasons.
	(*group) = (tina_group){._count = 1, ._magic = _TINA_MAGIC};
}

static void _tina_tasks_enqueue_nolock(tina_tasks* tasks, const tina_task* list, size_t count, tina_group* group){
	if(group){
		_TINA_ASSERT(group->_magic == _TINA_MAGIC, "Tina Tasks Error: Group is corrupt or uninitialized");
		group->_count += count;
	}
	
	_TINA_ASSERT(tasks->_pool.count >= count, "Tina Task Error: Ran out of tasks.");
	for(size_t i = 0; i < count; i++){
		tina_task copy = list[i];
		copy._group = group;
		
		_TINA_ASSERT(copy.func, "Tina Tasks Error: Task must have a body function.");
		_TINA_ASSERT(copy.queue_idx < tasks->_queue_count, "Tina Tasks Error: Invalid queue index.");
		
		// Pop a task from the pool.
		tina_task* task = tasks->_pool.arr[--tasks->_pool.count];
		(*task) = copy;
		
		// Push it to the proper queue.
		_tina_queue* queue = &tasks->_queues[copy.queue_idx];
		queue->arr[queue->head++ & queue->mask] = task;
		queue->count++;
	}
	_TINA_SIGNAL_BROADCAST(tasks->_wakeup);
}

void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_group* group){
	_TINA_MUTEX_LOCK(tasks->_lock); {
		_tina_tasks_enqueue_nolock(tasks, list, count, group);
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
}

void tina_tasks_wait(tina_tasks* tasks, tina_task* task, tina_group* group, unsigned threshold){
	_TINA_MUTEX_LOCK(tasks->_lock); {
		_TINA_ASSERT(group->_magic == _TINA_MAGIC, "Tina Tasks Error: Group is corrupt or uninitialized");
		group->_task = task;
		
		if(--group->_count > threshold){
			group->_count -= threshold;
			// Yield until the counter hits zero.
			tina_yield(group->_task->_coro, _TINA_STATUS_WAITING);
			// Restore the counter for the remaining tasks.
			group->_count += threshold;
		}
		
		// Make the group ready to use again.
		group->_count++;
		group->_task = NULL;
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
}

void tina_tasks_yield_current(tina_tasks* tasks, tina_task* task){
	_TINA_MUTEX_LOCK(tasks->_lock); {
		tina_yield(task->_coro, _TINA_STATUS_YIELDING);
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
}

void tina_tasks_abort_current(tina_tasks* tasks, tina_task* task){
	_TINA_MUTEX_LOCK(tasks->_lock); {
		tina_yield(task->_coro, _TINA_STATUS_ABORTED);
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
}

void tina_tasks_join(tina_tasks* tasks, const tina_task* list, size_t count, tina_task* task){
	tina_group group; tina_group_init(&group);
	tina_tasks_enqueue(tasks, list, count, &group);
	tina_tasks_wait(tasks, task, &group, 0);
}

typedef struct {
	tina_group* group;
	unsigned threshold;
	_TINA_SIGNAL_T wakeup;
} _tina_wakeup_context;

static void _tina_tasks_sleep_wakeup(tina_tasks* tasks, tina_task* task){
	_tina_wakeup_context* ctx = task->user_data;
	tina_tasks_wait(tasks, task, ctx->group, ctx->threshold);
	
	_TINA_MUTEX_LOCK(tasks->_lock); {
		_TINA_SIGNAL_BROADCAST(ctx->wakeup);
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
}

void tina_tasks_wait_blocking(tina_tasks* tasks, tina_group* group, unsigned threshold){
	_tina_wakeup_context ctx = {.group = group, .threshold = threshold};
	_TINA_SIGNAL_INIT(ctx.wakeup);
	
	_TINA_MUTEX_LOCK(tasks->_lock);_TINA_MUTEX_LOCK(tasks->_lock); {
		_tina_tasks_enqueue_nolock(tasks, &(tina_task){.func = _tina_tasks_sleep_wakeup, .user_data = &ctx}, 1, group);
		_TINA_SIGNAL_WAIT(ctx.wakeup, tasks->_lock);
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
	
	_TINA_SIGNAL_DESTROY(ctx.wakeup);
}

#endif // TINA_TASK_IMPLEMENTATION
#endif // TINA_TASKS_H
