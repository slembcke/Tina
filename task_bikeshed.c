#include <stdbool.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <threads.h>
#include <assert.h>

// #define TINA_IMPLEMENTATION
// #include "tina.h"

// #define TINA_TASK_IMPLEMENTATION
// #include "tina_task.h"

#define BIKESHED_IMPLEMENTATION
#include "bikeshed.h"

Bikeshed TASKS;
atomic_uint COUNT;

enum Bikeshed_TaskResult TaskGeneric(Bikeshed shed, Bikeshed_TaskID task_id, uint8_t channel, void* context){
	atomic_fetch_add(&COUNT, 1);
	return BIKESHED_TASK_RESULT_COMPLETE;
}

enum Bikeshed_TaskResult TaskA(Bikeshed shed, Bikeshed_TaskID task_id, uint8_t channel, void* context){
	Bikeshed_TaskID ids[16];
	Bikeshed_CreateTasks(TASKS, 16, (BikeShed_TaskFunc[]){
		TaskA, TaskGeneric, TaskGeneric, TaskGeneric,
		TaskGeneric, TaskGeneric, TaskGeneric, TaskGeneric,
		TaskGeneric, TaskGeneric, TaskGeneric, TaskGeneric,
		TaskGeneric, TaskGeneric, TaskGeneric, TaskGeneric,
	}, (void*[16]){context}, ids);
	
	Bikeshed_AddDependencies(TASKS, 1, ids + 0, 15, ids + 1);
	Bikeshed_ReadyTasks(TASKS, 15, ids + 1);
	
	unsigned* countdown = context;
	if(--*countdown){}
	
	atomic_fetch_add(&COUNT, 1);
	return BIKESHED_TASK_RESULT_COMPLETE;
}

static int worker_thread(void* tasks){
	while(true) Bikeshed_ExecuteOne(TASKS, 0);
	return 0;
}

int main(int argc, const char *argv[]){
	atomic_init(&COUNT, 0);
	
	void* buffer = malloc(BIKESHED_SIZE(1024, 16, 1));
	TASKS = Bikeshed_Create(buffer, 1024, 16, 1, NULL);
	
	int worker_count = 4;
	thrd_t workers[16];
	for(int i = 0; i < worker_count; i++) thrd_create(&workers[i], worker_thread, TASKS);
	
	unsigned parallel = 16;
	unsigned repeat_counter[parallel];
	for(int i = 0; i < parallel; i++){
		repeat_counter[i] = 64000;
		Bikeshed_TaskID id;
		Bikeshed_CreateTasks(TASKS, 1, (BikeShed_TaskFunc[]){TaskA}, (void*[]){repeat_counter + i}, &id);
		
		Bikeshed_ReadyTasks(TASKS, 1, &id);
	}
	
	puts("waiting");
	thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
	
	// tina_tasks_pause(TASKS);
	// for(int i = 0; i < worker_count; i++) thrd_join(workers[i], NULL);
	
	printf("exiting with count: %dK\n", COUNT/1000);
	return EXIT_SUCCESS;
}
