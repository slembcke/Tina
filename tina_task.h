#include <stdint.h>
#include <string.h>

#include "tina.h"

#ifndef TINA_TASK_H
#define TINA_TASK_H

#define TINA_TASK_PRIORITIES 1

typedef struct tina_tasks tina_tasks;
typedef struct tina_task tina_task;
typedef struct tina_counter tina_counter;
typedef void tina_task_func(tina_task* task);

struct tina_task {
	const char* name;
	tina_task_func* func;
	void* ptr;
	uint8_t priority;
	
	tina* _coro;
	tina_counter* _counter;
};

struct tina_counter {
	unsigned count;
	tina_task* task;
};

size_t tina_tasks_size(size_t task_count, size_t coro_count, size_t stack_size);
tina_tasks* tina_tasks_init(void* buffer, size_t task_count, size_t coro_count, size_t stack_size);
void tina_tasks_worker_loop(tina_tasks* tasks);
void tina_tasks_shutdown(tina_tasks* tasks);

void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_counter* counter);
void tina_tasks_wait(tina_tasks* tasks, tina_task* task, tina_counter* counter);

#define TINA_ASSERT(_COND_, _MESSAGE_) {if(!(_COND_)){puts(_MESSAGE_); abort();}}

#define TINA_MUTEX_T mtx_t
#define TINA_MUTEX_INIT(_LOCK_) mtx_init(&_LOCK_, mtx_plain)
#define TINA_MUTEX_LOCK(_LOCK_) mtx_lock(&_LOCK_)
#define TINA_MUTEX_UNLOCK(_LOCK_) mtx_unlock(&_LOCK_)

#define TINA_SIGNAL_T cnd_t
#define TINA_SIGNAL_INIT(_SIG_) cnd_init(&_SIG_)
#define TINA_SIGNAL_WAIT(_SIG_, _LOCK_) cnd_wait(&_SIG_, &_LOCK_);
#define TINA_SIGNAL_BROADCAST(_SIG_) cnd_broadcast(&_SIG_)

#ifdef TINA_IMPLEMENTATION

// TODO Reduce pointer bloat?
// TODO What is a reasonable shutdown procedure?
// TODO This probably doesn't compile as C++.

typedef struct {
	void** arr;
	uint16_t head, tail, count, capacity;
} _tina_queue;

struct tina_tasks {
	bool request_shutdown;
	TINA_MUTEX_T lock;
	TINA_SIGNAL_T wakeup;
	_tina_queue coro, pool, task[TINA_TASK_PRIORITIES];
};

static inline void _tina_enqueue(_tina_queue* queue, void* elt){
	TINA_ASSERT(queue->count < queue->capacity, "Queue overflow");
	queue->count++;
	queue->arr[queue->head++ & (queue->capacity - 1)] = elt;
}

static inline void* _tina_dequeue(_tina_queue* queue){
	TINA_ASSERT(queue->count > 0, "Queue overflow.");
	queue->count--;
	return queue->arr[queue->tail++ & (queue->capacity - 1)];
}

static uintptr_t _task_body(tina* coro, uintptr_t value){
	tina_tasks* tasks = (tina_tasks*)coro->user_data;
	while(true){
		TINA_MUTEX_UNLOCK(tasks->lock);
		tina_task* task = (tina_task*)value;
		task->func(task);
		TINA_MUTEX_LOCK(tasks->lock);
		
		value = tina_yield(coro, true);
	}
}

size_t tina_tasks_size(size_t task_count, size_t coro_count, size_t stack_size){
	size_t size = sizeof(tina_tasks);
	size += (coro_count + task_count + TINA_TASK_PRIORITIES*task_count)*sizeof(void*);
	size += coro_count*stack_size;
	size += task_count*sizeof(tina_task);
	return size;
}

tina_tasks* tina_tasks_init(void* buffer, size_t task_count, size_t coro_count, size_t stack_size){
	tina_tasks* tasks = memset(buffer, 0, tina_tasks_size(task_count, coro_count, stack_size));
	buffer += sizeof(tina_tasks);
	tasks->coro = (_tina_queue){.arr = buffer, .capacity = coro_count};
	buffer += coro_count*sizeof(void*);
	tasks->pool = (_tina_queue){.arr = buffer, .capacity = task_count};
	buffer += task_count*sizeof(void*);
	for(unsigned i = 0; i < TINA_TASK_PRIORITIES; i++){
		tasks->task[i] = (_tina_queue){.arr = buffer, .capacity = task_count};
		buffer += task_count*sizeof(void*);
	}
	
	tasks->coro.count = coro_count;
	for(unsigned i = 0; i < coro_count; i++){
		tina* coro = tina_init(buffer, stack_size, _task_body, &tasks);
		coro->name = "TINA TASK WORKER";
		coro->user_data = tasks;
		tasks->coro.arr[i] = coro;
		buffer += stack_size;
	}
	
	tasks->pool.count = task_count;
	for(unsigned i = 0; i < task_count; i++){
		tasks->pool.arr[i] = buffer;
		buffer += sizeof(tina_task);
	}
	
	TINA_MUTEX_INIT(tasks->lock);
	TINA_SIGNAL_INIT(tasks->wakeup);
	
	return tasks;
}

void tina_tasks_shutdown(tina_tasks* tasks){
	tasks->request_shutdown = true;
	TINA_SIGNAL_BROADCAST(tasks->wakeup);
}

void tina_tasks_worker_loop(tina_tasks* tasks){
	TINA_MUTEX_LOCK(tasks->lock);
	while(true){
		if(tasks->request_shutdown) break;
		
		tina_task* task = NULL;
		for(unsigned i = 0; i < TINA_TASK_PRIORITIES; i++){
			if(tasks->task[i].count > 0){
				task = _tina_dequeue(&tasks->task[i]);
				break;
			}
		}
		
		if(task){
			task->_coro = _tina_dequeue(&tasks->coro);
			run_again: {
				if(tina_yield(task->_coro, (uintptr_t)task)){
					_tina_enqueue(&tasks->pool, task);
					_tina_enqueue(&tasks->coro, task->_coro);
					
					tina_counter* counter = task->_counter;
					if(counter){
						task = counter->task;
						// This was the last task the counter was waiting on. Wake up the waiting task.
						if(--counter->count == 0) goto run_again;
					}
				}
			}
		} else {
			TINA_SIGNAL_WAIT(tasks->wakeup, tasks->lock);
		}
	}
	TINA_MUTEX_UNLOCK(tasks->lock);
}

void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_counter* counter){
	if(counter) *counter = (tina_counter){.count = count + 1};
	
	TINA_MUTEX_LOCK(tasks->lock);
	assert(count <= tasks->pool.count);
	
	for(size_t i = 0; i < count; i++){
		tina_task copy = list[i];
		copy._counter = counter;
		
		tina_task* task = _tina_dequeue(&tasks->pool);
		*task = copy;
		
		_tina_enqueue(&tasks->task[copy.priority], task);
	}
	
	TINA_SIGNAL_BROADCAST(tasks->wakeup);
	TINA_MUTEX_UNLOCK(tasks->lock);
}

void tina_tasks_wait(tina_tasks* tasks, tina_task* task, tina_counter* counter){
	assert(counter->task == NULL);
	counter->task = task;
	
	TINA_MUTEX_LOCK(tasks->lock);
	if(--counter->count > 0) tina_yield(counter->task->_coro, false);
	TINA_MUTEX_UNLOCK(tasks->lock);
}

#endif // TINA_TASK_IMPLEMENTATION
#endif // TINA_TASK_H
