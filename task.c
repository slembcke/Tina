#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define TINA_IMPLEMENTATION
#include "tina.h"

#define MAX_COROS 256
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

struct tina_counter {
	uintptr_t count;
	tina_task* task;
};

typedef struct {
	tina* coros[MAX_COROS];
	unsigned coro_head, coro_tail, coro_count;
	
	tina_task tasks[MAX_TASKS];
	unsigned task_head, task_tail, task_count;
} task_system;

enum task_status {
	TASK_STATUS_FINISHED,
	TASK_STATUS_WAITING,
};

static unsigned tina_counter_decrement(tina_counter* counter){ return --(counter->count); }

static void queue_tasks(task_system* system, const tina_task* tasks, size_t task_count, tina_counter* counter){
	if(counter) counter->count =  task_count + 1;
	
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

static void task_wait(tina_task* task, tina_counter* counter){
	// If there are any unfinished tasks, yield.
	if(tina_counter_decrement(counter) > 0){
		// TODO syncronized
		tina_yield(task->_coro, TASK_STATUS_WAITING);
	}
}

static uintptr_t task_coro_body(tina* coro, uintptr_t value){
	printf("task_coro_body()\n");
	
	while(true){
		tina_task* task = (tina_task*)value;
		task->func(task);
		
		value = tina_yield(coro, TASK_STATUS_FINISHED);
	}
}

static void coro_error(tina* coro, const char* message){
	printf("Tina error (%s): %s\n", coro->name, message);
}

// --

uint8_t CORO_BUFFER[MAX_COROS][STACK_SIZE];
task_system SYSTEM = {};

static void TaskA(tina_task* task);
static void TaskB(tina_task* task);
static void TaskC(tina_task* task);

static void TaskA(tina_task* task){
	printf("%s enter.\n", task->name);
	
	tina_counter counter = {.task = task};
	queue_tasks(&SYSTEM, (tina_task[]){
		{.func = TaskB, .name = "TaskB"},
	}, 1, &counter);
	
	printf("%s wait.\n", task->name);
	task_wait(task, &counter);
	
	printf("%s finish.\n", task->name);
}

static void TaskB(tina_task* task){
	printf("%s enter.\n", task->name);
	printf("%s finish.\n", task->name);
}

int main(int argc, const char *argv[]){
	for(unsigned i = 0; i < MAX_COROS; i++){
		tina* coro = tina_init(CORO_BUFFER[i], STACK_SIZE, task_coro_body, &system);
		coro->name = "TASK WORKER";
		coro->error_handler = coro_error;
		SYSTEM.coros[i] = coro;
	}
	SYSTEM.coro_count = MAX_COROS;
	
	queue_tasks(&SYSTEM, (tina_task[]){
		{.func = TaskA, .name = "TaskA"}
	}, 1, NULL);
	
	while(true){
		// TODO I suppose the actual system should sleep here or something?
		if(SYSTEM.task_count == 0) break;
		
		// Dequeue a task.
		tina_task* task = &SYSTEM.tasks[SYSTEM.task_tail];
		SYSTEM.task_tail = (SYSTEM.task_tail + 1) & (MAX_TASKS - 1);
		SYSTEM.task_count--;
		
		// Dequeue a coro to run the task on.
		assert(SYSTEM.coro_count > 0);
		task->_coro = SYSTEM.coros[SYSTEM.coro_tail];
		SYSTEM.coro_tail = (SYSTEM.coro_tail + 1) & (MAX_COROS - 1);
		
		run_again: {
			// Run the task.
			int status = tina_yield(task->_coro, (uintptr_t)task);
			
			if(status = TASK_STATUS_FINISHED){
				// pool the coro.
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
	
	return EXIT_SUCCESS;
}
