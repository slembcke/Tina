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

typedef struct {
	unsigned xmin, xmax;
	unsigned ymin, ymax;
} mandelbrot_task_context;

static void mandelbrot_render(mandelbrot_task_context* ctx){
	for(unsigned py = ctx->ymin; py < ctx->ymax; py++){
		for(unsigned px = ctx->xmin; px < ctx->xmax; px++){
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
}

static void mandelbrot_task(tina_task* task){
	mandelbrot_task_context* ctx = task->data;
	worker_context* wctx = task->thread_data;
	
	unsigned w = ctx->xmax - ctx->xmin;
	unsigned h = ctx->ymax - ctx->ymin;
	if(w <= 256 || h <= 256){
		mandelbrot_render(ctx);
	} else {
		unsigned xmin = ctx->xmin, xmax = ctx->xmax;
		unsigned ymin = ctx->ymin, ymax = ctx->ymax;
		unsigned xmid = (xmin + xmax)/2, ymid = (ymin + ymax)/2;
		mandelbrot_task_context sub_contexts[] = {
			{.xmin = xmin, .xmax = xmid, .ymin = ymin, .ymax = ymid},
			{.xmin = xmid, .xmax = xmax, .ymin = ymin, .ymax = ymid},
			{.xmin = xmin, .xmax = xmid, .ymin = ymid, .ymax = ymax},
			{.xmin = xmid, .xmax = xmax, .ymin = ymid, .ymax = ymax},
		};
		
		tina_tasks_join(wctx->tasks, (tina_task[]){
			{.func = mandelbrot_task, .data = sub_contexts + 0},
			{.func = mandelbrot_task, .data = sub_contexts + 1},
			{.func = mandelbrot_task, .data = sub_contexts + 2},
			{.func = mandelbrot_task, .data = sub_contexts + 3},
		}, 4, task);
		// printf("parent done (%d, %d, %d, %d)\n", ctx->xmin, ctx->xmax, ctx->ymin, ctx->ymax);
	}
}

int main(void){
	size_t task_count = 1024, coroutine_count = 128, stack_size = 64*1024;
	void* buffer = malloc(tina_tasks_size(task_count, coroutine_count, stack_size));
	tina_tasks* tasks = tina_tasks_init(buffer, task_count, coroutine_count, stack_size);
	
	int worker_count = 8;
	worker_context workers[MAX_WORKERS];
	for(int i = 0; i < worker_count; i++){
		worker_context* worker = workers + i;
		(*worker) = (worker_context){.tasks = tasks};
		thrd_create(&worker->thread, worker_body, worker);
	}
	
	tina_group group;
	tina_tasks_enqueue(tasks, (tina_task[]){
		{.func = mandelbrot_task, .data = &(mandelbrot_task_context){.xmin = 0*W/2, .xmax = 1*W/2, .ymin = 0, .ymax = H}},
		{.func = mandelbrot_task, .data = &(mandelbrot_task_context){.xmin = 1*W/2, .xmax = 2*W/2, .ymin = 0, .ymax = H}},
	}, 2, &group);
	tina_tasks_wait_sleep(tasks, &group);
	
	puts("Writing image.");
	stbi_write_png("out.png", W, H, 1, PIXELS, W);
	
	tina_tasks_pause(tasks);
	for(int i = 0; i < worker_count; i++){
		thrd_join(workers[i].thread, NULL);
	}
	
	puts("Exiting.");
	return EXIT_SUCCESS;
}
