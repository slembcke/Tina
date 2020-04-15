#include <stdbool.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <threads.h>
#include <assert.h>

#include "dispatch/dispatch.h"

atomic_uint COUNT;
dispatch_queue_t TASKS;

void TaskGeneric(void* context){
	atomic_fetch_add(&COUNT, 1);
	// Trying to force it to run multiple worker threads?
	// Dunno if this destroys the micro-bench.
	thrd_yield();
}

void TaskA(void* context){
	dispatch_group_t group = dispatch_group_create();
	for(int i = 0; i < 15; i++){
		dispatch_group_async_f(group, TASKS, NULL, TaskGeneric);
	}
	
	unsigned* countdown = context;
	if(--*countdown){
		dispatch_group_notify_f(group, TASKS, context, TaskA);
	}
	
	atomic_fetch_add(&COUNT, 1);
}

static int worker_thread(void* tasks){
	thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
	
	printf("exiting with count: %dK\n", COUNT/1000);
	exit(EXIT_SUCCESS);
}

int main(int argc, const char *argv[]){
	atomic_init(&COUNT, 0);
	TASKS = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);
	// TASKS = dispatch_get_main_queue();
	
	int worker_count = 1;
	thrd_t workers[16];
	for(int i = 0; i < worker_count; i++) thrd_create(&workers[i], worker_thread, NULL);
	
	unsigned parallel = 16;
	unsigned repeat_counter[parallel];
	for(int i = 0; i < parallel; i++){
		repeat_counter[i] = 64000;
		dispatch_async_f(TASKS, repeat_counter + i, TaskA);
	}
	
	puts("waiting");
	dispatch_main();
}
