#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <threads.h>

#define SOKOL_IMPL
#define SOKOL_GL_IMPL
#define SOKOL_GLCORE33
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_gl.h"

#define TINA_IMPLEMENTATION
#include <tina.h>

#define TINA_TASKS_IMPLEMENTATION
#include "tina_tasks.h"

tina_tasks* TASKS;

typedef struct {
	thrd_t thread;
} worker_context;

#define MAX_WORKERS 16
static unsigned WORKER_COUNT = 8;
worker_context WORKERS[MAX_WORKERS];

static int worker_body(void* data){
	worker_context* ctx = data;
	tina_tasks_run(TASKS, false, ctx);
	return 0;
}

#define TEXTURE_SIZE 256

typedef struct {
	unsigned xmin, xmax;
	unsigned ymin, ymax;
} mandelbrot_window;

static void mandelbrot_render(uint8_t *pixels){
	const unsigned maxi = 1024;
	const unsigned sample_count = 4;
	
	for(size_t py = 0; py < TEXTURE_SIZE; py++){
		for(size_t px = 0; px < TEXTURE_SIZE; px++){
			double value = 0;
			for(unsigned sample = 0; sample < sample_count; sample++){
				uint64_t ssx = ((uint64_t)px << 32) + (uint32_t)(3242174889u*sample);
				uint64_t ssy = ((uint64_t)py << 32) + (uint32_t)(2447445414u*sample);
				double x0 = 4*((double)ssx/(double)((uint64_t)TEXTURE_SIZE << 32)) - 3;
				double y0 = 4*((double)ssy/(double)((uint64_t)TEXTURE_SIZE << 32)) - 2;
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
			
			uint8_t intensity = 255*value/sample_count;
			const int stride = 4*TEXTURE_SIZE;
			pixels[4*px + py*stride + 0] = intensity;
			pixels[4*px + py*stride + 1] = intensity;
			pixels[4*px + py*stride + 2] = intensity;
			pixels[4*px + py*stride + 3] = intensity;
		}
	}
}

#define TASK_GROUP_MAX 32

// typedef struct {
// 	tina_task* task;
// 	tina_group* group;
// } mandelbrot_task_context;

// static void mandelbrot_subdiv(mandelbrot_task_context* ctx, mandelbrot_window win){
// 	// printf("subdiv (%d, %d, %d, %d)\n", win.xmin, win.xmax, win.ymin, win.ymax);
	
// 	unsigned xmin = win.xmin, xmax = win.xmax;
// 	unsigned ymin = win.ymin, ymax = win.ymax;
// 	unsigned xmid = (xmin + xmax)/2, ymid = (ymin + ymax)/2;
// 	mandelbrot_window sub_windows[] = {
// 		{.xmin = xmin, .xmax = xmid, .ymin = ymin, .ymax = ymid},
// 		{.xmin = xmid, .xmax = xmax, .ymin = ymin, .ymax = ymid},
// 		{.xmin = xmin, .xmax = xmid, .ymin = ymid, .ymax = ymax},
// 		{.xmin = xmid, .xmax = xmax, .ymin = ymid, .ymax = ymax},
// 	};
	
// 	if((xmax - xmin) <= 512 || (ymax - ymin) <= 512){
// 		// TODO Implement thread local linear allocator?
// 		mandelbrot_window* cursor = malloc(sizeof(sub_windows));
// 		memcpy(cursor, sub_windows, sizeof(sub_windows));
		
// 		tina_tasks_enqueue(TASKS, (tina_task[]){
// 			{.func = mandelbrot_render, .data = cursor + 0},
// 			{.func = mandelbrot_render, .data = cursor + 1},
// 			{.func = mandelbrot_render, .data = cursor + 2},
// 			{.func = mandelbrot_render, .data = cursor + 3},
// 		}, 4, ctx->group);
// 		tina_tasks_wait(TASKS, ctx->task, ctx->group, TASK_GROUP_MAX - 4);
// 	} else {
// 		mandelbrot_subdiv(ctx, sub_windows[0]);
// 		mandelbrot_subdiv(ctx, sub_windows[1]);
// 		mandelbrot_subdiv(ctx, sub_windows[2]);
// 		mandelbrot_subdiv(ctx, sub_windows[3]);
// 	}
// }

// static void mandelbrot_task(tina_task* task){
// 	mandelbrot_window* win = task->data;
// 	worker_context* wctx = task->thread_data;
	
// 	tina_group group; tina_group_init(&group);
// 	mandelbrot_subdiv(&(mandelbrot_task_context){
// 		.task = task,
// 		.group = &group,
// 	}, *win);
	
// 	// Wait for remaining tasks to finish.
// 	tina_tasks_wait(TASKS, task, &group, 0);
// }

sg_image texture;

typedef struct {} display_task_ctx;

static void display_task(tina_task* task){
	display_task_ctx* ctx = task->data;
	
	_sapp_glx_make_current();
	int w = sapp_width(), h = sapp_height();
	sg_pass_action action = {
		.colors[0] = {.action = SG_ACTION_CLEAR, .val = {1, 0, 1}},
	};
	sg_begin_default_pass(&action, w, h);
	
	sgl_defaults();
	sgl_enable_texture();
	
	sgl_texture(texture);
	sgl_begin_triangle_strip();
		sgl_v2f_t2f(-0.5, -0.5, 0, 0);
		sgl_v2f_t2f( 0.5, -0.5, 1, 0);
		sgl_v2f_t2f(-0.5,  0.5, 0, 1);
		sgl_v2f_t2f( 0.5,  0.5, 1, 1);
	sgl_end();
	
	sgl_draw();
	sg_end_pass();
	sg_commit();
}

static void app_display(void){
	tina_group group; tina_group_init(&group);
	tina_tasks_enqueue(TASKS, &(tina_task){.func = display_task, .data = NULL}, 1, &group);
	tina_tasks_wait_sleep(TASKS, &group, 0);
}

static void app_event(const sapp_event *event){
	switch(event->type){
		case SAPP_EVENTTYPE_KEY_UP: {
			if(event->key_code == SAPP_KEYCODE_ESCAPE) sapp_request_quit();
		} break;
		
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

static void app_init(void){
	puts("Sokol-App init.");
	
	puts("Creating TASKS.");
	size_t task_count = 1024, coroutine_count = 128, stack_size = 64*1024;
	void* buffer = malloc(tina_tasks_size(task_count, coroutine_count, stack_size));
	TASKS = tina_tasks_init(buffer, task_count, coroutine_count, stack_size);
	
	puts("Creating WORKERS.");
	for(int i = 0; i < WORKER_COUNT; i++){
		worker_context* worker = WORKERS + i;
		(*worker) = (worker_context){};
		thrd_create(&worker->thread, worker_body, worker);
	}
	
	puts("Init Sokol-GFX.");
	sg_desc gfx_desc = {};
	sg_setup(&gfx_desc);
	assert(sg_isvalid());
	
	puts("Init Sokol-GL.");
	sgl_desc_t gl_desc = {};
	sgl_setup(&gl_desc);
	
	uint8_t pixels[4*256*256];
	mandelbrot_render(pixels);
	
	texture = sg_make_image(&(sg_image_desc){
		.width = TEXTURE_SIZE, .height = TEXTURE_SIZE,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.min_filter = SG_FILTER_NEAREST,
		.mag_filter = SG_FILTER_NEAREST,
		.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
		.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
		.content.subimage[0][0] = {.ptr = pixels, .size = sizeof(pixels)},
	});
	
	
	puts("Starting root task.");
	tina_tasks_enqueue(TASKS, (tina_task[]){
		// {.func = mandelbrot_task, .data = &(mandelbrot_window){.xmin = 0, .xmax = W, .ymin = 0, .ymax = H}},
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
