// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Scott Lembcke and Howling Moon Software

// NOTE: This whole test is kinda dumb in that it sort of just tests shared memory performance, but whatever.

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#if _MSC_VER
	// AURGH! Whatever, this is a really dumb throughput test anyway.
	typedef unsigned atomic_uint;
	void atomic_init(atomic_uint* ptr, unsigned n){*ptr = n;}
	void atomic_fetch_add(atomic_uint* ptr, unsigned n){*ptr += n;}
#else
	#include <stdatomic.h>
#endif

#include "tina.h"
#include "tina_jobs.h"
#include "common/common.h"

tina_scheduler* SCHED;
atomic_uint COUNT;

static void task_increment(tina_job* task){
	atomic_fetch_add(&COUNT, 1);
}

static void task_more_tasks(tina_job* task){
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
	}, 16, NULL, 0);
	
	atomic_fetch_add(&COUNT, 1);
}

int main(int argc, const char *argv[]){
	atomic_init(&COUNT, 0);
	
	SCHED = tina_scheduler_new(1024, 1, 64, 64*1024);
	common_start_worker_threads(1, SCHED, 0);
	
	// Seed the first 16 tasks into the system.
	for(unsigned i = 0; i < 16; i++){
		tina_scheduler_enqueue(SCHED, task_more_tasks, NULL, 0, 0, NULL);
	}
	
	puts("waiting");
	unsigned seconds = 10;
	thrd_sleep(&(struct timespec){.tv_sec = seconds}, NULL);
	
	tina_scheduler_interrupt(SCHED, 0);
	common_destroy_worker_threads();
	
	printf("exiting with count: %dK tasks/sec\n", COUNT/1000/seconds);
	return EXIT_SUCCESS;
}
