#include <stdbool.h>
#include <stdint.h>

#ifndef TINA_TASKS_H
#define TINA_TASKS_H

// Default to two priorities. (Max of 256)
// When processing, lower valued priorities are started before higher valued ones.
// Each priority is stored in a separately allocated queue and requires a linear search to find the next task.
#define TINA_TASKS_PRIORITIES 2
// Convenience priority enum.
enum {TINA_PRIORITY_HI, TINA_PRIORITY_LO};

typedef struct tina_tasks tina_tasks;
typedef struct tina_task tina_task;
typedef struct tina_group tina_group;

// Task function prototype.
typedef void tina_task_func(tina_task* task);

struct tina_task {
	// Task name. (optional)
	const char* name;
	// Task body function.
	tina_task_func* func;
	// Task context pointer.
	void* data;
	// Priority of the task. Must be less than TINA_TASKS_PRIORITIES.
	uint8_t priority;
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
	unsigned _count;
	tina_task* _task;
};

// Get the allocation size for a tasks instance.
size_t tina_tasks_size(size_t task_count, size_t coroutine_count, size_t stack_size);
// Initialize memory for a task system. Use tina_tasks_size() to figure out how much you need.
// Does not need to be destroyed, and the buffer can simply be deallocated when done.
tina_tasks* tina_tasks_init(void* buffer, size_t task_count, size_t coro_count, size_t stack_size);

// Execute tasks continuously on the current thread.
// Only returns if tina_tasks_pause() is called, or if the queue becomes empty and 'flush' is true.
// Run this on each of your worker threads.
void tina_tasks_run(tina_tasks* tasks, bool flush, void* thread_data);
// Pause execution of tasks on all threads as soon as their current tasks finish.
void tina_tasks_pause(tina_tasks* tasks);

// Add tasks to the task system, optional pass the address of a tina_group to track when the tasks have completed.
void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_group* group);
// Yield the current task until the group of tasks finish.
void tina_tasks_wait(tina_tasks* tasks, tina_task* task, tina_group* group);
// Similar to pthread_join() as a convenience method. Enqueues some tasks and waits on them.
void tina_tasks_join(tina_tasks* tasks, const tina_task* list, size_t count, tina_task* task);

#ifdef TINA_TASKS_IMPLEMENTATION

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

typedef struct {
	void** arr;
	size_t count;
} _tina_stack;

// Simple power of two circular queues.
typedef struct {
	void** arr;
	size_t head, tail, count, mask;
} _tina_queue;

struct tina_tasks {
	// Thread control variables.
	bool _pause;
	_TINA_MUTEX_T _lock;
	_TINA_SIGNAL_T _wakeup;
	
	// Keep the tasks and coroutine pools in a stack so recently used items are fresh in the cache.
	_tina_stack _coro, _pool;
	_tina_queue _task[TINA_TASKS_PRIORITIES];
};

static uintptr_t _tina_task_worker(tina* coro, uintptr_t value){
	tina_tasks* tasks = (tina_tasks*)coro->user_data;
	while(true){
		// Unlock the mutex while executing a task.
		_TINA_MUTEX_UNLOCK(tasks->_lock); {
			tina_task* task = (tina_task*)value;
			task->func(task);
		} _TINA_MUTEX_LOCK(tasks->_lock);
		
		// Yield true (task completed) back to the tasks system, and recieve the next task.
		value = tina_yield(coro, true);
	}
	
	// Unreachable.
	return 0;
}

size_t tina_tasks_size(size_t task_count, size_t coro_count, size_t stack_size){
	// Size of task.
	size_t size = sizeof(tina_tasks);
	// Size of queues.
	size += (coro_count + task_count + TINA_TASKS_PRIORITIES*task_count)*sizeof(void*);
	// Size of coroutines.
	size += coro_count*stack_size;
	// Size of tasks.
	size += task_count*sizeof(tina_task);
	return size;
}

tina_tasks* tina_tasks_init(void* buffer, size_t task_count, size_t coro_count, size_t stack_size){
	_TINA_ASSERT((task_count & (task_count - 1)) == 0, "Tina Task Error: Task count must be a power of two.");
	
	// Sub allocate all of the memory for the various arrays.
	tina_tasks* tasks = buffer;
	buffer += sizeof(tina_tasks);
	tasks->_coro = (_tina_stack){.arr = buffer};
	buffer += coro_count*sizeof(void*);
	tasks->_pool = (_tina_stack){.arr = buffer};
	buffer += task_count*sizeof(void*);
	for(unsigned i = 0; i < TINA_TASKS_PRIORITIES; i++){
		tasks->_task[i] = (_tina_queue){.arr = buffer, .mask = task_count - 1};
		buffer += task_count*sizeof(void*);
	}
	
	// Initialize the coroutines and fill the pool.
	tasks->_coro.count = coro_count;
	for(unsigned i = 0; i < coro_count; i++){
		tina* coro = tina_init(buffer, stack_size, _tina_task_worker, &tasks);
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

static inline tina_task* _tina_tasks_next_task(tina_tasks* tasks){
	// Linear search of the task queues in priority order until a task is found.
	for(unsigned i = 0; i < TINA_TASKS_PRIORITIES; i++){
		_tina_queue* queue = &tasks->_task[i];
		if(queue->count > 0){
			// Dequeue a task.
			queue->count--;
			return queue->arr[queue->tail++ & queue->mask];
		}
	}
	
	return NULL;
}

static void _tina_tasks_execute_task(tina_tasks* tasks, tina_task* task, void* thread_data){
	// Update the thread data.
	task->thread_data = thread_data;
	
	if(tina_yield(task->_coro, (uintptr_t)task)){
		// This task has completed. Return it to the pool.
		tasks->_pool.arr[tasks->_pool.count++] = task;
		tasks->_coro.arr[tasks->_coro.count++] = task->_coro;
		
		// Did it have a group, and was it the last task being waited for?
		tina_group* group = task->_group;
		if(group && --group->_count == 0) _tina_tasks_execute_task(tasks, group->_task, thread_data);
	}
}

void tina_tasks_run(tina_tasks* tasks, bool flush, void* thread_data){
	// Task loop is only unlocked while running a task or waiting for a wakeup.
	_TINA_MUTEX_LOCK(tasks->_lock); {
		tasks->_pause = false;
		while(!tasks->_pause){
			tina_task* task = _tina_tasks_next_task(tasks);
			if(task){
				_TINA_ASSERT(tasks->_coro.count > 0, "Tina Task Error: Ran out of coroutines.");
				// Assign a coroutine to the task and run it.
				task->_coro = tasks->_coro.arr[--tasks->_coro.count];
				_tina_tasks_execute_task(tasks, task, thread_data);
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

static void _tina_tasks_push_task(tina_tasks* tasks, tina_task copy){
	// Pop a task from the pool.
	tina_task* task = tasks->_pool.arr[--tasks->_pool.count];
	(*task) = copy;
	
	_tina_queue* queue = &tasks->_task[copy.priority];
	queue->arr[queue->head++ & queue->mask] = task;
	queue->count++;
}

void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_group* group){
	_TINA_MUTEX_LOCK(tasks->_lock); {
		if(group) group->_count += count;
		
		_TINA_ASSERT(tasks->_pool.count >= count, "Tina Task Error: Ran out of tasks.");
		for(size_t i = 0; i < count; i++){
			tina_task copy = list[i];
			copy._group = group;
			
			_TINA_ASSERT(copy.func, "Tina Tasks Error: Task must have a body function.");
			_TINA_ASSERT(copy.priority < TINA_TASKS_PRIORITIES, "Tina Tasks Error: Task priority is invalid.");
			_tina_tasks_push_task(tasks, copy);
		}
		_TINA_SIGNAL_BROADCAST(tasks->_wakeup);
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
}

void tina_tasks_wait(tina_tasks* tasks, tina_task* task, tina_group* group){
	// _TINA_ASSERT(group->_task == NULL, "Tina Task Error: Groups cannot be reused.");
	
	_TINA_MUTEX_LOCK(tasks->_lock); {
		group->_task = task;
		if(--group->_count > 0){
			// There are still tasks running. Yield false (task not complete) back to the tasks system.
			tina_yield(group->_task->_coro, false);
		}
		
		// Make the group ready to use again.
		group->_count++;
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
}

void tina_tasks_governor(tina_tasks* tasks, tina_task* task, tina_group* group, unsigned threshold){
	_TINA_MUTEX_LOCK(tasks->_lock); {
		group->_task = task;
		if(--group->_count > threshold){
			group->_count -= threshold;
			tina_yield(group->_task->_coro, false);
			group->_count += threshold;
		}
		group->_count++;
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
}

void tina_tasks_join(tina_tasks* tasks, const tina_task* list, size_t count, tina_task* task){
	tina_group group = {};
	tina_tasks_enqueue(tasks, list, count, &group);
	tina_tasks_wait(tasks, task, &group);
}

typedef struct {
	tina_tasks* tasks;
	tina_group* group;
	_TINA_SIGNAL_T wakeup;
} _tina_wakeup_context;

static void _tina_tasks_sleep_wakeup(tina_task* task){
	_tina_wakeup_context* ctx = task->data;
	tina_tasks_wait(ctx->tasks, task, ctx->group);
	
	_TINA_MUTEX_LOCK(ctx->tasks->_lock); {
		_TINA_SIGNAL_BROADCAST(ctx->wakeup);
	} _TINA_MUTEX_UNLOCK(ctx->tasks->_lock);
}

void tina_tasks_wait_sleep(tina_tasks* tasks, tina_group* group){
	_tina_wakeup_context ctx = {.tasks = tasks, .group = group};
	_TINA_SIGNAL_INIT(ctx.wakeup);
	
	_TINA_MUTEX_LOCK(tasks->_lock); {
		_TINA_ASSERT(tasks->_pool.count >= 1, "Tina Task Error: Ran out of tasks.");
		_tina_tasks_push_task(tasks, (tina_task){.func = _tina_tasks_sleep_wakeup, .data = &ctx});
		_TINA_SIGNAL_BROADCAST(tasks->_wakeup);
		_TINA_SIGNAL_WAIT(ctx.wakeup, tasks->_lock);
	} _TINA_MUTEX_UNLOCK(tasks->_lock);
	
	_TINA_SIGNAL_DESTROY(ctx.wakeup);
}

#endif // TINA_TASK_IMPLEMENTATION
#endif // TINA_TASKS_H
