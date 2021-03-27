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
#include <stdatomic.h>
#include <assert.h>

#include "tina.h"
#include "tina_jobs.h"
#include "common/common.h"

enum {
	QUEUE_MAIN,
	QUEUE_WORK,
	_QUEUE_COUNT,
};

tina_scheduler* SCHED;
int COUNT = 0;

static void dummy_job(tina_job* job){
	// thrd_sleep(&(struct timespec){.tv_nsec = 1000000}, NULL);
	tina_job_switch_queue(job, QUEUE_MAIN);
	COUNT++;
}

static void make_and_wait_for_jobs(tina_job* job){
	tina_group group;
	
	for(int i = 0; i < 512; i++){
		tina_scheduler_enqueue(SCHED, NULL, dummy_job, NULL, 0, QUEUE_WORK, &group);
		tina_job_wait(job, &group, 16);
	}
	
	unsigned foo = -1;
	while(foo){
		printf("foo: %d\n", foo);
		foo = tina_job_wait(job, &group, foo - 1);
	}
	
	bool* done = tina_job_get_description(job)->user_data;
	*done = true;
}

int main(int argc, const char *argv[]){
	SCHED = tina_scheduler_new(1024, _QUEUE_COUNT, 65, 64*1024);
	common_start_worker_threads(1, SCHED, QUEUE_WORK);
	
	bool done = false;
	tina_scheduler_enqueue(SCHED, NULL, make_and_wait_for_jobs, &done, 0, QUEUE_MAIN, NULL);
	
	while(!done) tina_scheduler_run(SCHED, QUEUE_MAIN, true);
	
	tina_scheduler_pause(SCHED);
	common_destroy_worker_threads();
	
	printf("done %d\n", COUNT);
	return EXIT_SUCCESS;
}
