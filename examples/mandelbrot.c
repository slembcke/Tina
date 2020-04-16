#include <stdlib.h>
#include <stdio.h>

#include <threads.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TINA_IMPLEMENTATION
#define _TINA_ASSERT(_COND_, _MESSAGE_) { if(!(_COND_)){puts(_MESSAGE_); abort();} }
#include <tina.h>

#define TINA_TASKS_IMPLEMENTATION
// #define _TINA_MUTEX_T pthread_mutex_t
// #define _TINA_MUTEX_INIT(_LOCK_) pthread_mutex_init(&_LOCK_, NULL)
// #define _TINA_MUTEX_LOCK(_LOCK_) pthread_mutex_lock(&_LOCK_)
// #define _TINA_MUTEX_UNLOCK(_LOCK_) pthread_mutex_unlock(&_LOCK_)
// #define _TINA_SIGNAL_T pthread_cond_t
// #define _TINA_SIGNAL_INIT(_SIG_) pthread_cond_init(&_SIG_, NULL)
// #define _TINA_SIGNAL_WAIT(_SIG_, _LOCK_) pthread_cond_wait(&_SIG_, &_LOCK_);
// #define _TINA_SIGNAL_BROADCAST(_SIG_) pthread_cond_broadcast(&_SIG_)
#include "tina_tasks.h"

// TODO destroy cond/mutex

#define MAX_WORKERS 16

typedef struct {
	thrd_t thread;
	tina_tasks* tasks;
} worker_context;

static int worker_body(void* data){
	worker_context* ctx = data;
	tina_tasks_run(ctx->tasks, false, ctx);
	return 0;
}

#define W 4096
#define H 2048
uint8_t PIXELS[W*H];

static void TaskEmpty(tina_task* task){
	worker_context* ctx = task->data;
	puts("empty");
}

int main(void){
	size_t task_count = 1024, coroutine_count = 128, stack_size = 16*1024;
	void* buffer = malloc(tina_tasks_size(task_count, coroutine_count, stack_size));
	tina_tasks* tasks = tina_tasks_init(buffer, task_count, coroutine_count, stack_size);
	
	int worker_count = 1;
	worker_context workers[MAX_WORKERS];
	for(int i = 0; i < worker_count; i++){
		worker_context* worker = workers + i;
		(*worker) = (worker_context){.tasks = tasks};
		thrd_create(&worker->thread, worker_body, worker);
	}
	
	for(unsigned py = 0; py < H; py++){
		for(unsigned px = 0; px < W; px++){
			double x0 = 3.5*((double)px/(double)W) - 2.5;
			double y0 = 2.0*((double)py/(double)H) - 1.0;
			double x = 0, y = 0;
			
			const int maxi = 1024;
			unsigned i = 0;
			while(x*x + y*y <= 4 && i < maxi){
				double tmp = x*x - y*y + x0;
				y = 2*x*y + y0;
				x = tmp;
				i++;
			}
			
			PIXELS[px + W*py] = (i < maxi ? ~i : 0);
		}
	}
	
	puts("Writing image.");
	stbi_write_png("out.png", W, H, 1, PIXELS, W);
	
	tina_group group;
	tina_tasks_enqueue(tasks, &(tina_task){.func = TaskEmpty}, 1, &group);
	tina_tasks_wait_sleep(tasks, &group);
	
	puts("Shutting down workers.");
	tina_tasks_pause(tasks);
	for(int i = 0; i < worker_count; i++){
		thrd_join(workers[i].thread, NULL);
	}
	
	puts("Exiting.");
	return EXIT_SUCCESS;
}
