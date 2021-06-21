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

static void wait_countdown_sync(tina_job* job){
	unsigned* counter = tina_job_get_description(job)->user_data;
	unsigned idx = tina_job_get_description(job)->user_idx;
	
	// Let 2 tasks run each time the main job yields.
	while(*counter > idx + 2) tina_job_yield(job);
}

static void test_wait_countdown_sync(tina_job* job){
	unsigned counter = 64;
	tina_group group = {};
	
	// Run sub-jobs on the main queue so we can yield them 2 at a time.
	for(unsigned i = 0; i < counter; i++){
		tina_scheduler_enqueue(SCHED, NULL, wait_countdown_sync, &counter, i, QUEUE_MAIN, &group);
	}
	
	while(counter){
		unsigned expected = counter - 2;
		counter = tina_job_wait(job, &group, counter - 1);
		assert(counter == expected);
	}
	
	puts("test_wait_countdown_sync() success");
}

static void wait_countdown_async(tina_job* job){
	thrd_yield();
}

static void test_wait_countdown_async(tina_job* job){
	unsigned counter = 1000;
	tina_group group = {};
	
	for(unsigned i = 0; i < counter; i++){
		tina_scheduler_enqueue(SCHED, NULL, wait_countdown_async, &counter, i, QUEUE_WORK, &group);
	}
	
	while(counter){
		unsigned expected = counter - 1;
		counter = tina_job_wait(job, &group, counter - 1);
		assert(counter <= expected);
	}
	puts("test_wait_countdown_async() success");
}

typedef struct {
	tina_group sync;
	unsigned done_bits;
} multi_ctx;

static void wait_multi(tina_job* job){
	const tina_job_description* desc = tina_job_get_description(job);
	multi_ctx* ctx = desc->user_data;
	unsigned idx = desc->user_idx;
	
	// Wait two jobs per decrement.
	tina_job_wait(job, &ctx->sync, idx/2);
	ctx->done_bits |= 1 << idx;
}

static void test_wait_multiple(tina_job* job){
	multi_ctx ctx = {};
	
	tina_group_increment(SCHED, &ctx.sync, 16);
	for(unsigned i = 0; i < 32; i++){
		tina_scheduler_enqueue(SCHED, NULL, wait_multi, &ctx, i, QUEUE_MAIN, NULL);
	}
	
	unsigned expected_bits = 0;
	for(int i = 0; i < 16; i++){
		// Wake up jobs.
		tina_group_decrement(SCHED, &ctx.sync, 1);
		// Move to back of the queue.
		tina_job_yield(job);
		
		unsigned expected_bits = (0xFFFFFFFF00000000 >> (2*i + 2)) & 0xFFFFFFFF;
		assert(ctx.done_bits == expected_bits);
	}
	
	puts("test_wait_multiple() success");
}

static void run_tests(tina_job* job){
	test_wait_countdown_sync(job);
	test_wait_countdown_async(job);
	test_wait_multiple(job);
	tina_scheduler_interrupt(SCHED, QUEUE_MAIN);
}

int main(int argc, const char *argv[]){
	SCHED = tina_scheduler_new(1024, _QUEUE_COUNT, 65, 64*1024);
	common_start_worker_threads(1, SCHED, QUEUE_WORK);
	
	tina_scheduler_enqueue(SCHED, NULL, run_tests, NULL, 0, QUEUE_MAIN, NULL);
	tina_scheduler_run(SCHED, QUEUE_MAIN, TINA_RUN_LOOP);
	
	tina_scheduler_interrupt(SCHED, QUEUE_WORK);
	common_destroy_worker_threads();
	
	return EXIT_SUCCESS;
}
