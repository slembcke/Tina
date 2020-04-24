#include <stdlib.h>
#include <stdio.h>
#include <tgmath.h>
#include <complex.h>

#include "tinycthread.h"

#define SOKOL_IMPL
#define SOKOL_GL_IMPL
#define SOKOL_GLCORE33
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_gl.h"

#define TINA_IMPLEMENTATION
#include "../tina.h"

#define TINA_JOBS_IMPLEMENTATION
#include "../tina_jobs.h"

#if defined(__unix__)
#include <unistd.h>
static unsigned GET_CPU_COUNT(void){return sysconf(_SC_NPROCESSORS_ONLN);}
#elif defined(__WINNT__)
#error TODO
SYSTEM_INFO sysinfo;
GetSystemInfo(&sysinfo);
int numCPU = sysinfo.dwNumberOfProcessors;
#else
#error TODO Unhandled/unknown system type.
#endif

#define MAX_JOBS 1024
tina_scheduler* SCHED;
tina_group JOB_GOVERNOR;

enum {
	QUEUE_LO_PRIORITY,
	QUEUE_MED_PRIORITY,
	QUEUE_HI_PRIORITY,
	QUEUE_GL,
	_QUEUE_COUNT,
};

typedef struct {
	thrd_t thread;
} worker_context;

#define MAX_WORKERS 16
static unsigned WORKER_COUNT = MAX_WORKERS;
worker_context WORKERS[MAX_WORKERS];

static int worker_body(void* data){
	worker_context* ctx = data;
	tina_scheduler_run(SCHED, QUEUE_HI_PRIORITY, false, ctx);
	return 0;
}

#define TEXTURE_SIZE 256
#define TEXTURE_CACHE_SIZE 1024

typedef struct {long double x, y;} DriftVec2;
typedef struct {long double a, b, c, d, x, y;} DriftAffine;

static const DriftAffine DRIFT_AFFINE_ZERO = {0, 0, 0, 0, 0, 0};
static const DriftAffine DRIFT_AFFINE_IDENTITY = {1, 0, 0, 1, 0, 0};

static inline DriftAffine DriftAffineMakeTranspose(long double a, long double c, long double x, long double b, long double d, long double y){
	return (DriftAffine){a, b, c, d, x, y};
}

static inline DriftAffine DriftAffineMult(DriftAffine m1, DriftAffine m2){
  return DriftAffineMakeTranspose(
    m1.a*m2.a + m1.c*m2.b, m1.a*m2.c + m1.c*m2.d, m1.a*m2.x + m1.c*m2.y + m1.x,
    m1.b*m2.a + m1.d*m2.b, m1.b*m2.c + m1.d*m2.d, m1.b*m2.x + m1.d*m2.y + m1.y
  );
}

static inline DriftAffine DriftAffineInverse(DriftAffine m){
  long double inv_det = 1/(m.a*m.d - m.c*m.b);
  return DriftAffineMakeTranspose(
     m.d*inv_det, -m.c*inv_det, (m.c*m.y - m.d*m.x)*inv_det,
    -m.b*inv_det,  m.a*inv_det, (m.b*m.x - m.a*m.y)*inv_det
  );
}

static inline DriftAffine DriftAffineOrtho(const long double l, const long double r, const long double b, const long double t){
	long double sx = 2/(r - l);
	long double sy = 2/(t - b);
	long double tx = -(r + l)/(r - l);
	long double ty = -(t + b)/(t - b);
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

uint64_t TIMESTAMP;

typedef struct tile_node tile_node;
struct tile_node {
	uint64_t timestamp;
	bool requested;
	sg_image texture;
	tile_node* children;
};

static tile_node TREE_ROOT;

typedef struct {
	unsigned queue_idx;
	DriftAffine matrix;
	tile_node* node;
} generate_tile_ctx;

typedef struct {
	unsigned xmin, xmax;
	unsigned ymin, ymax;
} mandelbrot_window;

long double hermite(long double x){return x*x*(3 - 2*x);}

long double addend_cosnorm(complex z){return creal(z/cabs(z));}

long double addend_triangle(complex z, complex c){
	long double min = fabs(cabs(z*z) - cabs(c));
	long double max = cabs(z*z) + cabs(c);
	return (cabs(z*z + c) - min)/(max - min);
}

size_t TEXTURE_CURSOR;
tile_node* TEXTURE_NODE[TEXTURE_CACHE_SIZE];
sg_image TEXTURE_CACHE[TEXTURE_CACHE_SIZE];

typedef struct {
	long double complex* restrict coords;
	float* restrict r_samples;
	float* restrict g_samples;
	float* restrict b_samples;
} render_scanline_ctx;

#define SAMPLE_BATCH_COUNT 1024

static void render_samples_job(tina_job* job, void* user_data, void** thread_data){
	const unsigned maxi = 32*1024;
	const long double bailout = 256;
	
	const render_scanline_ctx* const ctx = user_data;
	for(unsigned idx = 0; idx < SAMPLE_BATCH_COUNT; idx++){
		long double r = 0, g = 0, b = 0;
		long double complex c = ctx->coords[idx];
		long double complex z = c;
		long double complex dz = 1;
		
		// long double min = INFINITY;
		// long double sum = 0;
		
		unsigned i = 0;
		while(cabs(z) <= bailout && i < maxi){
			dz *= 2*z;
			if(cabs(dz) < 0x1p-16){
				i = maxi;
				break;
			}
			
			// if(fabs(creal(-0.75 - z)) < 0.01){
			// 	i = maxi;
			// 	break;
			// }
			
			// sum += addend_triangle(z, c);
			
			z = z*z + c;
			i++;
		}
		
		if(i < maxi){
			long double rem = 1 + log2(log2(bailout)) - log2(log2(cabs(z)));
			long double n = (i - 1) + rem;
			// r += 1 - exp(-1e-2*n);
			float phase = 5*log2(n);
			r = 0.5 + 0.5*cos(phase + 0*M_PI/3);
			g = 0.5 + 0.5*cos(phase + 2*M_PI/3);
			b = 0.5 + 0.5*cos(phase + 4*M_PI/3);
			// r += n;
			// g += fmod(n, 1);
			
			// long double alpha = hermite(rem);
			// r += (sum + alpha*addend_triangle(z, c))/(i + alpha);
			// g = b = r;
		}
		
		ctx->r_samples[idx] = r;
		ctx->g_samples[idx] = g;
		ctx->b_samples[idx] = b;
	}
}

static void generate_tile_job(tina_job* job, void* user_data, void** thread_data){
	generate_tile_ctx *ctx = user_data;
	
	const unsigned multisample_count = 4;
	const size_t sample_count = multisample_count*TEXTURE_SIZE*TEXTURE_SIZE;
	const size_t batch_count = sample_count/SAMPLE_BATCH_COUNT;
	const size_t alloc_size = (0
		+ batch_count*sizeof(render_scanline_ctx)
		+ sample_count*sizeof(long double complex)
		+ 3*sample_count*sizeof(float)
	);
	
	// TODO Perfect use for a tagged heap?
	void* buffer = malloc(alloc_size);
	void* cursor = buffer;
	
	render_scanline_ctx* render_contexts = cursor;
	cursor += batch_count*sizeof(render_scanline_ctx);
	long double complex* coords = cursor;
	cursor += sample_count*sizeof(long double complex);
	float* r_samples = cursor;
	cursor += sample_count*sizeof(float);
	float* g_samples = cursor;
	cursor += sample_count*sizeof(float);
	float* b_samples = cursor;
	
	tina_group tile_governor; tina_group_init(&tile_governor);
	
	size_t sample_cursor = 0, batch_cursor = 0;
	for(unsigned y = 0; y < TEXTURE_SIZE; y++){
		for(unsigned x = 0; x < TEXTURE_SIZE; x++){
			for(unsigned sample = 0; sample < multisample_count; sample++){
				uint32_t ssx = ((uint32_t)x << 16) + (uint16_t)(49472*sample);
				uint32_t ssy = ((uint32_t)y << 16) + (uint16_t)(37345*sample);
				DriftVec2 p = DriftAffinePoint(ctx->matrix, (DriftVec2){
					2*((long double)ssx/(long double)((uint32_t)TEXTURE_SIZE << 16)) - 1,
					2*((long double)ssy/(long double)((uint32_t)TEXTURE_SIZE << 16)) - 1,
				});
				coords[sample_cursor] = p.x + p.y*I;
				
				if((++sample_cursor & (SAMPLE_BATCH_COUNT - 1)) == 0){
					// Check if this tile is already stale and bailout.
					if(ctx->node->timestamp + 16 < TIMESTAMP){
						ctx->node->requested = false;
						tina_job_wait(job, &tile_governor, 0);
						goto cleanup;
					}
					
					render_scanline_ctx* rctx = &render_contexts[batch_cursor];
					(*rctx) = (render_scanline_ctx){
						.coords = coords + batch_cursor*SAMPLE_BATCH_COUNT,
						.r_samples = r_samples + batch_cursor*SAMPLE_BATCH_COUNT,
						.g_samples = g_samples + batch_cursor*SAMPLE_BATCH_COUNT,
						.b_samples = b_samples + batch_cursor*SAMPLE_BATCH_COUNT,
					};
					
					tina_job_wait(job, &tile_governor, 4);
					tina_scheduler_enqueue(SCHED, "RenderSamples", render_samples_job, rctx, ctx->queue_idx, &tile_governor);
					
					batch_cursor++;
				}
			}
		}
	}
	tina_job_wait(job, &tile_governor, 0);
	
	// Resolve samples.
	uint8_t* pixels = malloc(4*TEXTURE_SIZE*TEXTURE_SIZE);
	float r = 0, g = 0, b = 0;
	for(size_t src_idx = 0, dst_idx = 0; src_idx < sample_count;){
		r += r_samples[src_idx];
		g += g_samples[src_idx];
		b += b_samples[src_idx];
		
		if((++src_idx & (multisample_count - 1)) == 0){
			float dither = ((dst_idx/4*193 + dst_idx/1024*146) & 0xFF)/65536.0;
			pixels[dst_idx++] = 255*fmax(0, fmin(r/multisample_count + dither, 1));
			pixels[dst_idx++] = 255*fmax(0, fmin(g/multisample_count + dither, 1));
			pixels[dst_idx++] = 255*fmax(0, fmin(b/multisample_count + dither, 1));
			pixels[dst_idx++] = 0;
			r = g = b = 0;
		}
	}
	
	tina_job_switch_queue(job, QUEUE_GL);
	size_t texture_cursor = TEXTURE_CURSOR;
	while(TEXTURE_NODE[texture_cursor] && TEXTURE_NODE[texture_cursor]->timestamp == TIMESTAMP){
		texture_cursor = (texture_cursor + 1) & (TEXTURE_CACHE_SIZE - 1);
	}
	TEXTURE_CURSOR = (texture_cursor + 1) & (TEXTURE_CACHE_SIZE - 1);
	
	if(TEXTURE_NODE[texture_cursor]) TEXTURE_NODE[texture_cursor]->texture.id = 0;
	
	TEXTURE_NODE[texture_cursor] = ctx->node;
	ctx->node->texture = TEXTURE_CACHE[texture_cursor];
	sg_update_image(ctx->node->texture, &(sg_image_content){.subimage[0][0] = {.ptr = pixels}});
	ctx->node->requested = false;
	free(pixels);
	
	cleanup:
	free(ctx);
	free(buffer);
}

#define VIEW_RESET (DriftAffine){0.75, 0, 0, 0.75, 0.5, 0}
static DriftAffine proj_matrix = {1, 0, 0, 1, 0, 0};
static DriftAffine view_matrix = VIEW_RESET;

static DriftAffine pixel_to_world_matrix(void){
	DriftAffine pixel_to_clip = DriftAffineOrtho(0, sapp_width(), sapp_height(), 0);
	DriftAffine vp_inv_matrix = DriftAffineInverse(DriftAffineMult(proj_matrix, view_matrix));
	return DriftAffineMult(vp_inv_matrix, pixel_to_clip);
}

typedef struct {} display_job_ctx;
static DriftVec2 mouse_pos;

static DriftAffine sub_matrix(DriftAffine m, long double x, long double y){
	return (DriftAffine){0.5*m.a, 0.5*m.b, 0.5*m.c, 0.5*m.d, m.x + x*m.a + y*m.c, m.y + x*m.b + y*m.d};
}

static bool frustum_cull(const DriftAffine mvp){
	// Clip space center and extents.
	DriftVec2 c = {mvp.x, mvp.y};
	long double ex = fabs(mvp.a) + fabs(mvp.c);
	long double ey = fabs(mvp.b) + fabs(mvp.d);
	
	return ((fabs(c.x) - ex < 1) && (fabs(c.y) - ey < 1));
}

static void draw_tile(DriftAffine mv_matrix, sg_image texture){
	sgl_matrix_mode_modelview();
	sgl_load_matrix(DriftAffineToGPU(mv_matrix).m);
	
	sgl_texture(texture);
	sgl_begin_triangle_strip();
		sgl_v2f_t2f(-1, -1, -1, -1);
		sgl_v2f_t2f( 1, -1,  1, -1);
		sgl_v2f_t2f(-1,  1, -1,  1);
		sgl_v2f_t2f( 1,  1,  1,  1);
	sgl_end();
}

static bool visit_tile(tile_node* node, DriftAffine matrix){
	DriftAffine mv_matrix = DriftAffineMult(view_matrix, matrix);
	if(!frustum_cull(DriftAffineMult(proj_matrix, mv_matrix))) return true;
	
	node->timestamp = TIMESTAMP;
	DriftAffine ddm = DriftAffineMult(DriftAffineInverse(pixel_to_world_matrix()), matrix);
	float scale = 2*sqrt(ddm.a*ddm.a + ddm.b*ddm.b) + sqrt(ddm.c*ddm.c + ddm.d*ddm.d);
	
	if(node->texture.id){
		if(scale > TEXTURE_SIZE){
			if(!node->children) node->children = calloc(4, sizeof(*node->children));
			
			draw_tile(mv_matrix, node->texture);
			visit_tile(node->children + 0, sub_matrix(matrix, -0.5, -0.5));
			visit_tile(node->children + 1, sub_matrix(matrix,  0.5, -0.5));
			visit_tile(node->children + 2, sub_matrix(matrix, -0.5,  0.5));
			visit_tile(node->children + 3, sub_matrix(matrix,  0.5,  0.5));
			return true;
		}
	} else if(!node->requested && JOB_GOVERNOR._count < MAX_WORKERS){
		node->requested = true;
		int queue_idx = fmax(QUEUE_LO_PRIORITY, fmin(log2(scale/512) - 1, QUEUE_HI_PRIORITY));
		
		generate_tile_ctx* generate_ctx = malloc(sizeof(*generate_ctx));
		(*generate_ctx) = (generate_tile_ctx){
			.queue_idx = queue_idx,
			.matrix = matrix,
			.node = node,
		};
		
		tina_scheduler_enqueue(SCHED, "GenTiles", generate_tile_job, generate_ctx, queue_idx, &JOB_GOVERNOR);
	}
	
	return false;
}

static void app_display(void){
	// Run jobs to load textures.
	tina_scheduler_run(SCHED, QUEUE_GL, true, NULL);
	TIMESTAMP++;
	
	int w = sapp_width(), h = sapp_height();
	sg_pass_action action = {.colors[0] = {.action = SG_ACTION_CLEAR, .val = {1, 1, 1}}};
	sg_begin_default_pass(&action, w, h);
	
	sgl_defaults();
	sgl_enable_texture();
	
	float pw = fmax(1, (float)w/(float)h);
	float ph = fmax(1, (float)h/(float)w);
	proj_matrix = DriftAffineOrtho(-pw, pw, -ph, ph);
	
	sgl_matrix_mode_projection();
	sgl_load_matrix(DriftAffineToGPU(proj_matrix).m);
	
	sgl_matrix_mode_texture();
	sgl_load_matrix((float[]){
		0.5, 0.0, 0, 0,
		0.0, 0.5, 0, 0,
		0.0, 0.0, 1, 0,
		0.5, 0.5, 0, 1,
	});
	
	visit_tile(&TREE_ROOT, (DriftAffine){16, 0, 0, 16, 0, 0});
	
	sgl_draw();
	sg_end_pass();
	sg_commit();
}

static bool mouse_drag;

static void app_event(const sapp_event *event){
	switch(event->type){
		case SAPP_EVENTTYPE_KEY_UP: {
			if(event->key_code == SAPP_KEYCODE_ESCAPE) sapp_request_quit();
			if(event->key_code == SAPP_KEYCODE_SPACE) view_matrix = VIEW_RESET;
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
			long double scale = exp(-0.5*event->scroll_y);
			DriftVec2 mpos = DriftAffinePoint(pixel_to_world_matrix(), mouse_pos);
			DriftAffine t = {scale, 0, 0, scale, mpos.x*(1 - scale), mpos.y*(1 - scale)};
			view_matrix = DriftAffineMult(view_matrix, t);
		} break;
		
		default: break;
	}
}

static void app_init(void){
	puts("Sokol-App init.");
	
	puts("Creating SCHED.");
	SCHED = tina_scheduler_new(MAX_JOBS, _QUEUE_COUNT, 128, 64*1024);
	tina_scheduler_queue_priority(SCHED, QUEUE_HI_PRIORITY, QUEUE_MED_PRIORITY);
	tina_scheduler_queue_priority(SCHED, QUEUE_MED_PRIORITY, QUEUE_LO_PRIORITY);
	tina_group_init(&JOB_GOVERNOR);
	
	WORKER_COUNT = GET_CPU_COUNT();
	printf("%d CPUs detected.\n", WORKER_COUNT);
	
	puts("Creating WORKERS.");
	for(int i = 0; i < WORKER_COUNT; i++){
		worker_context* worker = WORKERS + i;
		(*worker) = (worker_context){};
		thrd_create(&worker->thread, worker_body, worker);
	}
	
	puts("Init Sokol-GFX.");
	sg_desc gfx_desc = {.image_pool_size = TEXTURE_CACHE_SIZE + 1};
	sg_setup(&gfx_desc);
	assert(sg_isvalid());
	
	for(unsigned i = 0; i < 1024; i++){
		TEXTURE_CACHE[i] = sg_make_image(&(sg_image_desc){
			.width = TEXTURE_SIZE, .height = TEXTURE_SIZE,
			.pixel_format = SG_PIXELFORMAT_RGBA8,
			.min_filter = SG_FILTER_LINEAR,
			.mag_filter = SG_FILTER_LINEAR,
			.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
			.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
			.usage = SG_USAGE_DYNAMIC,
		});
	}
	
	puts("Init Sokol-GL.");
	sgl_desc_t gl_desc = {};
	sgl_setup(&gl_desc);
	
	puts("Init complete.");
}

static void app_cleanup(void){
	puts("Sokol-App cleanup.");
	// tina_job_wait_sleep(SCHED, &group, 0);
	
	puts("WORKERS shutdown.");
	TIMESTAMP += 1000;
	tina_scheduler_pause(SCHED);
	for(int i = 0; i < WORKER_COUNT; i++) thrd_join(WORKERS[i].thread, NULL);
	
	puts ("Destroing SCHED");
	tina_scheduler_destroy(SCHED);
	free(SCHED);
	
	puts("Sokol-GFX shutdown.");
	// sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
	return (sapp_desc){
		.init_cb = app_init,
		.frame_cb = app_display,
		.event_cb = app_event,
		.cleanup_cb = app_cleanup,
		.width = 2048,
		.height = 2048,
		.window_title = "Mandelbrot",
	};
}
