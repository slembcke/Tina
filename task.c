#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define TINA_IMPLEMENTATION
#include "tina.h"

#define TINA_TASK_IMPLEMENTATION
#include "tina_task.h"

tina_tasks TASKS = {};

static void TaskGeneric(tina_task* task){
	printf("%s\n", task->name);
}

static void TaskA(tina_task* task){
	printf("%s enter.\n", task->name);
	
	tina_counter counter = {};
	tina_tasks_enqueue(&TASKS, (tina_task[]){
		{.func = TaskGeneric, .name = "TaskB"},
		{.func = TaskGeneric, .name = "TaskC"},
		{.func = TaskGeneric, .name = "TaskD"},
		{.func = TaskGeneric, .name = "TaskE"},
	}, 4, &counter);
	
	printf("%s queued and waiting.\n", task->name);
	tina_tasks_wait(&TASKS, task, &counter);
	
	// tina_tasks_enqueue(&TASKS, (tina_task[]){
	// 	{.func = TaskA, .name = "TaskA",}
	// }, 1, NULL);
	
	printf("%s finish.\n", task->name);
}

int main(int argc, const char *argv[]){
	tina_tasks_init(&TASKS);
	
	tina_tasks_enqueue(&TASKS, (tina_task[]){
		{.func = TaskA, .name = "TaskA1"},
	}, 1, NULL);
	
	tina_tasks_worker_loop(&TASKS);
	
	return EXIT_SUCCESS;
}
