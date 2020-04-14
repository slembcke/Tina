#include <stdint.h>
#include <string.h>
#include <threads.h>
#include <assert.h>

#include "tina.h"

#ifndef TINA_TASK_H
#define TINA_TASK_H

typedef struct tina_tasks tina_tasks;
typedef struct tina_task tina_task;
typedef struct tina_counter tina_counter;
typedef void tina_task_func(tina_task* task);

struct tina_task {
	const char* name;
	tina_task_func* func;
	void* ptr;
	
	tina* _coro;
	tina_counter* _counter;
};

typedef struct {
	void** arr;
	size_t head, tail, count, capacity;
} _tina_queue;

struct tina_counter {
	unsigned count;
	tina_task* task;
};

size_t tina_tasks_size(size_t task_count, size_t coro_count, size_t stack_size);
tina_tasks* tina_tasks_init(void* buffer, size_t task_count, size_t coro_count, size_t stack_size);
void tina_tasks_worker_loop(tina_tasks* tasks);

void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_counter* counter);
void tina_tasks_wait(tina_tasks* tasks, tina_task* task, tina_counter* counter);

#ifdef TINA_IMPLEMENTATION

struct tina_tasks {
	mtx_t lock;
	cnd_t tasks_available;
	_tina_queue coro, pool, task;
};

static inline void _tina_enqueue(_tina_queue* queue, void* elt){
	assert(queue->count < queue->capacity);
	queue->count++;
	queue->arr[queue->head++ & (queue->capacity - 1)] = elt;
}

static inline void* _tina_dequeue(_tina_queue* queue){
	assert(queue->count > 0);
	queue->count--;
	return queue->arr[queue->tail++ & (queue->capacity - 1)];
}

static uintptr_t _task_body(tina* coro, uintptr_t value){
	tina_tasks* tasks = (tina_tasks*)coro->user_data;
	while(true){
		mtx_unlock(&tasks->lock);
		tina_task* task = (tina_task*)value;
		task->func(task);
		mtx_lock(&tasks->lock);
		
		value = tina_yield(coro, true);
	}
}

size_t tina_tasks_size(size_t task_count, size_t coro_count, size_t stack_size){
	size_t size = sizeof(tina_tasks);
	size += (coro_count + 2*task_count)*sizeof(void*);
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
	tasks->task = (_tina_queue){.arr = buffer, .capacity = task_count};
	buffer += task_count*sizeof(void*);
	
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
	
	return tasks;
}

void tina_tasks_worker_loop(tina_tasks* tasks){
	mtx_lock(&tasks->lock);
	while(true){
		while(tasks->task.count == 0){
			// TODO should be a timed wait to allow shutting down the thread.
			cnd_wait(&tasks->tasks_available, &tasks->lock);
		}
		
		tina_task* task = _tina_dequeue(&tasks->task);
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
	}
	mtx_unlock(&tasks->lock);
}

void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_counter* counter){
	if(counter) counter->count = count + 1;
	
	mtx_lock(&tasks->lock);
	assert(count <= tasks->pool.count);
	
	for(size_t i = 0; i < count; i++){
		tina_task* task = _tina_dequeue(&tasks->pool);
		_tina_enqueue(&tasks->task, task);
		
		tina_task copy = list[i];
		copy._counter = counter;
		*task = copy;
	}
	
	cnd_broadcast(&tasks->tasks_available);
	mtx_unlock(&tasks->lock);
}

void tina_tasks_wait(tina_tasks* tasks, tina_task* task, tina_counter* counter){
	assert(counter->task == NULL);
	counter->task = task;
	
	mtx_lock(&tasks->lock);
	if(--counter->count > 0) tina_yield(counter->task->_coro, false);
	mtx_unlock(&tasks->lock);
}

#endif // TINA_TASK_IMPLEMENTATION
#endif // TINA_TASK_H
