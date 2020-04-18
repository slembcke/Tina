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

typedef struct {float x, y;} DriftVec2;
typedef struct {float a, b, c, d, x, y;} DriftAffine;

static const DriftAffine DRIFT_AFFINE_ZERO = {0, 0, 0, 0, 0, 0};
static const DriftAffine DRIFT_AFFINE_IDENTITY = {1, 0, 0, 1, 0, 0};

static inline DriftAffine DriftAffineMakeTranspose(float a, float c, float x, float b, float d, float y){
	return (DriftAffine){a, b, c, d, x, y};
}

static inline DriftAffine DriftAffineMult(DriftAffine m1, DriftAffine m2){
  return DriftAffineMakeTranspose(
    m1.a*m2.a + m1.c*m2.b, m1.a*m2.c + m1.c*m2.d, m1.a*m2.x + m1.c*m2.y + m1.x,
    m1.b*m2.a + m1.d*m2.b, m1.b*m2.c + m1.d*m2.d, m1.b*m2.x + m1.d*m2.y + m1.y
  );
}

static inline DriftAffine DriftAffineInverse(DriftAffine m){
  float inv_det = 1/(m.a*m.d - m.c*m.b);
  return DriftAffineMakeTranspose(
     m.d*inv_det, -m.c*inv_det, (m.c*m.y - m.d*m.x)*inv_det,
    -m.b*inv_det,  m.a*inv_det, (m.b*m.x - m.a*m.y)*inv_det
  );
}

static inline DriftAffine DriftAffineOrtho(const float l, const float r, const float b, const float t){
	float sx = 2/(r - l);
	float sy = 2/(t - b);
	float tx = -(r + l)/(r - l);
	float ty = -(t + b)/(t - b);
	return DriftAffineMakeTranspose(
		sx,  0, tx,
		 0, sy, ty
	);
}

static inline DriftVec2 DriftAffinePoint(DriftAffine t, DriftVec2 p){
	return (DriftVec2){t.a*p.x + t.c*p.y + t.x, t.b*p.x + t.d*p.y + t.y};
}

static inline DriftVec2 DriftAffineVec(DriftAffine t, DriftVec2 p){
	return (DriftVec2){t.a*p.x + t.c*p.y, t.b*p.x + t.d*p.y};
}

typedef struct {float m[16];} DriftGPUMatrix;

static inline DriftGPUMatrix DriftAffineToGPU(DriftAffine m){
	return (DriftGPUMatrix){.m = {m.a, m.b, 0, 0, m.c, m.d, 0, 0, 0, 0, 1, 0, m.x, m.y, 0, 1}};
}

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
				uint32_t ssx = ((uint32_t)px << 16) + (uint16_t)(49472*sample);
				uint32_t ssy = ((uint32_t)py << 16) + (uint16_t)(37345*sample);
				double x0 = 4*((double)ssx/(double)((uint32_t)TEXTURE_SIZE << 16)) - 3;
				double y0 = 4*((double)ssy/(double)((uint32_t)TEXTURE_SIZE << 16)) - 2;
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


typedef struct tile_node tile_node;
struct tile_node {
	DriftAffine matrix;
	sg_image texture;
	
	tile_node* quadrants[4];
};

static tile_node* TREE_ROOT;

static DriftAffine proj_matrix = {1, 0, 0, 1, 0, 0};
static DriftAffine view_matrix = {1, 0, 0, 1, 0, 0};

static DriftAffine pixel_to_world_matrix(void){
	DriftAffine pixel_to_clip = DriftAffineOrtho(0, sapp_width(), sapp_height(), 0);
	DriftAffine vp_inv_matrix = DriftAffineInverse(DriftAffineMult(proj_matrix, view_matrix));
	return DriftAffineMult(vp_inv_matrix, pixel_to_clip);
}

typedef struct {} display_task_ctx;
static DriftVec2 mouse_pos;


static tile_node* visit_tiles(tile_node* node){
	if(node){
		// TODO rearrange?
		DriftAffine ddm = DriftAffineMult(DriftAffineInverse(pixel_to_world_matrix()), node->matrix);
		double scale = sqrt(ddm.a*ddm.a + ddm.b*ddm.b) + sqrt(ddm.c*ddm.c + ddm.d*ddm.d);
		// if(scale < TEXTURE_SIZE){
			sgl_matrix_mode_modelview();
			DriftAffine mv_matrix = DriftAffineMult(view_matrix, node->matrix);
			sgl_load_matrix(DriftAffineToGPU(mv_matrix).m);
			
			sgl_texture(node->texture);
			sgl_begin_triangle_strip();
				sgl_v2f_t2f(-1, -1, 0, 0);
				sgl_v2f_t2f( 1, -1, 1, 0);
				sgl_v2f_t2f(-1,  1, 0, 1);
				sgl_v2f_t2f( 1,  1, 1, 1);
			sgl_end();
		// } else {
		// 	node->quadrants[0] = visit_tiles(node->quadrants[0]);
		// }
	} else {
		puts("render");
		uint8_t* pixels = malloc(4*256*256);
		mandelbrot_render(pixels);
		
		sg_image texture = sg_make_image(&(sg_image_desc){
			.width = TEXTURE_SIZE, .height = TEXTURE_SIZE,
			.pixel_format = SG_PIXELFORMAT_RGBA8,
			.min_filter = SG_FILTER_LINEAR,
			.mag_filter = SG_FILTER_LINEAR,
			.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
			.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
			.content.subimage[0][0] = {.ptr = pixels, .size = sizeof(pixels)},
		});
		free(pixels);
		
		node = malloc(sizeof(*node));
		(*node) = (tile_node){
			.matrix = DRIFT_AFFINE_IDENTITY,
			.texture = texture,
		};
	}
	
	return node;
}

static void display_task(tina_task* task){
	display_task_ctx* ctx = task->data;
	
	_sapp_glx_make_current();
	int w = sapp_width(), h = sapp_height();
	sg_pass_action action = {.colors[0] = {.action = SG_ACTION_CLEAR, .val = {1, 1, 1}}};
	sg_begin_default_pass(&action, w, h);
	
	sgl_defaults();
	sgl_enable_texture();
	
	sgl_matrix_mode_projection();
	sgl_load_matrix(DriftAffineToGPU(proj_matrix).m);
	
	TREE_ROOT = visit_tiles(TREE_ROOT);
	
	sgl_draw();
	sg_end_pass();
	sg_commit();
}

static void app_display(void){
	tina_group group; tina_group_init(&group);
	tina_tasks_enqueue(TASKS, &(tina_task){.func = display_task, .data = NULL}, 1, &group);
	tina_tasks_wait_sleep(TASKS, &group, 0);
}

static bool mouse_drag;

static void app_event(const sapp_event *event){
	switch(event->type){
		case SAPP_EVENTTYPE_KEY_UP: {
			if(event->key_code == SAPP_KEYCODE_ESCAPE) sapp_request_quit();
			if(event->key_code == SAPP_KEYCODE_SPACE) view_matrix = DRIFT_AFFINE_IDENTITY;
		} break;
		
		case SAPP_EVENTTYPE_MOUSE_MOVE: {
			DriftVec2 new_pos = (DriftVec2){event->mouse_x, event->mouse_y};
			if(mouse_drag){
				DriftVec2 mouse_delta = {new_pos.x - mouse_pos.x, new_pos.y - mouse_pos.y};
				DriftVec2 delta = DriftAffineVec(pixel_to_world_matrix(), mouse_delta);
				DriftAffine t = {1, 0, 0, 1, delta.x, delta.y};
				view_matrix = DriftAffineMult(view_matrix, t);
			}
			mouse_pos = new_pos;
		}; break;
		
		case SAPP_EVENTTYPE_MOUSE_DOWN: {
			if(event->mouse_button == SAPP_MOUSEBUTTON_LEFT) mouse_drag = true;
		} break;
		case SAPP_EVENTTYPE_MOUSE_UP: {
			if(event->mouse_button == SAPP_MOUSEBUTTON_LEFT) mouse_drag = false;
		} break;
		
		case SAPP_EVENTTYPE_MOUSE_SCROLL: {
			float scale = exp(0.1*event->scroll_y);
			DriftVec2 mpos = DriftAffinePoint(pixel_to_world_matrix(), mouse_pos);
			DriftAffine t = {scale, 0, 0, scale, mpos.x*(1 - scale), mpos.y*(1 - scale)};
			view_matrix = DriftAffineMult(view_matrix, t);
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
		.window_title = "Mandelbrot",
	};
}
