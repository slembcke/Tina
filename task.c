#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define TINA_IMPLEMENTATION
#include "tina.h"

#define MAX_COROS 16
#define MAX_TASKS 1024
#define STACK_SIZE (64*1024)

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
	tina* coros[MAX_COROS];
	unsigned coro_head, coro_tail, coro_count;
	
	tina_task tasks[MAX_TASKS];
	unsigned task_head, task_tail, task_count;
	
	uint8_t MEMORY[MAX_COROS][STACK_SIZE];
} tina_tasks;

struct tina_counter {
	uintptr_t count;
	tina_task* task;
};

void tina_tasks_init(tina_tasks *tasks);
void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_counter* counter);
void tina_tasks_worker_loop(tina_tasks* tasks);

void task_wait(tina_task* task, tina_counter* counter);

//--

tina_tasks SYSTEM = {};

static void TaskGeneric(tina_task* task){
	printf("%s\n", task->name);
}

static void TaskA(tina_task* task){
	printf("%s enter.\n", task->name);
	
	tina_counter counter = {};
	tina_tasks_enqueue(&SYSTEM, (tina_task[]){
		{.func = TaskGeneric, .name = "TaskB"},
		{.func = TaskGeneric, .name = "TaskC"},
		{.func = TaskGeneric, .name = "TaskD"},
		{.func = TaskGeneric, .name = "TaskE"},
	}, 4, &counter);
	
	printf("%s queued and waiting.\n", task->name);
	task_wait(task, &counter);
	
	tina_tasks_enqueue(&SYSTEM, (tina_task[]){
		{.func = TaskA, .name = "TaskA",}
	}, 1, NULL);
	
	printf("%s finish.\n", task->name);
}

int main(int argc, const char *argv[]){
	tina_tasks_init(&SYSTEM);
	
	tina_tasks_enqueue(&SYSTEM, (tina_task[]){
		{.func = TaskA, .name = "TaskA1"},
	}, 1, NULL);
	
	tina_tasks_worker_loop(&SYSTEM);
	
	return EXIT_SUCCESS;
}

//--

static unsigned tina_counter_decrement(tina_counter* counter){
	return --(counter->count);
}

void tina_tasks_enqueue(tina_tasks* tasks, const tina_task* list, size_t count, tina_counter* counter){
	if(counter){
		assert(counter->count == 0);
		counter->count =  count + 1;
	}
	
	// TODO syncronized
	unsigned head = tasks->task_head;
	assert(tasks->task_count + count <= MAX_TASKS);
	tasks->task_head = (head + count) & (MAX_TASKS - 1);
	tasks->task_count += count;
	
	for(size_t i = 0; i < count; i++){
		tina_task task = list[i];
		task._counter = counter;
		tasks->tasks[(head + i) & (MAX_TASKS - 1)] = task;
	}
}

void task_wait(tina_task* task, tina_counter* counter){
	assert(counter->task == NULL);
	counter->task = task;
	
	// If there are any unfinished tasks, yield.
	if(tina_counter_decrement(counter) > 0){
		// TODO syncronized
		tina_yield(task->_coro, false);
	}
}

static uintptr_t task_body(tina* coro, uintptr_t value){
	while(true){
		tina_task* task = (tina_task*)value;
		task->func(task);
		
		value = tina_yield(coro, true);
	}
}

void tina_tasks_init(tina_tasks *tasks){
	for(unsigned i = 0; i < MAX_COROS; i++){
		tina* coro = tina_init(tasks->MEMORY[i], STACK_SIZE, task_body, &tasks);
		coro->name = "TINA TASK WORKER";
		tasks->coros[i] = coro;
	}
	tasks->coro_count = MAX_COROS;
}

void tina_tasks_worker_loop(tina_tasks* tasks){
	while(true){
		// TODO I suppose the thread should sleep here or something?
		if(tasks->task_count == 0) break;
		assert(tasks->coro_count > 0);
		
		// Dequeue a task and a coroutine to run it on.
		tina_task* task = &tasks->tasks[tasks->task_tail++ & (MAX_TASKS - 1)];
		tasks->task_count--;
		
		task->_coro = tasks->coros[tasks->coro_tail++ & (MAX_COROS - 1)];
		tasks->coro_count--;
		
		run_again: {
			if(tina_yield(task->_coro, (uintptr_t)task)){
				// Task is finished. Return the coroutine to the pool.
				tasks->coros[tasks->coro_head++ & (MAX_COROS - 1)] = task->_coro;
				tasks->coro_count++;
			}
			
			tina_counter* counter = task->_counter;
			if(counter){
				task = counter->task;
				// This was the last task the counter was waiting on. Wake up the waiting task.
				if(tina_counter_decrement(counter) == 0) goto run_again;
			}
		}
	}
}
