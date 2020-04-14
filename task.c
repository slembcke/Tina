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
} task_system;

struct tina_counter {
	uintptr_t count;
	tina_task* task;
};

void task_system_init(task_system *system);
void task_system_queue_tasks(task_system* system, const tina_task* tasks, size_t task_count, tina_counter* counter);
void task_system_run(task_system* system);

static inline tina_counter task_counter_make(tina_task* task){ return (tina_counter){.task = task}; }
void task_wait(tina_task* task, tina_counter* counter);

//--

task_system SYSTEM = {};

static void TaskGeneric(tina_task* task){
	printf("%s\n", task->name);
}

static void TaskA(tina_task* task){
	printf("%s enter.\n", task->name);
	
	tina_counter counter = task_counter_make(task);
	task_system_queue_tasks(&SYSTEM, (tina_task[]){
		{.func = TaskGeneric, .name = "TaskB"},
		{.func = TaskGeneric, .name = "TaskC"},
		{.func = TaskGeneric, .name = "TaskD"},
		{.func = TaskGeneric, .name = "TaskE"},
	}, 4, &counter);
	
	printf("%s queued and waiting.\n", task->name);
	task_wait(task, &counter);
	
	task_system_queue_tasks(&SYSTEM, (tina_task[]){
		{.func = TaskA, .name = "TaskA",}
	}, 1, NULL);
	
	printf("%s finish.\n", task->name);
}

int main(int argc, const char *argv[]){
	task_system_init(&SYSTEM);
	
	task_system_queue_tasks(&SYSTEM, (tina_task[]){
		{.func = TaskA, .name = "TaskA1"},
	}, 1, NULL);
	
	task_system_run(&SYSTEM);
	
	return EXIT_SUCCESS;
}

//--

static unsigned tina_counter_decrement(tina_counter* counter){
	return --(counter->count);
}

void task_system_queue_tasks(task_system* system, const tina_task* tasks, size_t task_count, tina_counter* counter){
	if(counter){
		assert(counter->count == 0);
		counter->count =  task_count + 1;
	}
	
	// TODO syncronized
	unsigned head = system->task_head;
	assert(system->task_count + task_count <= MAX_TASKS);
	system->task_head = (head + task_count) & (MAX_TASKS - 1);
	system->task_count += task_count;
	
	for(size_t i = 0; i < task_count; i++){
		tina_task task = tasks[i];
		task._counter = counter;
		system->tasks[(head + i) & (MAX_TASKS - 1)] = task;
	}
}

enum task_status {
	TASK_STATUS_FINISHED,
	TASK_STATUS_WAITING,
};

void task_wait(tina_task* task, tina_counter* counter){
	// If there are any unfinished tasks, yield.
	if(tina_counter_decrement(counter) > 0){
		// TODO syncronized
		tina_yield(task->_coro, TASK_STATUS_WAITING);
	}
}

static uintptr_t task_body(tina* coro, uintptr_t value){
	while(true){
		tina_task* task = (tina_task*)value;
		task->func(task);
		
		value = tina_yield(coro, TASK_STATUS_FINISHED);
	}
}

void task_system_init(task_system *system){
	for(unsigned i = 0; i < MAX_COROS; i++){
		tina* coro = tina_init(system->MEMORY[i], STACK_SIZE, task_body, &system);
		coro->name = "TASK WORKER";
		system->coros[i] = coro;
	}
	system->coro_count = MAX_COROS;
}

void task_system_run(task_system* system){
	while(true){
		// TODO I suppose the actual system should sleep here or something?
		if(system->task_count == 0) break;
		
		// Dequeue a task.
		tina_task* task = &system->tasks[system->task_tail++ & (MAX_TASKS - 1)];
		system->task_count--;
		
		// Dequeue a coro to run the task on.
		assert(system->coro_count > 0);
		task->_coro = system->coros[system->coro_tail++ & (MAX_COROS - 1)];
		system->coro_count--;
		
		run_again: {
			// Run the task.
			enum task_status status = tina_yield(task->_coro, (uintptr_t)task);
			
			if(status = TASK_STATUS_FINISHED){
				system->coros[system->coro_head++ & (MAX_COROS - 1)] = task->_coro;
				system->coro_count++;
			}
			
			// Decrement the task's counter if it has one.
			tina_counter* counter = task->_counter;
			if(counter){
				task = counter->task;
				// If it decrements to zero, then the waiting task is ready to run.
				if(tina_counter_decrement(counter) == 0) goto run_again;
			}
		}
	}
}
