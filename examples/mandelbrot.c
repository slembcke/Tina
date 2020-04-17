#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <threads.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TINA_IMPLEMENTATION
#define _TINA_ASSERT(_COND_, _MESSAGE_) { if(!(_COND_)){puts(_MESSAGE_); abort();} }
#include <tina.h>

#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_app.h"
#include "sokol_gfx.h"

#define TINA_TASKS_IMPLEMENTATION
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

#define W (1*1024)
#define H (W)
uint8_t PIXELS[W*H];

typedef struct {
	unsigned xmin, xmax;
	unsigned ymin, ymax;
} mandelbrot_window;

static void mandelbrot_render(tina_task* task){
	mandelbrot_window* win = task->data;
	// printf("render (%d, %d, %d, %d)\n", win->xmin, win->xmax, win->ymin, win->ymax);
	
	const unsigned maxi = 1024;
	const unsigned sample_count = 4;
	
	for(unsigned py = win->ymin; py < win->ymax; py++){
		for(unsigned px = win->xmin; px < win->xmax; px++){
			double value = 0;
			for(unsigned sample = 0; sample < sample_count; sample++){
				uint64_t ssx = ((uint64_t)px << 32) + (uint32_t)(3242174889u*sample);
				uint64_t ssy = ((uint64_t)py << 32) + (uint32_t)(2447445414u*sample);
				double x0 = 4*((double)ssx/(double)((uint64_t)W << 32)) - 3;
				double y0 = 4*((double)ssy/(double)((uint64_t)H << 32)) - 2;
				double x = 0, y = 0;
				
				unsigned i = 0;
				while(x*x + y*y <= 4 && i < maxi){
					double tmp = x*x - y*y + x0;
					y = 2*x*y + y0;
					x = tmp;
					i++;
				}
				
				value += (i < maxi ? exp(-1e-2*i) : 0);
			}
			PIXELS[px + W*py] = 255*value/sample_count;
		}
	}
}

#define TASK_GROUP_MAX 32

typedef struct {
	tina_tasks* tasks;
	tina_task* task;
	tina_group* group;
} mandelbrot_task_context;

static void mandelbrot_subdiv(mandelbrot_task_context* ctx, mandelbrot_window win){
	// printf("subdiv (%d, %d, %d, %d)\n", win.xmin, win.xmax, win.ymin, win.ymax);
	
	unsigned xmin = win.xmin, xmax = win.xmax;
	unsigned ymin = win.ymin, ymax = win.ymax;
	unsigned xmid = (xmin + xmax)/2, ymid = (ymin + ymax)/2;
	mandelbrot_window sub_windows[] = {
		{.xmin = xmin, .xmax = xmid, .ymin = ymin, .ymax = ymid},
		{.xmin = xmid, .xmax = xmax, .ymin = ymin, .ymax = ymid},
		{.xmin = xmin, .xmax = xmid, .ymin = ymid, .ymax = ymax},
		{.xmin = xmid, .xmax = xmax, .ymin = ymid, .ymax = ymax},
	};
	
	if((xmax - xmin) <= 512 || (ymax - ymin) <= 512){
		// TODO Implement thread local linear allocator?
		mandelbrot_window* cursor = malloc(sizeof(sub_windows));
		memcpy(cursor, sub_windows, sizeof(sub_windows));
		
		tina_tasks_enqueue(ctx->tasks, (tina_task[]){
			{.func = mandelbrot_render, .data = cursor + 0},
			{.func = mandelbrot_render, .data = cursor + 1},
			{.func = mandelbrot_render, .data = cursor + 2},
			{.func = mandelbrot_render, .data = cursor + 3},
		}, 4, ctx->group);
		tina_tasks_wait(ctx->tasks, ctx->task, ctx->group, TASK_GROUP_MAX - 4);
	} else {
		mandelbrot_subdiv(ctx, sub_windows[0]);
		mandelbrot_subdiv(ctx, sub_windows[1]);
		mandelbrot_subdiv(ctx, sub_windows[2]);
		mandelbrot_subdiv(ctx, sub_windows[3]);
	}
}

static void mandelbrot_task(tina_task* task){
	mandelbrot_window* win = task->data;
	worker_context* wctx = task->thread_data;
	
	tina_group group; tina_group_init(&group);
	mandelbrot_subdiv(&(mandelbrot_task_context){
		.tasks = wctx->tasks,
		.task = task,
		.group = &group,
	}, *win);
	
	// Wait for remaining tasks to finish.
	tina_tasks_wait(wctx->tasks, task, &group, 0);
}

static void app_display(void){
	int w = sapp_width(), h = sapp_height();
	sg_pass_action action = {
		.colors[0] = {.action = SG_ACTION_CLEAR, .val = {1, 0, 0}},
	};
	sg_begin_default_pass(&action, w, h);
	
	sg_end_pass();
	sg_commit();
}

static void app_event(const sapp_event *event){
	switch(event->type){
		case SAPP_EVENTTYPE_MOUSE_MOVE: {
			// ChipmunkDemoMouse = MouseToSpace(event);
		}; break;
		
		case SAPP_EVENTTYPE_MOUSE_UP:
		case SAPP_EVENTTYPE_MOUSE_DOWN: {
			// Click(event);
		} break;
		
		default: break;
	}
}

unsigned WORKER_COUNT = 8;
worker_context WORKERS[MAX_WORKERS];
tina_tasks* TASKS;

static void app_init(void){
	puts("Sokol-App init.");
	
	puts("Creating TASKS.");
	size_t task_count = 1024, coroutine_count = 128, stack_size = 64*1024;
	void* buffer = malloc(tina_tasks_size(task_count, coroutine_count, stack_size));
	TASKS = tina_tasks_init(buffer, task_count, coroutine_count, stack_size);
	
	puts("Creating WORKERS.");
	for(int i = 0; i < WORKER_COUNT; i++){
		worker_context* worker = WORKERS + i;
		(*worker) = (worker_context){.tasks = TASKS};
		thrd_create(&worker->thread, worker_body, worker);
	}
	
	puts("Init Sokol-GFX.");
	sg_desc desc = {0};
	sg_setup(&desc);
	assert(sg_isvalid());
	
	puts("Starting root task.");
	tina_tasks_enqueue(TASKS, (tina_task[]){
		{.func = mandelbrot_task, .data = &(mandelbrot_window){.xmin = 0, .xmax = W, .ymin = 0, .ymax = H}},
	}, 0, NULL);
}

static void app_cleanup(void){
	puts("Sokol-App cleanup.");
	// tina_tasks_wait_sleep(TASKS, &group, 0);
	
	puts("WORKERS shutdown.");
	tina_tasks_pause(TASKS);
	for(int i = 0; i < WORKER_COUNT; i++) thrd_join(WORKERS[i].thread, NULL);
	
	puts ("Destroing TASKS");
	tina_tasks_destroy(TASKS);
	free(TASKS);
	
	puts("Sokol-GFX shutdown.");
	// sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
	return (sapp_desc){
		.init_cb = app_init,
		.frame_cb = app_display,
		.event_cb = app_event,
		.cleanup_cb = app_cleanup,
		.width = 1024,
		.height = 1024,
		.high_dpi = true,
		.window_title = "Mandelbrot",
	};
}
