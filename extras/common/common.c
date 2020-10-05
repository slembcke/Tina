#include <stdio.h>
#include "libs/tinycthread.h"

#define TINA_IMPLEMENTATION
// #define _TINA_ASSERT(_COND_, _MESSAGE_) //{ if(!(_COND_)){fprintf(stdout, _MESSAGE_"\n"); abort();} }
#include "tina.h"

#define TINA_JOBS_IMPLEMENTATION
#include "tina_jobs.h"

#include "common.h"

#if defined(__unix__)
	#include <unistd.h>
	static unsigned common_get_cpu_count(void){return sysconf(_SC_NPROCESSORS_ONLN);}
#elif defined(__WINNT__)
	static unsigned common_get_cpu_count(void){
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		return sysinfo.dwNumberOfProcessors;
	}
#else
	#error TODO Unhandled/unknown system type.
#endif

typedef struct {
	thrd_t thread;
	tina_scheduler* sched;
	unsigned queue_idx;
	unsigned thread_id;
} worker_context;

#define MAX_WORKERS 256
static unsigned WORKER_COUNT;
worker_context WORKERS[MAX_WORKERS];

static int common_worker_body(void* data){
	worker_context* ctx = data;
	tina_scheduler_run(ctx->sched, ctx->queue_idx, false, ctx->thread_id);
	return 0;
}

void common_start_worker_threads(unsigned thread_count, tina_scheduler* sched, unsigned queue_idx){
	if(thread_count){
		WORKER_COUNT = thread_count;
	} else {
		WORKER_COUNT = common_get_cpu_count();
		printf("%d CPUs detected.\n", WORKER_COUNT);
	}
	
	puts("Creating WORKERS.");
	for(unsigned i = 0; i < WORKER_COUNT; i++){
		worker_context* worker = WORKERS + i;
		(*worker) = (worker_context){.sched = sched, .queue_idx = queue_idx, .thread_id = i};
		thrd_create(&worker->thread, common_worker_body, worker);
	}
}

unsigned common_worker_count(void){return WORKER_COUNT;}

void common_destroy_worker_threads(){
	for(unsigned i = 0; i < WORKER_COUNT; i++) thrd_join(WORKERS[i].thread, NULL);
}
