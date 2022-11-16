/*
	Copyright (c) 2021 Scott Lembcke

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

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
