#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <threads.h>
#include <assert.h>

#include "tina.h"
#include "tina_jobs.h"

tina_scheduler* TASKS;
atomic_uint COUNT;

static void TaskGeneric(tina_job* task, void* user_data, void** thread_data){
	atomic_fetch_add(&COUNT, 1);
}

static void TaskA(tina_job* task, void* user_data, void** thread_data){
	tina_scheduler_join(TASKS, (tina_job_description[]){
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
	}, 16 - 1, task);
	
	unsigned* countdown = user_data;
	if(--*countdown){
		tina_scheduler_enqueue(TASKS, "Task0", TaskA, countdown, 0, NULL);
	}
	
	atomic_fetch_add(&COUNT, 1);
}

static int worker_thread(void* tasks){
	tina_scheduler_run(tasks, 0, false, NULL);
	return 0;
}

int main(int argc, const char *argv[]){
	atomic_init(&COUNT, 0);
	
	TASKS = tina_scheduler_new(1024, 1, 64, 64*1024);
	
	unsigned worker_count = 4;
	thrd_t workers[16];
	for(unsigned i = 0; i < worker_count; i++) thrd_create(&workers[i], worker_thread, TASKS);
	
	unsigned parallel = 16;
	unsigned repeat_group[parallel];
	for(unsigned i = 0; i < parallel; i++){
		repeat_group[i] = 128000;
		tina_scheduler_enqueue(TASKS, "Task0", TaskA, repeat_group + i, 0, NULL);
	}
	
	puts("waiting");
	thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
	
	tina_scheduler_pause(TASKS);
	for(unsigned i = 0; i < worker_count; i++) thrd_join(workers[i], NULL);
	
	printf("exiting with count: %dK\n", COUNT/1000);
	return EXIT_SUCCESS;
}
