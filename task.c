#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <threads.h>
#include <assert.h>

#define TINA_IMPLEMENTATION
#include "tina.h"

#define TINA_TASK_IMPLEMENTATION
#include "tina_task.h"

tina_tasks TASKS = {};
unsigned COUNT;

static void TaskGeneric(tina_task* task){
	printf("%s\n", task->name);
	COUNT++;
}

static void TaskA(tina_task* task){
	printf("%s\n", task->name);
	
	tina_counter counter = {};
	tina_tasks_enqueue(&TASKS, (tina_task[]){
		{.func = TaskGeneric, .name = "Task1"},
		{.func = TaskGeneric, .name = "Task2"},
		{.func = TaskGeneric, .name = "Task3"},
		{.func = TaskGeneric, .name = "Task4"},
		{.func = TaskGeneric, .name = "Task5"},
		{.func = TaskGeneric, .name = "Task6"},
		{.func = TaskGeneric, .name = "Task7"},
		{.func = TaskGeneric, .name = "Task8"},
		{.func = TaskGeneric, .name = "Task9"},
		{.func = TaskGeneric, .name = "TaskA"},
		{.func = TaskGeneric, .name = "TaskB"},
		{.func = TaskGeneric, .name = "TaskC"},
		{.func = TaskGeneric, .name = "TaskD"},
		{.func = TaskGeneric, .name = "TaskE"},
		{.func = TaskGeneric, .name = "TaskF"},
	}, 1, &counter);
	
	tina_tasks_wait(&TASKS, task, &counter);
	
	unsigned* countdown = task->ptr;
	if(--*countdown){
		tina_tasks_enqueue(&TASKS, (tina_task[]){
			{.name = "Task0", .func = TaskA, .ptr = countdown}
		}, 1, NULL);
	}
	
	COUNT++;
}

static int worker_thread(void* tasks){
	tina_tasks_worker_loop(tasks);
	return 0;
}

int main(int argc, const char *argv[]){
	tina_tasks_init(&TASKS);
	
	thrd_t worker;
	thrd_create(&worker, worker_thread, &TASKS);
	
	unsigned parallel = 1;
	unsigned repeat_counter[parallel];
	for(int i = 0; i < parallel; i++){
		repeat_counter[i] = 4;
		tina_tasks_enqueue(&TASKS, (tina_task[]){
			{.name = "Task0", .func = TaskA, .ptr = repeat_counter + i},
		}, 1, NULL);
	}
	
	puts("waiting");
	thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
	
	printf("exiting with count: %d", COUNT);
	return EXIT_SUCCESS;
}
