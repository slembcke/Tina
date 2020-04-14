#include <stdint.h>
#include <string.h>
#include <threads.h>
#include <assert.h>

#include "tina.h"

#ifndef TINA_TASK_H
#define TINA_TASK_H

#define TINA_TASKS_MAX_COROS (256)
#define TINA_TASKS_MAX_TASKS (1024)
#define TINA_TASKS_STACK_SIZE (64*1024)

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

typedef struct {
	_tina_queue coro, pool, task;
	
	mtx_t lock;
	cnd_t tasks_available;
	uint8_t CORO_BUFFER[TINA_TASKS_MAX_COROS][TINA_TASKS_STACK_SIZE];
	tina_task TASK_BUFFER[TINA_TASKS_MAX_TASKS];
	void* COROQ_BUFFER[TINA_TASKS_MAX_COROS];
	void* POOLQ_BUFFER[TINA_TASKS_MAX_TASKS];
	void* TASKQ_BUFFER[TINA_TASKS_MAX_TASKS];
} tina_tasks;

struct tina_counter {
	unsigned count;
	tina_task* task;
};

void tina_tasks_init(tina_tasks *tasks);
void tina_tasks_worker_loop(tina_tasks* tasks);

void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_counter* counter);
void tina_tasks_wait(tina_tasks* tasks, tina_task* task, tina_counter* counter);

#ifdef TINA_IMPLEMENTATION

static inline void _tina_enqueue(_tina_queue* queue, void* elt){
	queue->count++;
	queue->arr[queue->head++ & (queue->capacity - 1)] = elt;
}

static inline void* _tina_dequeue(_tina_queue* queue){
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

void tina_tasks_init(tina_tasks *tasks){
	tasks->coro = (_tina_queue){.arr = tasks->COROQ_BUFFER, .capacity = TINA_TASKS_MAX_COROS};
	tasks->pool = (_tina_queue){.arr = tasks->POOLQ_BUFFER, .capacity = TINA_TASKS_MAX_TASKS};
	tasks->task = (_tina_queue){.arr = tasks->TASKQ_BUFFER, .capacity = TINA_TASKS_MAX_TASKS};
	
	tasks->coro.count = TINA_TASKS_MAX_COROS;
	for(unsigned i = 0; i < TINA_TASKS_MAX_COROS; i++){
		tina* coro = tina_init(tasks->CORO_BUFFER[i], TINA_TASKS_STACK_SIZE, _task_body, &tasks);
		coro->name = "TINA TASK WORKER";
		coro->user_data = tasks;
		tasks->coro.arr[i] = coro;
	}
	
	tasks->pool.count = TINA_TASKS_MAX_TASKS;
	for(unsigned i = 0; i < TINA_TASKS_MAX_TASKS; i++){
		tasks->pool.arr[i] = &tasks->TASK_BUFFER[i];
	}
}

void tina_tasks_worker_loop(tina_tasks* tasks){
	mtx_lock(&tasks->lock);
	while(true){
		// Wait for a task to become available.
		while(tasks->task.count == 0){
			// TODO should be a timed wait to allow shutting down the thread.
			cnd_wait(&tasks->tasks_available, &tasks->lock);
		}
		
		// Dequeue a task and a coroutine to run it on.
		tina_task* task = _tina_dequeue(&tasks->task);
		task->_coro = _tina_dequeue(&tasks->coro);
		
		run_again: {
			bool finished = tina_yield(task->_coro, (uintptr_t)task);
			
			if(finished){
				// Task is finished. Return it and it's coroutine to the pool.
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
	// If there are any unfinished tasks, yield.
	if(--counter->count > 0) tina_yield(counter->task->_coro, false);
	mtx_unlock(&tasks->lock);
}

#endif // TINA_TASK_IMPLEMENTATION
#endif // TINA_TASK_H
