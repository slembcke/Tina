// NOTE: This whole test is kinda dumb in that it sort of just tests shared memory performance, but whatever.

#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <assert.h>

#include "tina.h"
#include "tina_jobs.h"
#include "common/common.h"

tina_scheduler* SCHED;
atomic_uint COUNT;

static void task_increment(tina_job* task, void* user_data, unsigned* thread_id){
	atomic_fetch_add(&COUNT, 1);
}

static void task_more_tasks(tina_job* task, void* user_data, unsigned* thread_id){
	tina_scheduler_enqueue_batch(SCHED, (tina_job_description[]){
		// Make a bunch of tasks to increment the counter.
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		{.func = task_increment},
		// Make a task to add more tasks!
		{.func = task_more_tasks},
	}, 16, NULL);
	
	atomic_fetch_add(&COUNT, 1);
}

int main(int argc, const char *argv[]){
	atomic_init(&COUNT, 0);
	
	SCHED = tina_scheduler_new(1024, 1, 64, 64*1024);
	common_start_worker_threads(1, SCHED, 0);
	
	// Seed the first 16 tasks into the system.
	for(unsigned i = 0; i < 16; i++){
		tina_scheduler_enqueue(SCHED, NULL, task_more_tasks, NULL, 0, NULL);
	}
	
	puts("waiting");
	unsigned seconds = 10;
	thrd_sleep(&(struct timespec){.tv_sec = seconds}, NULL);
	
	tina_scheduler_pause(SCHED);
	common_destroy_worker_threads();
	
	printf("exiting with count: %dK tasks/sec\n", COUNT/1000/seconds);
	return EXIT_SUCCESS;
}
